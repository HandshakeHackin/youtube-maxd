"""A release must replace a CI label without duplicating entries or changing payloads."""
from pathlib import Path
import tempfile
import unittest
import zipfile
from publish_maxd_150 import relabel

class RelabelTest(unittest.TestCase):
    def test_existing_label_is_replaced_once_and_payload_modes_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original, first, second = (root / name for name in ('ci.zip', 'release.zip', 'again.zip'))
            payload = b'fixture executable, not a PS5 binary'
            with zipfile.ZipFile(original, 'w') as archive:
                for name, data in [('eboot.bin', payload), ('build-label.txt', b'PR 4, fixture\n')]:
                    info = zipfile.ZipInfo('PPSA99991/' + name)
                    info.external_attr = 0o100777 << 16
                    info.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(info, data)
            relabel(original, first, 'YouTube Max’d 1.5.0')
            relabel(first, second, 'YouTube Max’d 1.5.0 repeat')
            for path, label in [(first, 'YouTube Max’d 1.5.0'), (second, 'YouTube Max’d 1.5.0 repeat')]:
                with zipfile.ZipFile(path) as archive:
                    self.assertEqual(len(archive.namelist()), 2)
                    self.assertEqual(archive.namelist().count('PPSA99991/build-label.txt'), 1)
                    self.assertEqual(archive.read('PPSA99991/eboot.bin'), payload)
                    self.assertEqual(archive.read('PPSA99991/build-label.txt'), (label + '\n').encode())
                    self.assertTrue(all((i.external_attr >> 16) & 0o777 == 0o777 for i in archive.infolist()))
                    self.assertIsNone(archive.testzip())

if __name__ == '__main__':
    unittest.main()
