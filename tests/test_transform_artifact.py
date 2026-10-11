"""Artifact integrity gates only; these fixtures do not claim GPU compilation."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from build_video_transform_shaders import verify, digest
from install_video_transform_shaders import install, EVO, PIN
from unittest.mock import patch

def sha(data):
    return hashlib.sha256(data).hexdigest()

class TransformArtifactTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root/"headers").mkdir()
        (self.root/"metadata").mkdir()
        self.expected = {"sources": {"evo_revision": "fixture",
            "pipes": {"video_fixture.pipe": sha(b"source")}}}
        self.expected["packager_sha256"]=digest(EVO/"tools/build_agc_pipes.py")
        self.header = b"fixture header, not a compiled shader"
        self.meta = {"target":"gfx1013", "gfxip":"10.1.3", "pipeline_type":"Ngg",
                     "no_relocations":True, "source_sha256":sha(b"source")}
        self.write()

    def write(self):
        (self.root/"headers/video_fixture_pipe.h").write_bytes(self.header)
        metadata = json.dumps(self.meta).encode()
        (self.root/"metadata/video_fixture.manifest.json").write_bytes(metadata)
        self.manifest = {"build": copy.deepcopy(self.expected),
                         "headers":{"video_fixture_pipe.h":sha(self.header)},
                         "metadata":{"video_fixture.manifest.json":sha(metadata)}}
        self.save_manifest()

    def save_manifest(self):
        (self.root/"manifest.json").write_text(json.dumps(self.manifest))

    def native_tree(self):
        native=self.root/"native"
        native.mkdir()
        (native/".prosperotube-source").write_text(PIN)
        target=native/"projects/evoplayer/shaders/agc"
        target.mkdir(parents=True)
        (target/"video_fixture_pipe.h").write_bytes(b"old")
        (target/"ui_fixture_pipe.h").write_bytes(b"unrelated UI")
        return native,target

    def test_install_preserves_unrelated_headers(self):
        native,target=self.native_tree()
        with patch("install_video_transform_shaders.current_sources",return_value=self.expected["sources"]):
            install(self.root,native)
        self.assertEqual((target/"video_fixture_pipe.h").read_bytes(),self.header)
        self.assertEqual((target/"ui_fixture_pipe.h").read_bytes(),b"unrelated UI")

    def test_install_corruption_changes_nothing(self):
        native,target=self.native_tree()
        (self.root/"headers/video_fixture_pipe.h").write_bytes(b"corrupt")
        with patch("install_video_transform_shaders.current_sources",return_value=self.expected["sources"]):
            with self.assertRaises(ValueError): install(self.root,native)
        self.assertEqual((target/"video_fixture_pipe.h").read_bytes(),b"old")

    def test_install_stale_source_changes_nothing(self):
        native,target=self.native_tree()
        with patch("install_video_transform_shaders.current_sources",return_value={}):
            with self.assertRaises(ValueError): install(self.root,native)
        self.assertEqual((target/"video_fixture_pipe.h").read_bytes(),b"old")

    def test_install_wrong_destination_pin(self):
        native,target=self.native_tree()
        (native/".prosperotube-source").write_text("wrong")
        with self.assertRaises(ValueError): install(self.root,native)
        self.assertEqual((target/"video_fixture_pipe.h").read_bytes(),b"old")

    def test_install_cannot_modify_pinned_evo(self):
        with self.assertRaises(ValueError): install(self.root,EVO)

    def test_complete_integrity_fixture(self):
        self.assertEqual(verify(self.root,self.expected),self.root)

    def test_stale_sources(self):
        other=copy.deepcopy(self.expected)
        other["sources"]["evo_revision"]="different"
        with self.assertRaises(ValueError): verify(self.root,other)

    def test_missing_completion_manifest(self):
        (self.root/"manifest.json").unlink()
        with self.assertRaises(FileNotFoundError): verify(self.root,self.expected)

    def test_missing_header(self):
        (self.root/"headers/video_fixture_pipe.h").unlink()
        with self.assertRaises(FileNotFoundError): verify(self.root,self.expected)

    def test_corrupt_header(self):
        (self.root/"headers/video_fixture_pipe.h").write_bytes(b"truncated")
        with self.assertRaises(ValueError): verify(self.root,self.expected)

    def test_incomplete_set(self):
        self.manifest["headers"].clear();self.save_manifest()
        with self.assertRaises(ValueError): verify(self.root,self.expected)

    def test_unexpected_path(self):
        self.manifest["headers"]["../outside.h"]=sha(b"")
        self.save_manifest()
        with self.assertRaises(ValueError): verify(self.root,self.expected)

    def test_corrupt_metadata(self):
        (self.root/"metadata/video_fixture.manifest.json").write_text("{}")
        with self.assertRaises(ValueError): verify(self.root,self.expected)

    def test_wrong_target(self):
        for field,value in [("target","gfx1030"),("gfxip","10.3.0"),
                            ("pipeline_type","VsPs"),("no_relocations",False),
                            ("source_sha256",sha(b"stale shader"))]:
            with self.subTest(field=field):
                original=self.meta[field];self.meta[field]=value;self.write()
                with self.assertRaises(ValueError): verify(self.root,self.expected)
                self.meta[field]=original

if __name__ == "__main__":
    unittest.main()
