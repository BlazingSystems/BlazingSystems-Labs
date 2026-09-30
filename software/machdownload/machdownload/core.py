from __future__ import annotations
import concurrent.futures as cf
import hashlib, json, os, re, ssl, threading, time
from dataclasses import dataclass, asdict
from pathlib import Path
from urllib.parse import urlparse, unquote
from urllib.request import Request, build_opener, HTTPRedirectHandler
from urllib.error import HTTPError, URLError

UA = "MachDownload/0.1 (+direct HTTP downloader)"
CHUNK = 256 * 1024

class DownloadError(Exception): pass
class ResourceChanged(DownloadError): pass
class Cancelled(DownloadError): pass

def _safe_name(s: str) -> str:
    s = re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", s).strip(" .")
    return s[:180] or "download.bin"

def _filename(url, headers):
    cd = headers.get("Content-Disposition", "")
    m = re.search(r"filename\*=UTF-8''([^;]+)", cd, re.I)
    if m: return _safe_name(unquote(m.group(1)))
    m = re.search(r'filename="?([^";]+)"?', cd, re.I)
    if m: return _safe_name(m.group(1))
    return _safe_name(Path(unquote(urlparse(url).path)).name or "download.bin")

def _opener():
    return build_opener(HTTPRedirectHandler())

def probe(url, timeout=20):
    headers={"User-Agent":UA, "Accept-Encoding":"identity"}
    op=_opener()
    req=Request(url, headers={**headers, "Range":"bytes=0-0"}, method="GET")
    try:
        with op.open(req, timeout=timeout) as r:
            h=r.headers
            code=getattr(r, "status", 200)
            cr=h.get("Content-Range","")
            total=None
            m=re.search(r"/(\d+)$", cr)
            if m: total=int(m.group(1))
            elif h.get("Content-Length"): total=int(h["Content-Length"])
            ranges=(code==206 and bool(m)) or h.get("Accept-Ranges","").lower()=="bytes"
            return {
                "final_url": r.geturl(), "size": total, "ranges": ranges,
                "etag": h.get("ETag"), "last_modified": h.get("Last-Modified"),
                "content_type": h.get("Content-Type"), "filename": _filename(r.geturl(),h)
            }
    except HTTPError as e:
        raise DownloadError(f"HTTP {e.code}: {e.reason}") from e
    except URLError as e:
        raise DownloadError(f"Network error: {e.reason}") from e

@dataclass
class Segment:
    start:int
    end:int
    done:int=0

class DownloadJob:
    def __init__(self, url, dest_dir, connections=8, timeout=30, retries=5,
                 progress=None, stop_event=None):
        self.url=url
        self.dest_dir=Path(dest_dir)
        self.connections=max(1,min(int(connections),32))
        self.timeout=timeout; self.retries=retries
        self.progress=progress or (lambda *a, **k: None)
        self.stop_event=stop_event or threading.Event()
        self.lock=threading.Lock()
        self.meta=None; self.parts=[]; self.started=time.time()
        self.downloaded=0

    def _paths(self, name):
        self.dest_dir.mkdir(parents=True, exist_ok=True)
        final=self.dest_dir/name
        state=self.dest_dir/(name+".machstate")
        partdir=self.dest_dir/(".mach-"+hashlib.sha1((name+self.url).encode()).hexdigest()[:10])
        return final,state,partdir

    def _save_state(self, state, data):
        tmp=state.with_suffix(state.suffix+".tmp")
        tmp.write_text(json.dumps(data, indent=2), encoding="utf-8")
        os.replace(tmp,state)

    def _load_state(self,state):
        try: return json.loads(state.read_text(encoding="utf-8"))
        except Exception: return None

    def _identity_ok(self, old, new):
        if old.get("size") and new.get("size") and old["size"]!=new["size"]: return False
        if old.get("etag") and new.get("etag") and old["etag"]!=new["etag"]: return False
        if old.get("last_modified") and new.get("last_modified") and old["last_modified"]!=new["last_modified"]: return False
        return True

    def _report(self):
        elapsed=max(.001,time.time()-self.started)
        self.progress(self.downloaded, self.meta.get("size"), self.downloaded/elapsed)

    def _fetch_segment(self, idx, seg, partdir, state, state_data):
        p=partdir/f"{idx:03d}.part"
        existing=p.stat().st_size if p.exists() else 0
        seg.done=min(existing, seg.end-seg.start+1)
        with self.lock: self.downloaded += seg.done
        pos=seg.start+seg.done
        if pos>seg.end: return
        for attempt in range(self.retries+1):
            if self.stop_event.is_set(): raise Cancelled()
            try:
                req=Request(self.meta["final_url"], headers={
                    "User-Agent":UA, "Accept-Encoding":"identity",
                    "Range":f"bytes={pos}-{seg.end}"
                })
                with _opener().open(req, timeout=self.timeout) as r:
                    if getattr(r,"status",200)!=206:
                        raise DownloadError("Server stopped honoring byte ranges; refusing unsafe segmented resume.")
                    cr=r.headers.get("Content-Range","")
                    if not cr.startswith(f"bytes {pos}-"):
                        raise DownloadError("Server returned an unexpected Content-Range.")
                    with p.open("ab") as f:
                        while True:
                            if self.stop_event.is_set(): raise Cancelled()
                            b=r.read(CHUNK)
                            if not b: break
                            remaining=seg.end-(seg.start+seg.done)+1
                            if len(b)>remaining: b=b[:remaining]
                            f.write(b); seg.done += len(b); pos += len(b)
                            with self.lock:
                                self.downloaded += len(b)
                                state_data["segments"][idx]["done"]=seg.done
                                self._report()
                            if pos>seg.end: break
                    if seg.done != seg.end-seg.start+1:
                        raise DownloadError("Connection ended before segment completed.")
                    return
            except Cancelled: raise
            except Exception:
                if attempt>=self.retries: raise
                time.sleep(min(8, .5*(2**attempt)))
        raise DownloadError("Segment failed.")

    def _single(self, final, state):
        tmp=final.with_suffix(final.suffix+".machpart")
        old=self._load_state(state)
        existing=tmp.stat().st_size if tmp.exists() else 0
        if old and not self._identity_ok(old.get("remote",{}), self.meta):
            raise ResourceChanged("Remote file changed; partial file was preserved.")
        headers={"User-Agent":UA,"Accept-Encoding":"identity"}
        mode="wb"
        if existing and self.meta.get("ranges"):
            headers["Range"]=f"bytes={existing}-"; mode="ab"; self.downloaded=existing
        elif existing:
            tmp.unlink(); existing=0
        req=Request(self.meta["final_url"],headers=headers)
        with _opener().open(req,timeout=self.timeout) as r, tmp.open(mode) as f:
            if existing and getattr(r,"status",200)!=206:
                raise DownloadError("Unsafe resume response; partial file preserved.")
            while True:
                if self.stop_event.is_set(): raise Cancelled()
                b=r.read(CHUNK)
                if not b: break
                f.write(b); self.downloaded+=len(b); self._report()
                self._save_state(state,{"remote":self.meta,"mode":"single"})
        if self.meta.get("size") is not None and tmp.stat().st_size != self.meta["size"]:
            raise DownloadError("Final size mismatch; partial file preserved.")
        os.replace(tmp,final)
        state.unlink(missing_ok=True)
        return final

    def run(self):
        self.meta=probe(self.url,self.timeout)
        final,state,partdir=self._paths(self.meta["filename"])
        if final.exists():
            base=final.stem; suf=final.suffix; n=1
            while final.exists():
                final=self.dest_dir/f"{base} ({n}){suf}"; n+=1
            state=self.dest_dir/(final.name+".machstate")
            partdir=self.dest_dir/(".mach-"+hashlib.sha1((final.name+self.url).encode()).hexdigest()[:10])
        if not self.meta.get("ranges") or not self.meta.get("size") or self.connections==1:
            return self._single(final,state)

        old=self._load_state(state)
        if old and not self._identity_ok(old.get("remote",{}),self.meta):
            raise ResourceChanged("Remote file identity changed. Existing partial segments were preserved.")
        size=self.meta["size"]
        n=min(self.connections,max(1,(size+1024*1024-1)//(1024*1024)))
        step=(size+n-1)//n
        self.parts=[]
        for i in range(n):
            a=i*step; b=min(size-1,(i+1)*step-1)
            if a<=b:self.parts.append(Segment(a,b))
        partdir.mkdir(parents=True,exist_ok=True)
        state_data={"remote":self.meta,"url":self.url,"segments":[asdict(x) for x in self.parts]}
        self._save_state(state,state_data)
        try:
            with cf.ThreadPoolExecutor(max_workers=len(self.parts),thread_name_prefix="mach") as ex:
                fs=[ex.submit(self._fetch_segment,i,s,partdir,state,state_data) for i,s in enumerate(self.parts)]
                for f in cf.as_completed(fs): f.result()
        except Exception:
            self._save_state(state,state_data)
            raise
        tmp=final.with_suffix(final.suffix+".assembling")
        with tmp.open("wb") as out:
            for i,s in enumerate(self.parts):
                p=partdir/f"{i:03d}.part"
                if p.stat().st_size != s.end-s.start+1:
                    raise DownloadError("Segment size mismatch; refusing to assemble.")
                with p.open("rb") as inp:
                    while True:
                        b=inp.read(1024*1024)
                        if not b: break
                        out.write(b)
        if tmp.stat().st_size != size:
            raise DownloadError("Assembled size mismatch.")
        os.replace(tmp,final)
        for p in partdir.glob("*.part"): p.unlink()
        partdir.rmdir(); state.unlink(missing_ok=True)
        return final
