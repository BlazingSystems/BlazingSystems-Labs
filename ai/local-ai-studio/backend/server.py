import json, os, sys, threading, webbrowser, base64, io, time, random, subprocess, shutil
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; WEB=ROOT/'web'; OUT=ROOT/'output'; MODELS=ROOT/'models'; CFG=ROOT/'presets'/'settings.json'
OUT.mkdir(exist_ok=True); CFG.parent.mkdir(exist_ok=True)
def cfg():
 d={'device':'auto','low_memory':True,'sd_model':str(MODELS/'diffusers'/'sd15'),'piper_exe':str(ROOT/'engines'/'piper'/'piper.exe'),'piper_voice':''}
 if CFG.exists():
  try:d.update(json.loads(CFG.read_text()))
  except:pass
 return d
def dataimg(s):
 from PIL import Image
 if not s:return None
 return Image.open(io.BytesIO(base64.b64decode(s.split(',',1)[-1]))).convert('RGB')
def saveimg(im,prefix):
 p=OUT/f'{prefix}_{time.strftime("%Y%m%d_%H%M%S")}_{random.randint(100,999)}.png'; im.save(p); return p
def sd_pipe(kind='txt'):
 import torch
 from diffusers import StableDiffusionPipeline, StableDiffusionImg2ImgPipeline
 c=cfg(); m=c['sd_model']; kw={'torch_dtype':torch.float32,'local_files_only':True,'safety_checker':None,'requires_safety_checker':False}
 P=StableDiffusionPipeline if kind=='txt' else StableDiffusionImg2ImgPipeline
 pipe=P.from_pretrained(m,**kw)
 if c['device']=='cuda' and torch.cuda.is_available(): pipe=pipe.to('cuda')
 else: pipe=pipe.to('cpu')
 try: pipe.enable_attention_slicing('max'); pipe.enable_vae_slicing()
 except: pass
 return pipe
def txt2img(d):
 import torch
 p=sd_pipe('txt'); seed=int(d.get('seed',-1)); gen=None if seed<0 else torch.Generator(device='cpu').manual_seed(seed)
 im=p(d.get('prompt',''),negative_prompt=d.get('negative',''),width=int(d.get('width',512)),height=int(d.get('height',512)),num_inference_steps=int(d.get('steps',12)),guidance_scale=float(d.get('cfg',7)),generator=gen).images[0]
 del p; return saveimg(im,'txt2img')
def img2img(d):
 im=dataimg(d.get('image')); assert im is not None,'Select an input image.'
 import torch
 p=sd_pipe('img'); seed=int(d.get('seed',-1)); gen=None if seed<0 else torch.Generator(device='cpu').manual_seed(seed)
 im.thumbnail((768,768)); out=p(prompt=d.get('prompt',''),negative_prompt=d.get('negative',''),image=im,strength=float(d.get('strength',.65)),num_inference_steps=int(d.get('steps',12)),guidance_scale=float(d.get('cfg',7)),generator=gen).images[0]
 del p; return saveimg(out,'img2img')
def upscale(d):
 im=dataimg(d.get('image')); assert im is not None,'Select an input image.'
 exe=ROOT/'engines'/'realesrgan'/'realesrgan-ncnn-vulkan.exe'
 if exe.exists():
  inp=saveimg(im,'up_in'); out=OUT/f'upscale_{int(time.time())}.png'; cmd=[str(exe),'-i',str(inp),'-o',str(out),'-s',str(int(d.get('scale',2)))]
  subprocess.run(cmd,check=True,cwd=exe.parent); return out
 from PIL import Image
 s=int(d.get('scale',2)); return saveimg(im.resize((im.width*s,im.height*s),Image.Resampling.LANCZOS),'upscale')
def motion_video(im,d,prefix='video'):
 import imageio.v2 as imageio, numpy as np
 from PIL import Image
 fps=int(d.get('fps',12)); secs=max(1,int(d.get('seconds',4))); frames=[]
 W,H=im.size
 for i in range(fps*secs):
  z=1+0.08*i/max(1,fps*secs-1); cw,ch=int(W/z),int(H/z); x=(W-cw)//2; y=(H-ch)//2
  fr=im.crop((x,y,x+cw,y+ch)).resize((W,H),Image.Resampling.LANCZOS); frames.append(np.asarray(fr))
 p=OUT/f'{prefix}_{int(time.time())}.mp4'; imageio.mimsave(p,frames,fps=fps,codec='libx264',quality=7); return p
def textvideo(d):
 p=txt2img(d); from PIL import Image; return motion_video(Image.open(p).convert('RGB'),d,'textvideo')
def framesvideo(d):
 a=dataimg(d.get('image')); b=dataimg(d.get('image2')); assert a is not None and b is not None,'Select first and last images.'
 import imageio.v2 as imageio, numpy as np
 from PIL import Image
 b=b.resize(a.size); fps=int(d.get('fps',12)); n=fps*max(1,int(d.get('seconds',4))); frames=[]
 for i in range(n): frames.append(np.asarray(Image.blend(a,b,i/max(1,n-1))))
 p=OUT/f'frames_{int(time.time())}.mp4'; imageio.mimsave(p,frames,fps=fps,codec='libx264',quality=7); return p
def multiref(d):
 from PIL import Image,ImageOps
 ims=[dataimg(x) for x in d.get('images',[]) if x]; ims=[x for x in ims if x]; assert ims,'Select reference images.'
 thumbs=[]
 for im in ims[:8]: im.thumbnail((384,384)); thumbs.append(ImageOps.pad(im,(384,384)))
 cols=2; rows=(len(thumbs)+1)//2; sheet=Image.new('RGB',(cols*384,rows*384),'white')
 for i,im in enumerate(thumbs): sheet.paste(im,((i%2)*384,(i//2)*384))
 return saveimg(sheet,'multiref_sheet')
def tts(d):
 c=cfg(); exe=Path(c.get('piper_exe','')); voice=Path(c.get('piper_voice','')); assert exe.exists(),'Install Piper executable in engines/piper.'; assert voice.exists(),'Set Piper voice .onnx path in Settings.'
 p=OUT/f'tts_{int(time.time())}.wav'; subprocess.run([str(exe),'--model',str(voice),'--output_file',str(p)],input=d.get('prompt','').encode('utf8'),check=True); return p
def generate(d):
 m=d.get('mode'); f={'text-image':txt2img,'image-image':img2img,'upscale':upscale,'text-video':textvideo,'frames-video':framesvideo,'multi-ref':multiref,'tts':tts}.get(m); assert f,'Unsupported mode'; return f(d)
class H(SimpleHTTPRequestHandler):
 def translate_path(self,path): return str(WEB/(path.split('?',1)[0].lstrip('/') or 'index.html'))
 def sendj(self,o,status=200):
  b=json.dumps(o).encode(); self.send_response(status); self.send_header('Content-Type','application/json'); self.send_header('Content-Length',len(b)); self.end_headers(); self.wfile.write(b)
 def do_GET(self):
  if self.path=='/api/status': return self.sendj({'ok':True,'python':sys.version.split()[0],'config':cfg(),'sd_ready':Path(cfg()['sd_model']).exists()})
  if self.path.startswith('/output/'):
   p=OUT/Path(self.path).name
   if p.exists():
    self.send_response(200); self.send_header('Content-Type','application/octet-stream'); self.send_header('Content-Length',p.stat().st_size); self.end_headers(); self.wfile.write(p.read_bytes()); return
  return super().do_GET()
 def do_POST(self):
  try:d=json.loads(self.rfile.read(int(self.headers.get('Content-Length','0'))) or b'{}')
  except:return self.sendj({'ok':False,'error':'Bad JSON'},400)
  if self.path=='/api/settings':
   c=cfg();c.update(d);CFG.write_text(json.dumps(c,indent=2));return self.sendj({'ok':True,'config':c})
  if self.path=='/api/generate':
   try:
    p=generate(d); return self.sendj({'ok':True,'file':p.name,'url':'/output/'+p.name})
   except Exception as e:return self.sendj({'ok':False,'error':f'{type(e).__name__}: {e}'},500)
  return self.sendj({'ok':False},404)
 def log_message(self,*a):pass
if __name__=='__main__':
 port=7860; threading.Timer(1,lambda:webbrowser.open(f'http://127.0.0.1:{port}')).start(); print('Loumer Local AI Studio Engine:',f'http://127.0.0.1:{port}'); ThreadingHTTPServer(('127.0.0.1',port),H).serve_forever()
