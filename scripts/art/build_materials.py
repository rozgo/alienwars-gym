#!/usr/bin/env python3
"""Fetch four CC0 Poly Haven scans and pack RGB albedo + height in alpha.
Build-time only: no external requests are made by the game. Requires Pillow.
"""
from pathlib import Path
import hashlib,io,json,urllib.request
from PIL import Image
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'resources/alienwars/art'
CACHE=ROOT/'outputs/art/source';CACHE.mkdir(parents=True,exist_ok=True)
UA='AlienWarsArt/1.0 (https://github.com/rozgo/alienwars-gym)'
def get(url):
    return urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':UA}),timeout=90).read()
manifest={}
for name,stem in [('forest_ground_04','forest'),('rock_face_03','rock'),('leafy_grass','grass'),('bark_brown_02','bark')]:
    files=json.loads(get('https://api.polyhaven.com/files/'+name));channels={};inputs={}
    for channel in ['Diffuse','Displacement']:
        info=files[channel]['1k']['jpg'];target=CACHE/(name+'_'+channel+'.jpg')
        data=target.read_bytes() if target.exists() else get(info['url'])
        assert hashlib.md5(data).hexdigest()==info['md5'],info['url']
        target.write_bytes(data);inputs[channel]=info
        channels[channel]=Image.open(io.BytesIO(data)).resize((1024,1024),Image.Resampling.LANCZOS)
    image=channels['Diffuse'].convert('RGBA');image.putalpha(channels['Displacement'].convert('L'))
    path=OUT/(stem+'.png');image.save(path,optimize=True)
    manifest[stem]={'asset':name,'source':'https://polyhaven.com/a/'+name,'license':'CC0-1.0','inputs':inputs,'packed':'RGB albedo (sRGB), alpha height (linear), 1024x1024','sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size}
    print(stem,path.stat().st_size)
(OUT/'materials.json').write_text(json.dumps(manifest,indent=2)+'\n')
