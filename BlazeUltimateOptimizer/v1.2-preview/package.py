from pathlib import Path
import hashlib
import zipfile

root=Path(__file__).resolve().parent
release=root/"release"
release.mkdir(exist_ok=True)
app=root
exe_names=["BlazeUltimateOptimizer-x64.exe","BlazeUltimateOptimizer-arm64.exe"]
names=exe_names+["BlazeUltimateOptimizer.c","build.py","resources.py","verify.py",
                 "README.md","BlazeUltimateOptimizer.ico","BlazeSystems-LogoV2-source-crop.png",
                 "SAFETY-CHECKS-v1.1.md"]
out=release/"BlazeUltimateOptimizer-v1.2-preview-complete.zip"
with zipfile.ZipFile(out,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
    for name in names:
        path=root/name
        if not path.is_file():
            raise FileNotFoundError(path)
        archive.write(path,arcname=name)
import shutil
for name in exe_names:
    shutil.copy2(root/name,release/name)
checksum_files=[release/name for name in exe_names]+[out]
(release/"SHA256SUMS.txt").write_text(
    "".join(hashlib.sha256(path.read_bytes()).hexdigest()+"  "+path.name+"\n" for path in checksum_files),
    encoding="utf-8")
print("Prepared verified build package:",out)
