#!/usr/bin/env python3
"""Compose twelve real world renders into two readable four-second boards."""
import json
import os
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/os.environ.get('DEMO_OUT','outputs/demo-v3')
rows=json.loads((OUT/'gallery.json').read_text())
font=lambda size:ImageFont.truetype('/System/Library/Fonts/Menlo.ttc',size)
names=['Mixed','Temperate','Desert','Frozen']
(OUT/'raw').mkdir(exist_ok=True)
assert len(rows)==12 and all(sum(r['biome']==b for r in rows)==3 for b in range(4))
for board,name in enumerate(['02-world-generation','03-world-variety']):
    im=Image.new('RGB',(1920,1080),(20,23,25));d=ImageDraw.Draw(im)
    d.text((48,32),'PROCEDURAL ENVIRONMENTS',font=font(16),fill='#9bc9bd')
    d.text((46,63),'Wave Function Collapse',font=font(42),fill='#eeefea')
    d.text((48,122),'Different worlds. Different challenges.',font=font(20),fill='#b9c1be')
    d.text((1505,43),f'WORLD GALLERY  {board+1} / 2',font=font(16),fill='#9bc9bd')
    for i,r in enumerate(rows[board*6:board*6+6]):
        x=48+(i%3)*616;y=174+(i//3)*424
        picture=Image.open(OUT/r['image']).resize((592,333),Image.Resampling.LANCZOS)
        im.paste(picture,(x,y));d.rectangle((x,y,x+591,y+332),outline='#424b4b',width=1)
        d.text((x,y+345),names[r['biome']].upper(),font=font(21),fill='#eeefea')
        layout='Symmetric' if r['sym'] else 'Asymmetric'
        d.text((x+245,y+348),layout,font=font(17),fill='#9bc9bd')
        d.text((x,y+377),f"Seed {r['seed']}  ·  Bases {r['a']} / {r['b']}",font=font(15),fill='#a3afad')
    d.text((48,1037),'12 seeds / 4 terrain palettes / Symmetric + asymmetric',font=font(17),fill='#9bc9bd')
    im.save(OUT/f'{name}.png');im.save(OUT/f'{name}-capture-first.jpg',quality=95)
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-loop','1','-framerate','30','-i',str(OUT/f'{name}.png'),
        '-t','4','-an','-c:v','libx264','-crf','17','-pix_fmt','yuv420p','-movflags','+faststart',str(OUT/'raw'/f'{name}.mp4')],check=True)
    print('BOARD',name)
