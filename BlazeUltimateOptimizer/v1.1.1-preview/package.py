from pathlib import Path
import hashlib
import sys
import zipfile

app=Path(sys.argv[1])
out=Path(sys.argv[2])
out.parent.mkdir(parents=True,exist_ok=True)
names=[
    'BlazeUltimateOptimizer-x64.exe',
    'BlazeUltimateOptimizer-arm64.exe',
    'BlazeUltimateOptimizer.c',
    'build.py',
    'resources.py',
    'verify.py',
    'BlazeUltimateOptimizer.ico',
    'BlazeSystems-LogoV2-source-crop.png',
    'Blaze Systems Brand Identity Board(1).png',
    'HOTFIX-README.md',
    'SAFETY-CHECKS-v1.1.md',
]
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name in names:
        assert (app/name).is_file(),name
        z.write(app/name,name)
checks=out.parent/'SHA256-v1.1.1.txt'
with checks.open('w',encoding='utf-8') as f:
    for file in [app/'BlazeUltimateOptimizer-x64.exe',
                 app/'BlazeUltimateOptimizer-arm64.exe',out]:
        f.write(hashlib.sha256(file.read_bytes()).hexdigest()+'  '+file.name+'\n')
print('Release package',out,'size',out.stat().st_size)
