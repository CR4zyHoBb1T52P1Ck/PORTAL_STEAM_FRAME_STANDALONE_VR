"""Generate private Portal VR mesh overrides from the user's retail VPK files.

Usage: python generate_owned_assets.py <Portal install folder> <new output folder>
Outputs contain derived Valve game assets; distribute the generator, not output.
No rendering, game launch, network access or changes to the original install.
"""
import argparse,hashlib,json
from pathlib import Path
from vpk_index import entries,read
from complete_portal_gun import complete
from generate_native_hands import generate
from generate_gunshell51 import generate as gun_shell

def build(game,output):
    game=Path(game);output=Path(output)
    assert (game/'portal').is_dir() and not output.exists(),'Use an owned Portal install and a new output directory'
    assert not output.resolve().is_relative_to(game.resolve()),'Generate outside the game install; install only after Portal is closed'
    wanted={f'models/weapons/{name}.{ext}' for name in ('v_portalgun','w_portalgun','v_hands') for ext in ('mdl','vvd','dx90.vtx')}
    source={}
    for group in ('hl2','portal'):
        for pack in sorted((game/group).glob('*_dir.vpk')):
            for name,item in entries(pack).items():
                if name in wanted:source[name]=read(pack,item)
    assert set(source)==wanted,'Retail model tuple is incomplete'
    def model(name):return tuple(source[f'models/weapons/{name}.{ext}'] for ext in ('mdl','vvd','dx90.vtx'))
    gun,gun_receipt=complete(model('v_portalgun'),model('w_portalgun'))
    hands,hands_receipt=generate(model('v_portalgun'),model('v_hands'))
    shell,shell_receipt=gun_shell(gun)
    # All validation finishes before an output folder is created.
    target=output/'models/weapons';target.mkdir(parents=True)
    for name,files in (('v_portalgun',gun),('v_hands',hands)):
        for ext,data in zip(('mdl','vvd','dx90.vtx'),files):(target/(name+'.'+ext)).write_bytes(data)
    (output/'gun-shell51.bin').write_bytes(shell)
    receipt=dict(shell=shell_receipt,gun=gun_receipt,hands=hands_receipt,input_sha256={k:hashlib.sha256(v).hexdigest() for k,v in source.items()},
        output_sha256={str(p.relative_to(output)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in target.iterdir()},
        game_launched=False,original_install_changed=False)
    (output/'generation.json').write_text(json.dumps(receipt,indent=2)+'\n');return receipt

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('output');a=p.parse_args()
    print(json.dumps(build(a.game,a.output),indent=2))
