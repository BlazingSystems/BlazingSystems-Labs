"""Compile exact crop of user's Blaze Systems icon, plus PE version metadata.
No vector redraw or redesign: artwork stays the source image, resampled by Pillow.
Generates Windows resource .res without requiring a Windows SDK / windres.
"""
from pathlib import Path
from PIL import Image
from io import BytesIO
import struct
P=Path(__file__).parent
SOURCE=P/'BlazeSystems-LogoV2-source-crop.png'
# The exact icon crop is retained from the user's approved brand board.
# Keep it in the build package so rebuilding needs no network access.
im=Image.open(SOURCE).convert('RGBA')
if im.size != (268,267):
    raise RuntimeError('Unexpected approved app-icon crop dimensions')
icon=im
sizes=(16,32,48,128,256)
data=[]
for sz in sizes:
  out=BytesIO(); icon.resize((sz,sz),Image.Resampling.LANCZOS).save(out,format='PNG')
  data.append(out.getvalue())
iconfile=BytesIO();iconfile.write(struct.pack('<HHH',0,1,len(sizes)));off=6+16*len(sizes)
for sz,payload in zip(sizes,data):
  iconfile.write(struct.pack('<BBBBHHII',0 if sz==256 else sz,0 if sz==256 else sz,0,0,1,32,len(payload),off));off+=len(payload)
for payload in data:iconfile.write(payload)
(P/'BlazeUltimateOptimizer.ico').write_bytes(iconfile.getvalue())

def pad4(b):return b+b'\0'*((-len(b))%4)
def w(s):return s.encode('utf-16le')+b'\0\0'
def block(key,value=b'',children=(),typ=1,vlen=None):
  content=struct.pack('<HHH',0,0,typ)+w(key)
  content=pad4(content)+value
  content=pad4(content)+b''.join(children)
  if vlen is None:vlen=(len(value)//2 if typ==1 else len(value))
  return struct.pack('<HHH',len(content),vlen,typ)+content[6:]
fixed=struct.pack('<13I',0xFEEF04BD,0x00010000,0x00010001,0x00000000,0x00010001,0x00000000,0x0000003F,0,0,0x00040004,1,0,0)
strings=[('CompanyName','Loumer Cabaluna Manreal'),('ProductName','Blaze Ultimate Optimizer'),('FileDescription','Blaze Ultimate Optimizer - Low Spec Windows Preview'),('LegalCopyright','Copyright (C) 2026 Loumer Cabaluna Manreal'),('FileVersion','1.2.0.0'),('ProductVersion','1.2.0.0'),('OriginalFilename','BlazeUltimateOptimizer.exe'),('InternalName','BlazeUltimateOptimizer')]
stringtable=block('040904B0',children=[block(key,w(value)) for key,value in strings])
stringfile=block('StringFileInfo',children=[stringtable])
var=block('Translation',struct.pack('<HH',1033,1200),typ=0)
varfile=block('VarFileInfo',children=[var])
vers=block('VS_VERSION_INFO',fixed,children=[stringfile,varfile],typ=0)

def resentry(typ,idx,dat):
  h=struct.pack('<II',len(dat),32)+struct.pack('<HHHH',0xFFFF,typ,0xFFFF,idx)+struct.pack('<IHHII',0,0x1030,1033,0,0)
  assert len(h)==32
  return h+pad4(dat)
res=bytearray(resentry(0,0,b''))
for i,payload in enumerate(data,1):res.extend(resentry(3,i,payload))
grp=struct.pack('<HHH',0,1,len(sizes))
for i,(sz,payload) in enumerate(zip(sizes,data),1):
  grp+=struct.pack('<BBBBHHIH',0 if sz==256 else sz,0 if sz==256 else sz,0,0,1,32,len(payload),i)
res.extend(resentry(14,101,grp))
res.extend(resentry(16,1,vers))
(P/'BlazeUltimateOptimizer.res').write_bytes(res)
print('Icon from owner source:',len(iconfile.getvalue()),'bytes. Resource:',len(res),'bytes')