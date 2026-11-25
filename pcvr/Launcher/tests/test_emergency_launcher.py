import importlib.util,json,hashlib,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/frame'))
import standalone_launch
spec=importlib.util.spec_from_file_location('frame_setup',ROOT/'FrameSetup.py');setup=importlib.util.module_from_spec(spec);spec.loader.exec_module(setup)

class EmergencyLauncherTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='portal public launcher ');self.addCleanup(self.temp.cleanup)
  self.root=Path(self.temp.name);self.game=self.root/'owned game';self.game.mkdir();self.pack=self.root/'package';self.pack.mkdir()
  runtime=self.game/'bin/VR';runtime.mkdir(parents=True);(runtime/'config.txt').write_text('GunGrabRightCM=19\nKeepThisPreference=yes\n')
  (self.game/'hl2.exe').write_bytes(b'MZ CPU fixture, never executed');(self.game/'bin/d3d9.dll').write_bytes(b'fixture DLL, never loaded')
  (self.game/'steam_appid.txt').write_bytes(b'400\n\0')
  files={}
  for name in ('appid_guard.py','portal_startup_settings.py','portal_restart_helper.py'):
   data=(ROOT/'tools/frame'/name).read_bytes();(runtime/name).write_bytes(data);files['bin/VR/'+name]=hashlib.sha256(data).hexdigest()
  files['bin/d3d9.dll']=hashlib.sha256((self.game/'bin/d3d9.dll').read_bytes()).hexdigest()
  launcher={}
  for name,data in [('launcher/portal-standalone.exe',b'MZ fixture'),('tools/frame/standalone_launch.py',(ROOT/'tools/frame/standalone_launch.py').read_bytes())]:
   p=self.pack/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);launcher[name]=hashlib.sha256(data).hexdigest()
  self.manifest={'files':files,'launcher_files':launcher,'build':'emergency fixture'}
  (self.pack/'manifest.json').write_text(json.dumps(self.manifest))
  self.patches=[patch.object(setup,'ROOT',self.pack),patch.object(sys,'platform','linux'),patch.object(setup,'active')]
  for p in self.patches:p.start();self.addCleanup(p.stop)
 def test_setup_preserves_game_settings_and_marker_and_prints_local_command(self):
  before=(self.game/'bin/VR/config.txt').read_bytes();r=setup.configure(self.game)
  self.assertFalse(r['game_launched']);self.assertFalse(r['pc_required_to_play'])
  self.assertEqual((self.game/'bin/VR/config.txt').read_bytes(),before);self.assertEqual((self.game/'steam_appid.txt').read_bytes(),b'400\n\0')
  folder=Path(r['steam_shortcut']['working_directory']);self.assertFalse((folder/'steam_appid.txt').exists());self.assertNotEqual(folder,self.game)
  self.assertIn('-dx9',r['steam_shortcut']['launch_options']);self.assertIn('%command%',r['steam_shortcut']['launch_options']);self.assertIn(' --portal-root ',r['steam_shortcut']['launch_options'])
  profile=json.loads((folder/'profile.json').read_text());self.assertEqual(profile['uri'],'auto');self.assertEqual(profile['game'],str(self.game.resolve()))
  self.assertEqual(setup.configure(self.game)['status'],r['status'])
 def test_dry_run_writes_nothing(self):
  r=setup.configure(self.game,dry=True);self.assertEqual(r['status'],'validated-dry-run');self.assertFalse(self.game.with_name(self.game.name+'_FrameVR_Setup').exists())
 def test_modified_launcher_payload_rejected_before_writes(self):
  (self.pack/'launcher/portal-standalone.exe').write_bytes(b'modified')
  with self.assertRaisesRegex(RuntimeError,'checksum'):setup.configure(self.game)
  self.assertFalse(self.game.with_name(self.game.name+'_FrameVR_Setup').exists())
 def test_setup_inside_game_rejected(self):
  with self.assertRaisesRegex(RuntimeError,'separate'):setup.configure(self.game,self.game/'launch')
 def test_mismatched_profile_preserved(self):
  r=setup.configure(self.game);p=Path(r['steam_shortcut']['working_directory'])/'profile.json';data=json.loads(p.read_text());data['game']='different game';p.write_text(json.dumps(data))
  with self.assertRaisesRegex(RuntimeError,'different game'):setup.configure(self.game)
 def test_running_game_blocks_install(self):
  with patch.object(setup,'active',side_effect=RuntimeError('Close Portal yourself')):
   with self.assertRaisesRegex(RuntimeError,'Close Portal'):setup.configure(self.game)
 def test_auto_restart_identity_uses_current_shortcut_not_owned_retail_app(self):
  with patch.dict(os.environ,{'SteamGameId':'14736448987825438720','SteamAppId':'3431096903'},clear=True):self.assertEqual(standalone_launch.restart_uri({'uri':'auto'}),'steam://rungameid/14736448987825438720')
  with patch.dict(os.environ,{'SteamAppId':'3431096903'},clear=True):self.assertEqual(standalone_launch.restart_uri({'uri':'auto'}),'steam://rungameid/14736448987825438720')
  with patch.dict(os.environ,{},clear=True):self.assertIsNone(standalone_launch.restart_uri({'uri':'auto'}))
 def test_invalid_retail_marker_never_discarded(self):
  from appid_guard import archive_generated_appid
  (self.game/'steam_appid.txt').write_bytes(b'unexpected')
  with self.assertRaises(RuntimeError):archive_generated_appid(self.game,self.root/'setup')
  self.assertEqual((self.game/'steam_appid.txt').read_bytes(),b'unexpected')

if __name__=='__main__':unittest.main()
