from huggingface_hub import snapshot_download
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'models'/'diffusers'/'sd15'
p.mkdir(parents=True,exist_ok=True)
print('Downloading Stable Diffusion 1.5 to',p)
snapshot_download(repo_id='stable-diffusion-v1-5/stable-diffusion-v1-5',local_dir=str(p),ignore_patterns=['*.ckpt','*.safetensors'])
print('Done.')
