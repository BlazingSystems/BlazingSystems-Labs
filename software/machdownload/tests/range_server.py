from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
import re, sys
DATA=(bytes(range(256))*32768)
ETAG='"mach-test-v1"'
class H(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path!="/file.bin": self.send_error(404); return
        rng=self.headers.get("Range")
        a,b=0,len(DATA)-1
        if rng:
            m=re.match(r"bytes=(\d+)-(\d*)",rng)
            if not m:self.send_error(416);return
            a=int(m.group(1)); b=int(m.group(2)) if m.group(2) else b
            b=min(b,len(DATA)-1)
            if a>b:self.send_error(416);return
            self.send_response(206); self.send_header("Content-Range",f"bytes {a}-{b}/{len(DATA)}")
        else:self.send_response(200)
        self.send_header("Content-Length",str(b-a+1)); self.send_header("Accept-Ranges","bytes")
        self.send_header("ETag",ETAG); self.end_headers()
        self.wfile.write(DATA[a:b+1])
    def log_message(self,*a):pass
if __name__=="__main__":
    port=int(sys.argv[1]) if len(sys.argv)>1 else 8765
    print(f"http://127.0.0.1:{port}/file.bin",flush=True)
    ThreadingHTTPServer(("127.0.0.1",port),H).serve_forever()
