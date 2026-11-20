import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/frame'))
from pathlib import Path
import tempfile
import unittest
from appid_guard import archive_generated_appid


class AppIdGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.game=self.root/'game';self.game.mkdir()
        self.setup=self.root/'setup'

    def test_no_generated_marker_needs_no_changes(self):
        self.assertIsNone(archive_generated_appid(self.game,self.setup))
        self.assertFalse(self.setup.exists())

    def test_generated_marker_is_preserved_byte_for_byte(self):
        data=b'400\n\0';source=self.game/'steam_appid.txt';source.write_bytes(data)
        archived=Path(archive_generated_appid(self.game,self.setup))
        self.assertFalse(source.exists());self.assertEqual(archived.read_bytes(),data)

    def test_unexpected_marker_is_not_removed(self):
        source=self.game/'steam_appid.txt';source.write_bytes(b'another app')
        with self.assertRaisesRegex(RuntimeError,'Unexpected'):
            archive_generated_appid(self.game,self.setup)
        self.assertEqual(source.read_bytes(),b'another app');self.assertFalse(self.setup.exists())

    def test_repeated_generated_markers_have_separate_archives(self):
        source=self.game/'steam_appid.txt'
        source.write_bytes(b'400');first=Path(archive_generated_appid(self.game,self.setup))
        source.write_bytes(b'400\n\0');second=Path(archive_generated_appid(self.game,self.setup))
        self.assertNotEqual(first,second);self.assertEqual(first.read_bytes(),b'400')
        self.assertEqual(second.read_bytes(),b'400\n\0')


if __name__=='__main__':unittest.main(verbosity=2)
