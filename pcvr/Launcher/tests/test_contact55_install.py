"""Temporary CPU fixtures only; no game, graphics, network or headset activity."""
import importlib.util,hashlib,json,os,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/frame'))
spec=importlib.util.spec_from_file_location('public55_install',ROOT/'Install.py');installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)
class InstallerTests(unittest.TestCase):
 def setUp(self):
  temp=tempfile.TemporaryDirectory(prefix='portal55 CPU ');self.addCleanup(temp.cleanup);self.root=Path(temp.name);self.pack=self.root/'package';self.game=self.root/'game'
  (self.game/'portal/cfg').mkdir(parents=True);(self.game/'hl2.exe').write_bytes(b'CPU fixture; never executed')
  (self.game/'bin/VR').mkdir(parents=True);self.config=self.game/'bin/VR/config.txt';self.config.write_bytes(b'#owner comment\nHandSizePercent=77\nRenderWindow=0\n')
  self.dll=self.game/'bin/d3d9.dll';self.dll.write_bytes(b'old DLL fixture');self.auto=self.game/'portal/cfg/autoexec.cfg';self.auto.write_bytes(b'//owner autoexec\n')
  data={'bin/d3d9.dll':b'new DLL fixture','bin/VR/config.txt':b'HandSizePercent=100\nRenderWindow=1\nFirmSupportGrip=true\n'}
  for n,v in data.items():p=self.pack/'payload'/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(v)
  (self.pack/'manifest.json').write_text(json.dumps(dict(version='Contact55 CPU',files={n:hashlib.sha256(v).hexdigest() for n,v in data.items()})))
 def generate(self,game,output):
  p=output/'models/weapons';p.mkdir(parents=True)
  for ext in ('mdl','vvd','dx90.vtx'):(p/('v_portalgun.'+ext)).write_bytes(('synthetic '+ext).encode())
  (output/'gun-shell51.bin').write_bytes(b'synthetic local shell; not game data')
 def run_install(self,dry=False):
  with patch.object(installer,'ROOT',self.pack),patch.object(installer,'active'),patch.object(installer,'build',self.generate):return installer.install(self.game,dry)
 def test_dry_run_does_not_modify_game_or_make_backup(self):
  before={str(p.relative_to(self.game)):p.read_bytes() for p in self.game.rglob('*') if p.is_file()};r=self.run_install(True)
  self.assertEqual(r['status'],'validated-dry-run');self.assertEqual(before,{str(p.relative_to(self.game)):p.read_bytes() for p in self.game.rglob('*') if p.is_file()})
 def test_install_generates_shell_transaction_and_preserves_owner_preferences(self):
  r=self.run_install();self.assertEqual(r['status'],'installed');self.assertFalse(r['game_launched']);text=self.config.read_text();self.assertIn('HandSizePercent=77',text);self.assertIn('RenderWindow=0',text);self.assertIn('#owner comment',text);self.assertIn('FirmSupportGrip=true',text)
  self.assertEqual(self.dll.read_bytes(),b'new DLL fixture');self.assertEqual((self.game/'bin/VR/gun-shell51.bin').read_bytes(),b'synthetic local shell; not game data');self.assertTrue(self.auto.read_bytes().startswith(b'//owner autoexec\n'))
  journal=json.loads((Path(r['backup'])/'installation.json').read_text());self.assertIsNone(journal['before']['bin/VR/gun-shell51.bin']);self.assertEqual(journal['status'],'installed')
 def test_late_shell_write_failure_restores_dll_config_and_models(self):
  original=os.replace;shell=self.game/'bin/VR/gun-shell51.bin'
  def fail(a,b):
   if Path(b)==shell:raise OSError('synthetic shell write failure')
   return original(a,b)
  with patch.object(os,'replace',side_effect=fail),self.assertRaises(OSError):self.run_install()
  self.assertEqual(self.dll.read_bytes(),b'old DLL fixture');self.assertEqual(self.config.read_bytes(),b'#owner comment\nHandSizePercent=77\nRenderWindow=0\n');self.assertFalse(shell.exists());self.assertFalse(any((self.game/'portal/custom').rglob('*.mdl')))
 def test_existing_shell_is_backed_up_and_restored_on_failed_update(self):
  shell=self.game/'bin/VR/gun-shell51.bin';shell.write_bytes(b'old local shell');real=os.replace
  def fail(a,b):
   if Path(b)==shell and Path(a).read_bytes()!=b'old local shell':raise OSError('synthetic shell lock')
   return real(a,b)
  with patch.object(os,'replace',side_effect=fail),self.assertRaises(OSError):self.run_install()
  self.assertEqual(shell.read_bytes(),b'old local shell');self.assertEqual(self.dll.read_bytes(),b'old DLL fixture')
 def test_active_session_refuses_before_generation(self):
  with patch.object(installer,'active',side_effect=RuntimeError('Close Portal yourself')),patch.object(installer,'build') as generate,self.assertRaises(RuntimeError):installer.install(self.game)
  generate.assert_not_called();self.assertEqual(self.dll.read_bytes(),b'old DLL fixture')
 def test_payload_tamper_refuses_all_writes(self):
  (self.pack/'payload/bin/d3d9.dll').write_bytes(b'tampered')
  with self.assertRaisesRegex(RuntimeError,'checksum mismatch'):self.run_install()
  self.assertEqual(self.dll.read_bytes(),b'old DLL fixture')
 def test_concurrent_generated_asset_edit_is_preserved(self):
  shell=self.game/'bin/VR/gun-shell51.bin';real=os.replace;changed=[]
  def change(a,b):
   result=real(a,b)
   if Path(b)==self.dll and not changed:changed.append(True);shell.write_bytes(b'newer owner shell')
   return result
  with patch.object(os,'replace',side_effect=change),self.assertRaisesRegex(RuntimeError,'changed during installation'):self.run_install()
  self.assertEqual(shell.read_bytes(),b'newer owner shell');self.assertEqual(self.dll.read_bytes(),b'old DLL fixture')
if __name__=='__main__':unittest.main()
