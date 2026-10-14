#!/usr/bin/env python3
"""Original ProsperoTube UI tones, synthesized with Python math; no sampled audio.
Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse, hashlib, io, json, math, struct, wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'native/afterglow/sound/recordings/prosperotube'
RATE=48000
# D minor pentatonic: a rounded fundamental with quiet, glass-like partials.
D,F,G,A,C=293.664768,349.228231,391.995436,440.0,523.251131
# duration, RMS dBFS, pitches, variations. The same notes fall for Back/Close.
CUES={
 'focus':(.045,-35,[D*2],2),'select':(.14,-30,[D,A],1),
 'back':(.14,-31,[A,D],1),'tab':(.10,-33,[F,A],1),
 'open':(.22,-30,[D,F],1),'modal_open':(.26,-30,[D,A],1),
 'modal_close':(.22,-31,[A,D],1),'toggle':(.085,-33,[G],3),
 'slider':(.040,-35,[F*2],3),'error':(.16,-32,[F,D],1),
 'notify':(.28,-29,[G,A],1),'play_start':(.40,-26,[D],1),
 'favorite_on':(.22,-28,[D,A],1),'favorite_off':(.18,-31,[A,D],1),
 'saved':(.32,-28,[D,F,A],1),'welcome':(.72,-26,[D,F,A,D*2],1),
 'resume':(.18,-30,[D,G],1),'tick':(.025,-36,[D*2],3),
 'type':(.042,-35,[A*2],3),'erase':(.060,-35,[F*2],1),
 'pause':(.12,-30,[G,F],1),'seek_commit':(.09,-31,[D*2,D],1),
 'skip':(.20,-30,[F,G,A],1),'signed_in':(.60,-25,[D,F,A],1),
 'goodbye':(.40,-27,[D*2,A],1),
}
def synth(cue,variation):
    duration,db,notes,_=CUES[cue];n=round(duration*RATE);values=[]
    detune=1+(variation-2)*.006
    for i in range(n):
        t=i/RATE;value=0.0
        for k,f in enumerate(notes):
            start=k*duration*.22;length=duration-start;u=t-start
            if not 0<=u<length:continue
            # Soft attacks and an explicit end fade keep repeated typing/detents click-free.
            attack=.035 if cue=='play_start' else min(.006,length*.15)
            env=min(1,u/attack)**2*math.exp(-4*u/length)
            env*=min(1,(length-u)/min(.014,length*.25))**2
            hz=f*detune
            phase=2*math.pi*hz*u
            value+=env*(math.sin(phase)+.20*math.sin(phase*2.01)+.07*math.sin(phase*3.98))
        values.append(value)
    rms=math.sqrt(sum(x*x for x in values)/n)
    gain=10**(db/20)/rms
    ceiling=10**((-19 if cue in ('focus','type','erase','tick','slider','tab','toggle') else -12)/20)
    gain=min(gain,ceiling/max(abs(x) for x in values))
    return [round(x*gain*32767) for x in values]
def wav_bytes(samples):
    out=io.BytesIO()
    with wave.open(out,'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE)
        w.writeframes(struct.pack('<'+'h'*len(samples),*samples))
    return out.getvalue()
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');parser.add_argument('--sampler',type=Path);args=parser.parse_args()
    DEST.mkdir(parents=True,exist_ok=True);manifest={'generator':'scripts/generate_afterglow_sounds.py','provenance':'Original procedural synthesis; no recordings, samples, services or downloaded audio.','copyright':'Copyright (C) 2026 BlackBearReloaded','license':'GPL-3.0-or-later','files':{}}
    reel=[];timeline=[]
    for cue,(_,target,_,variations) in CUES.items():
        for v in range(1,variations+1):
            samples=synth(cue,v);data=wav_bytes(samples);name=f'{cue}_{v:02}.wav'
            if args.check:assert (DEST/name).read_bytes()==data,name+' differs from reproducible source'
            else:(DEST/name).write_bytes(data)
            peak=max(abs(x) for x in samples)/32768;rms=math.sqrt(sum(x*x for x in samples)/len(samples))/32768
            manifest['files'][name]={'sha256':hashlib.sha256(data).hexdigest(),'seconds':len(samples)/RATE,'rms_dbfs':round(20*math.log10(rms),2),'peak_dbfs':round(20*math.log10(peak),2),'target_rms_dbfs':target}
            if v==1:
                timeline.append({'cue':cue,'at_seconds':round(len(reel)/RATE,3)})
                reel.extend(samples);reel.extend([0]*round(.35*RATE))
    meta=json.dumps(manifest,indent=2,sort_keys=True)+'\n';path=DEST.parent/'manifest.json'
    if args.check:assert path.read_text()==meta,'manifest differs'
    else:path.write_text(meta)
    if args.sampler:
        args.sampler.parent.mkdir(parents=True,exist_ok=True);args.sampler.write_bytes(wav_bytes(reel))
        args.sampler.with_suffix('.json').write_text(json.dumps(timeline,indent=2)+'\n')
    print(f'PASS {len(manifest["files"])} original sound assets, reproducible PCM16 mono 48kHz')
if __name__=='__main__':main()
