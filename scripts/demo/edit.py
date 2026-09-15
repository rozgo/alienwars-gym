#!/usr/bin/env python3
"""Assemble the 57-second showcase and its original ambient soundtrack."""
import hashlib
import json
from pathlib import Path
import subprocess
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'outputs/demo'
SHOTS = [('01-opening',4),('02-terrain',6),('03-lidar',3),('04-rf',3),
         ('05-depth',4),('06-ground',3),('07-naval',4),('08-entrance',4),
         ('09-tunnel',7),('10-isolation',7),('11-bridge',3),('12-variety',3),('13-closing',6)]
def run(args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()

def soundtrack():
    # Original, seeded synthesis; no sampled recording or licensed music.
    rate=48000;duration=57;mix=np.zeros((rate*duration,2),dtype=np.float64)
    rng=np.random.default_rng(73)
    def tone(start,length,hz,amp,pan=0,pluck=False):
        lo=max(0,round(start*rate));hi=min(len(mix),lo+round(length*rate))
        if hi<=lo:return
        t=np.arange(hi-lo)/rate
        env=(1-np.exp(-t/(.006 if pluck else .9)))*np.minimum(1,(length-t)/(.22 if pluck else 2))
        if pluck:env*=np.exp(-t*3.6)
        env=np.maximum(0,env)
        phase=2*np.pi*hz*t
        sound=(np.sin(phase)+.18*np.sin(phase*2)+.04*np.sin(phase*3))*amp*env
        mix[lo:hi,0]+=sound*np.sqrt((1-pan)/2)
        mix[lo:hi,1]+=sound*np.sqrt((1+pan)/2)
    midi=lambda n:440*2**((n-69)/12)
    beat=60/84;bar=beat*4
    chords=[[52,55,59,66],[48,55,59,62],[45,52,55,59],[50,57,62,64],[52,55,59,66]]
    for section,notes in enumerate(chords):
        start=section*bar*4
        for j,n in enumerate(notes):tone(start,bar*4+2,midi(n),.035,(j-1.5)/2)
        for b in range(16):
            time=start+b*beat
            if time>53:continue
            if b%2==0:tone(time,.6,midi(notes[0]-12),.11,0,True)
            if b%2==1:tone(time,1.4,midi(notes[(b//2)%4]+12),.028,float(rng.uniform(-.6,.6)),True)
    # A quiet filtered-noise pulse gives movement without competing with the visuals.
    for i in range(4,75,2):
        lo=round(i*beat*rate);n=round(.075*rate)
        if lo+n>len(mix):break
        noise=rng.normal(0,1,n);noise=np.convolve(noise,np.ones(9)/9,mode='same')
        sound=noise*np.exp(-np.arange(n)/rate*70)*.018
        mix[lo:lo+n]+=sound[:,None]
    fade=np.minimum(1,np.arange(len(mix))/(rate*1.4))*np.minimum(1,(len(mix)-np.arange(len(mix)))/(rate*3.0))
    mix*=fade[:,None];mix=np.tanh(mix)
    target=OUT/'soundtrack.wav'
    with wave.open(str(target),'wb') as f:
        f.setnchannels(2);f.setsampwidth(2);f.setframerate(rate);f.writeframes((mix*32767).astype('<i2').tobytes())
    return target

def main():
    for name,duration in SHOTS:
        assert Image.open(OUT/f'{name}-capture-first.jpg').size==(1920,1080),f'{name}: cropped capture'
        raw=OUT/'raw'/f'{name}.mp4'
        data=json.loads(run(['ffprobe','-v','error','-show_entries','format=duration:stream=width,height,avg_frame_rate','-of','json',str(raw)]))
        assert abs(float(data['format']['duration'])-duration)<.05,(name,data)
        assert data['streams'][0]['width']==1920 and data['streams'][0]['height']==1080
        assert data['streams'][0]['avg_frame_rate']=='30/1'
    audio=soundtrack()
    playlist=OUT/'concat.txt';playlist.write_text(''.join(f"file 'raw/{name}.mp4'\n" for name,_ in SHOTS))
    final=OUT/'AlienWars-Gym-Demo.mp4'
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','warning','-y','-f','concat','-safe','0','-i',str(playlist),'-i',str(audio),
        '-map','0:v:0','-map','1:a:0','-vf','fade=t=in:st=0:d=0.3,fade=t=out:st=56.4:d=0.6',
        '-c:v','libx264','-preset','medium','-crf','18','-pix_fmt','yuv420p','-r','30',
        '-af','loudnorm=I=-20:TP=-2:LRA=8','-c:a','aac','-b:a','192k','-ar','48000','-t','57','-movflags','+faststart',
        '-metadata','title=AlienWars Gym — Procedural Worlds',
        '-metadata','comment=Actual Raylib/WebAssembly Map Lab footage. Original ambient soundtrack. Built on PufferLib 5.',str(final)],check=True,cwd=ROOT)
    # Contact sheet for editorial QA. Each image comes from the encoded final cut.
    frames=OUT/'review';frames.mkdir(exist_ok=True)
    times=[2,7,11.5,14.5,18,21.5,25,29,34,40,43.5,46.5,49.5,54]
    font=ImageFont.truetype('/System/Library/Fonts/Menlo.ttc',18)
    sheet=Image.new('RGB',(1280,7*384),(20,24,26));draw=ImageDraw.Draw(sheet)
    for i,time in enumerate(times):
        dest=frames/f'{time:04.1f}.jpg'
        run(['ffmpeg','-v','error','-y','-ss',str(time),'-i',str(final),'-frames:v','1','-q:v','2',str(dest)])
        im=Image.open(dest);im.thumbnail((640,360));x=(i%2)*640;y=(i//2)*384
        sheet.paste(im,(x,y));draw.text((x+12,y+361),f'{time:04.1f}s',font=font,fill=(165,200,188))
    sheet.save(OUT/'contact-sheet.jpg',quality=92)
    run(['ffmpeg','-v','error','-y','-ss','54','-i',str(final),'-frames:v','1',str(OUT/'poster.png')])
    probe=json.loads(run(['ffprobe','-v','error','-show_entries','format=duration,size:stream=codec_name,codec_type,width,height,avg_frame_rate,sample_rate','-of','json',str(final)]))
    probe['sha256']=hashlib.sha256(final.read_bytes()).hexdigest()
    probe['soundtrack']='Original deterministic ambient synthesis; no third-party samples.'
    probe['source_url']='https://rozgo.github.io/alienwars-gym/maplab/?seed=73&sym=1&a=6&b=6&biome=2&tunnels=1&cut=1&sensors=15&unit=7&sensorsAll=1'
    (OUT/'video-manifest.json').write_text(json.dumps(probe,indent=2)+'\n')
    print(json.dumps(probe,indent=2))

if __name__=='__main__':main()
