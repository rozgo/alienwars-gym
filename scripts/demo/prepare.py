#!/usr/bin/env -S uv run --group art
"""Prepare the 59-second revision from reviewed footage and supplied RL clips."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/os.environ.get('DEMO_SOURCE','outputs/demo-v3')
OUT=ROOT/os.environ.get('DEMO_OUT','outputs/demo-v4')
RL=Path(os.environ.get('DEMO_RL_CLIPS',str(ROOT.parent/'alienwars-gym-rl/outputs/navigation/demo')))
OUT.mkdir(parents=True,exist_ok=True)
(OUT/'raw').mkdir(exist_ok=True)
font=lambda size:ImageFont.truetype('/System/Library/Fonts/Menlo.ttc',size)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()

def overlays():
    # Replace only the observatory's heading area, above every metric and plot.
    # Keep the source's recorded replay indicator visible as well.
    im=Image.new('RGBA',(1920,1080));d=ImageDraw.Draw(im)
    d.rectangle((140,77,1490,193),fill=(24,24,24,255))
    d.text((153,83),'PUFFERLIB 5 / REINFORCEMENT LEARNING',font=font(16),fill='#9bc9bd')
    d.text((151,106),'Training observatory',font=font(36),fill='#eeefea')
    d.text((153,158),'Learning progress · Policy behavior',font=font(20),fill='#b9c1be')
    im.save(OUT/'training-caption.png')
    im=Image.new('RGBA',(1920,1080));d=ImageDraw.Draw(im)
    for y in range(760,1080):
        d.line((0,y,1599,y),fill=(16,23,25,round(190*(y-760)/320)))
    d.text((52,893),'TRAINED POLICY / NAVIGATION LAB',font=font(16),fill='#9bc9bd')
    d.text((50,925),'Navigation Lab',font=font(36),fill='#eeefea')
    d.text((52,988),'Trained scout · Bridge crossing · Sensor overlays',font=font(18),fill='#b9c1be')
    im.save(OUT/'navigation-caption.png')

def main():
    only=next((a.split('=',1)[1] for a in sys.argv[1:] if a.startswith('--shot=')),None)
    original=json.loads((SOURCE/'capture.json').read_text())
    closing=json.loads((OUT/'13-closing-capture.json').read_text())
    supplied=json.loads((RL/'manifest.json').read_text())
    for item in supplied['clips']:
        assert digest(RL/item['file'])==item['sha256'],f"Changed supplied clip: {item['file']}"
    overlays()
    # (output name, source file, source in point, seconds, optional title overlay)
    # Keep Navigation Lab's full eight seconds at its original playback rate.
    # Observatory is trimmed to its first half; the recorded 55× rate is unchanged.
    timeline=[
        ('01-opening',SOURCE/'raw/01-opening.mp4',.4,3,None),
        ('02-lidar',SOURCE/'raw/04-lidar.mp4',0,4,None),
        ('03-radio',SOURCE/'raw/05-radio.mp4',0,4,None),
        ('04-depth',SOURCE/'raw/06-depth.mp4',0,4,None),
        ('05-naval',SOURCE/'raw/08-naval.mp4',0,4,None),
        ('06-entrance',SOURCE/'raw/09-entrance.mp4',.4,3,None),
        ('07-tunnel',SOURCE/'raw/10-tunnel.mp4',.3,5,None),
        ('08-network-symmetric',SOURCE/'raw/11-network-symmetric.mp4',.4,4,None),
        ('09-network-asymmetric',SOURCE/'raw/12-network-asymmetric.mp4',1,3,None),
        ('10-gallery-temperate-desert',SOURCE/'raw/02-world-generation.mp4',0,4,None),
        ('11-gallery-frozen-mixed',SOURCE/'raw/03-world-variety.mp4',0,4,None),
        ('12-training-observatory',RL/'training-observatory.mp4',0,4,OUT/'training-caption.png'),
        ('13-navigation-lab',RL/'nav-lab.mp4',0,8,OUT/'navigation-caption.png'),
        ('14-closing',OUT/'raw/13-closing.mp4',.5,5,None),
    ]
    assert sum(t[3] for t in timeline)==59
    assert only is None or only in {t[0] for t in timeline},'Unknown take'
    shots=[];time=0
    for name,source,start,seconds,overlay in timeline:
        assert source.exists(),source
        source_probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries','format=duration','-of','json',str(source)],text=True))
        assert start+seconds<=float(source_probe['format']['duration'])+.001
        args=['ffmpeg','-hide_banner','-loglevel','error','-y','-ss',str(start),'-i',str(source)]
        if overlay:
            args+=['-i',str(overlay),'-filter_complex','[0:v]setpts=PTS-STARTPTS,setsar=1[v];[v][1:v]overlay=0:0:format=auto,format=yuv420p[out]','-map','[out]']
        else:args+=['-map','0:v:0','-vf','setpts=PTS-STARTPTS,setsar=1']
        target=OUT/'raw'/f'{name}.mp4'
        assert source.resolve()!=target.resolve()
        args+=['-frames:v',str(seconds*30),'-an','-r','30','-c:v','libx264','-preset','fast','-crf','17','-pix_fmt','yuv420p','-movflags','+faststart',str(target)]
        if only is None or name==only:
            subprocess.run(args,check=True)
            subprocess.run(['ffmpeg','-v','error','-y','-i',str(target),'-frames:v','1','-q:v','2',str(OUT/f'{name}-capture-first.jpg')],check=True)
        assert target.exists(),f'Missing take: {target}'
        shots.append({'name':name,'seconds':seconds,'frames':seconds*30,'timeline_start':time,
                      'source':str(source),'source_sha256':digest(source),'source_in':start,
                      'playback_rate':1,'overlay':overlay.name if overlay else None})
        print('PREPARED',name,time,seconds,flush=True);time+=seconds
    report={'revision':4,'sourceURL':original['sourceURL'],'browser':original['browser'],
            'worlds':original['worlds']+closing['worlds'],'errors':original['errors']+closing['errors'],
            'shots':shots,'supplied_clips':supplied,'gallery_seconds':8,
            'notes':['AlienWars spelling corrected in the recaptured closing card.',
                     'Twelve-world gallery follows both tunnel network reveals.',
                     'Observatory uses seconds 0–4, with captions focused on reinforcement learning and policy behavior.',
                     'Navigation Lab retains the full supplied eight seconds.',
                     'The trained bridge sequence takes the place of the earlier ground bridge shot.']}
    (OUT/'capture.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()
