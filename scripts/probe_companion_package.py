"""Build and unpack an isolated companion archive, then decode/seek its codec demos.

Imports the committed packaged companion, not the working source. The generated
archive is an offline probe artifact, not a native PS5 candidate or release.
"""
from pathlib import Path
import hashlib,json,subprocess,sys,tempfile,zipfile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'scripts'))
from generate_fixtures import generate
generate(root/'fixtures/generated')
import package_release
revision=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
out=root/'.deps/companion-package-smoke';out.mkdir(parents=True,exist_ok=True)
archive=out/'ProsperoTube-companion.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as bundle:
    for name,body in package_release.companion_entries(root,revision):
        bundle.writestr('ProsperoTube/'+name,body)
# A fresh destination prevents an older run from hiding missing packaged files.
with tempfile.TemporaryDirectory(prefix='unpacked-',dir=out) as temporary:
    with zipfile.ZipFile(archive) as bundle:
        assert bundle.testzip() is None
        bundle.extractall(temporary)
    package=Path(temporary)/'ProsperoTube'
    sys.path.insert(0,str(package/'companion'))
    import probe_av1
    import prosperotube.demo
    assert Path(prosperotube.demo.__file__).is_relative_to(package)
    for codec in ['av1','hevc']:
        probe_av1.main(codec, package/'fixtures/generated', generate_media=False)
with archive.open('rb') as stream:
    checksum=hashlib.file_digest(stream,'sha256').hexdigest()
print(json.dumps({'source_revision':revision,'archive':str(archive),'sha256':checksum,'result':'pass',
 'evidence':'Both codec probes use committed packaged companion source and archived media; no media regeneration during probes.'}))
