import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/frame'))
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from portal_startup_settings import startup_text,autoexec_text,apply_startup_settings

class StartupSettingsTests(unittest.TestCase):
    def test_values_clamped_and_duplicate_last_wins(self):
        text=startup_text('TextureQuality=1\nTextureQuality=2 # choice\nAnisotropicFiltering=16')
        self.assertIn('mat_picmip 2\n',text);self.assertIn('mat_forceaniso 16\n',text)
        for invalid in ('3','-1','1;quit','false','nan'):
            self.assertIn('mat_picmip 0\n',startup_text('TextureQuality='+invalid))
        commands='\n'.join(line for line in text.splitlines() if not line.startswith('//'))
        for forbidden in ('mat_hdr_level','mat_setvideomode','quit','disconnect','map '):self.assertNotIn(forbidden,commands)
    def test_autoexec_preserves_user_commands_and_is_idempotent(self):
        original='// user\r\nbind x screenshot\r\necho hello'
        result=autoexec_text(original);self.assertTrue(result.startswith(original))
        self.assertEqual(autoexec_text(result),result)
        for command in ('exec "portal_frame_startup.cfg"\n','EXEC portal_frame_startup // mine\n'):
            self.assertEqual(autoexec_text(command),command)
    def fixture(self):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        root=Path(temp.name);game=root/'game';setup=root/'setup';setup.mkdir()
        (game/'bin/VR').mkdir(parents=True);(game/'portal/cfg').mkdir(parents=True)
        (game/'bin/VR/config.txt').write_text('TextureQuality=1\nAnisotropicFiltering=4')
        return game,setup
    def test_idle_write_backups_and_repeat_changes_no_user_bytes(self):
        game,setup=self.fixture();auto=game/'portal/cfg/autoexec.cfg'
        before=b'// original\r\necho caf\xe9\r\n';auto.write_bytes(before)
        result=apply_startup_settings(game,setup)
        self.assertEqual(len(result['changed']),2);self.assertTrue(auto.read_bytes().startswith(before))
        self.assertEqual(next((setup/'Startup-Settings').glob('autoexec*')).read_bytes(),before)
        self.assertEqual(apply_startup_settings(game,setup)['changed'],[])
        (game/'bin/VR/config.txt').write_text('TextureQuality=2')
        result=apply_startup_settings(game,setup)
        self.assertEqual(len(result['changed']),1)
        self.assertIn('mat_picmip 2',(game/'portal/cfg/portal_frame_startup.cfg').read_text())
        self.assertEqual(auto.read_bytes().count(b'exec portal_frame_startup.cfg'),1)
    def test_running_app_prevents_all_writes(self):
        game,setup=self.fixture()
        with self.assertRaises(RuntimeError):apply_startup_settings(game,setup,['hl2.exe'])
        self.assertFalse((game/'portal/cfg/autoexec.cfg').exists())
        self.assertFalse((setup/'Startup-Settings').exists())
    def test_failure_rolls_back_first_write(self):
        game,setup=self.fixture()
        import portal_startup_settings as module
        replace=module.os.replace;calls=[]
        def fail_second(source,target):
            calls.append(target)
            if len(calls)==2:raise OSError('injected failure')
            return replace(source,target)
        with patch.object(module.os,'replace',side_effect=fail_second),self.assertRaises(OSError):apply_startup_settings(game,setup)
        self.assertFalse((game/'portal/cfg/portal_frame_startup.cfg').exists())
        self.assertFalse((game/'portal/cfg/autoexec.cfg').exists())

if __name__=='__main__':unittest.main()
