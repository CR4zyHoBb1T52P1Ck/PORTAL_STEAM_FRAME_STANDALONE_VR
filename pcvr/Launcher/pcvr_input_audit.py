"""Semantic Index coverage checks. Tracking alone is not input readiness."""
import json
from pathlib import Path
BASE='/actions/base/in/'
REQUIRED_BASE={BASE+x for x in ('MenuTriggerLeft','MenuTriggerRight','MenuGripLeft','MenuGripRight','MenuStickLeft','MenuStickRight','MenuStickClickLeft','MenuStickClickRight','MenuEscape','MenuToggle','MenuDpadUp','MenuDpadDown','MenuDpadLeft','MenuDpadRight','support_lefthand','support_righthand')}
REQUIRED_GAME=('Walk','Turn','PrimaryAttack','SecondaryAttack','Jump','Crouch','Use','MenuSelect','MenuBack','Pause')

def audit(manifest,binding,profile=None):
 errors=[];declared={a['name'].lower():a['type'] for a in manifest['actions']};bound={};count=0
 if binding.get('controller_type')!='knuckles':errors.append('Index controller_type must be knuckles.')
 for aset,group in binding.get('bindings',{}).items():
  for src in group.get('sources',[]):
   if profile is not None:
    component='/input/'+src.get('path','').split('/input/')[-1]
    if component not in profile.get('input_source',{}):errors.append('Unsupported Index component: '+component)
   for channel,value in src.get('inputs',{}).items():
    output=value.get('output','');kind=declared.get(output.lower());count+=1;bound.setdefault(output.lower(),[]).append(src)
    if not output.lower().startswith(aset.lower()+'/'):errors.append('Action set mismatch: '+output)
    if kind is None:errors.append('Undeclared action: '+output)
    if channel=='position' and kind!='vector2':errors.append('Position must target vector2: '+output)
    if channel!='position' and kind!='boolean':errors.append('Button must target boolean: '+output)
 for name in sorted(REQUIRED_BASE):
  if name.lower() not in bound:errors.append('Missing required Index base action: '+name)
 for aset in ('main','left_handed'):
  for name in REQUIRED_GAME:
   path='/actions/'+aset+'/in/'+name
   if path.lower() not in bound:errors.append('Missing required Index game action: '+path)
 for side in ('Left','Right'):
  name=(BASE+'MenuTrigger'+side).lower()
  for src in bound.get(name,[]):
   if src['path']!='/user/hand/'+side.lower()+'/input/trigger':errors.append('Physical trigger mismatch: '+name)
 return dict(valid=not errors,errors=errors,required_base_actions=len(REQUIRED_BASE),source_outputs=count,controller_type=binding.get('controller_type'))

def installed(game):
 root=Path(game)/'bin/VR/SteamVRActionManifest'
 try:
  manifest=json.loads((root/'action_manifest.json').read_text());ref=next(b['binding_url'] for b in manifest['default_bindings'] if b['controller_type']=='knuckles');file=root/ref
  if not file.resolve().is_relative_to(root.resolve()) or file.is_symlink():raise ValueError('Unsafe binding path')
  result=audit(manifest,json.loads(file.read_text()));result['selected_file']=ref;return result
 except (OSError,ValueError,TypeError,KeyError,StopIteration):return dict(valid=False,errors=['Index binding/manifest missing, invalid or unreadable.'])
