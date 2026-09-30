import hashlib, tempfile, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from machdownload.core import DownloadJob
EXPECTED=hashlib.sha256(bytes(range(256))*32768).hexdigest()
def main():
    d=Path(tempfile.mkdtemp(prefix="machtest-"))
    p=DownloadJob("http://127.0.0.1:8765/file.bin",d,8).run()
    got=hashlib.sha256(p.read_bytes()).hexdigest()
    assert got==EXPECTED,(got,EXPECTED)
    print("PASS segmented download SHA-256",got)
if __name__=="__main__":main()
