"""Independent format, level, duration, provenance and package checks for original UI audio."""
import hashlib, json, math, struct, sys, wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'native/afterglow/sound/recordings'
manifest=json.loads((base/'manifest.json').read_text())
assert manifest['license']=='GPL-3.0-or-later'
assert 'procedural' in manifest['provenance']
assert len(manifest['files'])==34
limits={'type':(.035,.050),'erase':(.060,.060),'tick':(.020,.030),'favorite_on':(.220,.220),'favorite_off':(.180,.180),'play_start':(.350,.450),'pause':(.120,.120),'seek_commit':(.090,.090),'skip':(.200,.200),'signed_in':(.600,.600),'goodbye':(.400,.400)}
for name,meta in manifest['files'].items():
    file=base/'prosperotube'/name
    assert hashlib.sha256(file.read_bytes()).hexdigest()==meta['sha256'],name
    with wave.open(str(file),'rb') as w:
        assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getcomptype())==(1,2,48000,'NONE'),name
        samples=struct.unpack('<'+'h'*w.getnframes(),w.readframes(w.getnframes()))
    assert samples[0]==0 and samples[-1]==0,name
    peak=max(abs(x) for x in samples)/32768
    rms=math.sqrt(sum(x*x for x in samples)/len(samples))/32768
    assert peak<10**(-12/20) and min(samples)>-32768 and max(samples)<32767,name
    assert abs(20*math.log10(rms)-meta['target_rms_dbfs'])<.15,name
    cue=name.rsplit('_',1)[0];seconds=len(samples)/48000
    lo,hi=limits.get(cue,(.020,.750 if cue=='welcome' else .500))
    assert lo-1e-6<=seconds<=hi+1e-6,name
    if cue in ('focus','type','erase','tick','slider'):
        assert peak<10**(-19/20),name
sys.path.insert(0,str(root/'scripts'))
import afterglow_overlay
shipped=afterglow_overlay.sound_files(root,Path('/nonexistent-kit'))
assert len(shipped)==34 and {name for name,_ in shipped}=={'prosperotube/'+name for name in manifest['files']}
if len(sys.argv)>1:
    work=Path(sys.argv[1])/'projects/evoplayer/assets'
    actual={f.relative_to(work/'hui/afterglow/sfx').as_posix() for f in (work/'hui/afterglow/sfx').rglob('*.wav')}
    assert actual=={name for name,_ in shipped}
    for name,file in shipped:assert (work/'hui/afterglow/sfx'/name).read_bytes()==file.read_bytes()
    notice=(work/'prosperotube-notices/afterglow-sounds.txt').read_text()
    assert 'Original procedural synthesis' in notice and 'ps5-homebrew-ui' not in notice
    assert (work/'prosperotube-notices/afterglow-sounds.json').read_bytes()==(base/'manifest.json').read_bytes()
print('PASS 34 PCM16/48kHz assets: duration, RMS/headroom, click-free endpoints, hashes, own provenance and packaging')
