import json,hashlib,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import PCVRLauncher as launcher
class PcvrTests(unittest.TestCase):
 def setUp(self):
  self.t=tempfile.TemporaryDirectory(prefix='portal pcvr ');self.addCleanup(self.t.cleanup);self.game=Path(self.t.name)
  (self.game/'hl2.exe').write_bytes(b'CPU fixture only');(self.game/'portal').mkdir();(self.game/'bin/VR').mkdir(parents=True)
  (self.game/'bin/d3d9.dll').write_bytes(b'fixture');(self.game/'bin/VR/config.txt').write_text('HandSizePercent=76\nGunGrabRightCM=-2\nRenderWindow=0\n')
  self.manifest={'build':'fixture','files':{'bin/d3d9.dll':hashlib.sha256(b'fixture').hexdigest()}}
 def test_play_arguments_are_native_dx9_and_have_no_shutdown_timer(self):
  args=launcher.command(self.game);self.assertIn('-dx9',args);self.assertEqual(args[0],str(self.game/'hl2.exe'))
  self.assertFalse(any('timeout' in s or 'quit' in s for s in args));self.assertNotIn('+playdemo',args)
 def test_replay_name_is_validated_and_must_exist(self):
  for value in ('../test','portalvr_take_x;quit','portalvr_take_x"bad','other_take'):
   with self.assertRaises(RuntimeError):launcher.command(self.game,value)
  take='portalvr_take_123_456';(self.game/'portal'/(take+'.dem')).write_bytes(b'fixture');self.assertEqual(launcher.command(self.game,take)[-2:],['+playdemo',take])
 def test_diagnostics_are_read_only_and_distinguish_runtime_readiness(self):
  before=(self.game/'bin/VR/config.txt').read_bytes()
  with patch.object(launcher,'names',return_value={'steam.exe','vrserver.exe','vrcompositor.exe'}):r=launcher.check(self.game,self.manifest)
  self.assertTrue(r['dll_matches'] and r['steamvr_ready']);self.assertFalse(r['game_launch_authorized']);self.assertIsNone(r['game_lifetime_limit']);self.assertEqual((self.game/'bin/VR/config.txt').read_bytes(),before)
 def test_pc_profile_backs_up_and_preserves_grip_body_values(self):
  before=(self.game/'bin/VR/config.txt').read_bytes();archive=launcher.defaults(self.game);after=(self.game/'bin/VR/config.txt').read_text()
  self.assertEqual((Path(archive)/'config.txt.before').read_bytes(),before);self.assertIn('RenderWindow=1',after);self.assertIn('HandSizePercent=76',after);self.assertIn('GunGrabRightCM=-2',after)
 def test_no_automatic_termination_or_upload_implementation(self):
  source=Path(launcher.__file__).read_text();self.assertNotIn('.kill(',source);self.assertNotIn('.terminate(',source);self.assertIn('code=child.wait()',source);self.assertNotIn('requests.post',source);self.assertNotIn('timeout=',source)
if __name__=='__main__':unittest.main()
