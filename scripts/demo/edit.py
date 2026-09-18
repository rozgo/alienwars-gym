#!/usr/bin/env -S uv run --group art
"""Assemble and verify the silent showcase for local review."""
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / os.environ.get('DEMO_OUT', 'outputs/demo-v4')
CAPTURE = json.loads((OUT/'capture.json').read_text())
SHOTS = [(s['name'],s['seconds']) for s in CAPTURE['shots']]
DURATION = sum(duration for _,duration in SHOTS)
REVISION = CAPTURE['revision']
assert 0 < DURATION < 60, 'Keep the complete revision under one minute'
def run(args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()

def main():
    for name,duration in SHOTS:
        assert Image.open(OUT/f'{name}-capture-first.jpg').size==(1920,1080),f'{name}: cropped capture'
        raw=OUT/'raw'/f'{name}.mp4'
        data=json.loads(run(['ffprobe','-v','error','-show_entries','format=duration:stream=width,height,avg_frame_rate','-of','json',str(raw)]))
        assert abs(float(data['format']['duration'])-duration)<.05,(name,data)
        assert data['streams'][0]['width']==1920 and data['streams'][0]['height']==1080
        assert data['streams'][0]['avg_frame_rate']=='30/1'
    playlist=OUT/'concat.txt';playlist.write_text(''.join(f"file 'raw/{name}.mp4'\n" for name,_ in SHOTS))
    final=OUT/f'AlienWars-Gym-Demo-v{REVISION}.mp4'
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','warning','-y','-f','concat','-safe','0','-i',str(playlist),
        '-map','0:v:0','-an','-vf',f'fade=t=in:st=0:d=0.3,fade=t=out:st={DURATION-.6}:d=0.6',
        '-c:v','libx264','-preset','medium','-crf','18','-pix_fmt','yuv420p','-r','30',
        '-t',str(DURATION),'-movflags','+faststart',
        '-metadata',f'title=AlienWars Gym — Demo v{REVISION}',
        '-metadata','comment=Actual Map Lab, training metrics and trained Navigation Lab footage. Silent review cut. PufferLib 5 / Raylib.',str(final)],check=True,cwd=ROOT)
    # Contact sheet for editorial QA. Each image comes from the encoded final cut.
    frames=OUT/'review';frames.mkdir(exist_ok=True)
    times=[]; cursor=0
    for _,duration in SHOTS:
        times.append(cursor+duration/2);cursor+=duration
    times.extend([22.2,23.2,27.2,30.8,33.7,42.2,45.7,46.2,53.7] if REVISION==4 else [36.4,37.4,42.4,46.4,50.5]);times.sort()
    font=ImageFont.truetype('/System/Library/Fonts/Menlo.ttc',18)
    sheet=Image.new('RGB',(1280,math.ceil(len(times)/2)*384),(20,24,26));draw=ImageDraw.Draw(sheet)
    for i,time in enumerate(times):
        dest=frames/f'{time:04.1f}.jpg'
        run(['ffmpeg','-v','error','-y','-ss',str(time),'-i',str(final),'-frames:v','1','-q:v','2',str(dest)])
        im=Image.open(dest);im.thumbnail((640,360));x=(i%2)*640;y=(i//2)*384
        sheet.paste(im,(x,y));draw.text((x+12,y+361),f'{time:04.1f}s',font=font,fill=(165,200,188))
    sheet.save(OUT/'contact-sheet.jpg',quality=92)
    run(['ffmpeg','-v','error','-y','-ss',str(DURATION-3),'-i',str(final),'-frames:v','1',str(OUT/'poster.png')])
    probe=json.loads(run(['ffprobe','-v','error','-show_entries','format=duration,size:stream=codec_name,codec_type,width,height,avg_frame_rate,sample_rate','-of','json',str(final)]))
    probe['sha256']=hashlib.sha256(final.read_bytes()).hexdigest()
    probe['revision']=REVISION
    probe['world_count']=len({w['hash'] for w in CAPTURE['worlds']})
    probe['capture_source_commits']=sorted({w['build']['source_commit'] for w in CAPTURE['worlds']})
    probe['audio']='None: silent video, no audio stream.'
    probe['gallery']={'worlds':12,'palettes':4,'seconds':8}
    probe['presentation']='Frame-synchronous camera tracks; stronger cached sensor overlays; production terrain and sensor measurements.'
    if 'supplied_clips' in CAPTURE:
        probe['supplied_clips']=CAPTURE['supplied_clips']
        probe['notes']=CAPTURE['notes']
    assert abs(float(probe['format']['duration'])-DURATION)<.01
    assert all(s['codec_type']!='audio' for s in probe['streams'])
    probe['source_url']='https://rozgo.github.io/alienwars-gym/maplab/?seed=73&sym=1&a=6&b=6&biome=2&tunnels=1&cut=1&sensors=15&unit=7&sensorsAll=1'
    (OUT/'video-manifest.json').write_text(json.dumps(probe,indent=2)+'\n')
    print(json.dumps(probe,indent=2))

if __name__=='__main__':main()
