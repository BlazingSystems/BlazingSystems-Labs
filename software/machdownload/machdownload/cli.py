import argparse, sys, threading
from pathlib import Path
from .core import DownloadJob
def main():
    ap=argparse.ArgumentParser(description="MachDownload direct HTTP/HTTPS accelerator")
    ap.add_argument("url"); ap.add_argument("-d","--dir",default=str(Path.home()/"Downloads"))
    ap.add_argument("-c","--connections",type=int,default=8)
    a=ap.parse_args()
    def prog(done,total,speed):
        pct=(done*100/total) if total else 0
        print(f"\r{pct:6.2f}%  {done/1048576:.1f} MiB  {speed/1048576:.2f} MiB/s",end="",flush=True)
    try:
        p=DownloadJob(a.url,a.dir,a.connections,progress=prog).run()
        print(f"\nSaved: {p}")
    except KeyboardInterrupt:
        print("\nInterrupted."); return 130
    except Exception as e:
        print(f"\nError: {e}",file=sys.stderr); return 1
    return 0
if __name__=="__main__":raise SystemExit(main())
