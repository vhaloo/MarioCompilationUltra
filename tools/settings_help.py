"""Compile original setting help and check that every setting has a semantic topic."""
import argparse,json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def topic(key):
    if key in {'Pixel volume','Voxel thickness','Volume angle'}:return 'Volume'
    k=key.lower()
    exact={'View':'View','Quality':'Quality','Display':'Screen','VR shader':'Shader','Automatic resolution':'Quality',
        'Automatic quality':'Quality','Performance reserve':'Headroom','Extra smooth':'Headroom',
        'React to device heat':'Thermal','Round-screen fisheye':'Lens','Watch zoom':'Zoom',
        'questmode':'XRMode','questresolution':'XRDetail','questeyedetail':'XRDetail','questwalldetail':'XRDetail',
        'questadaptive':'Quality','questflip':'Screen','questrefresh':'Refresh','questar':'XREnvironment',
        'questtransparent':'XREnvironment','questfoveation':'Foveation','questanchor':'XRAnchor',
        'questremovesaved':'XRAnchor','questpin':'XRAnchor','questautowall':'XRAnchor','questwalllock':'XRAnchor',
        'questpauselost':'Pause','questhandtracking':'XRControls','questtouch':'XRControls',
        'questhaptics':'XRControls','questhandjoystick':'XRControls','questremote':'XRControls',
        'questremotescale':'XRControls','questcontrolhints':'XRControls','questinsetcrt':'XRCrt',
        'questcoplanar':'XRCrt','questworldbackdrop':'XREnvironment','questsurround':'XREnvironment',
        'questsurroundbrightness':'XREnvironment','questenvironment':'XREnvironment','questoptionsscale':'Interface',
        'questcompanion':'XRControls','questmenudistance':'XRPlacement','Display status':'Hints',
        'Borderless fullscreen':'Screen','Span all displays':'Screen','Pause when unfocused':'Pause',
        'Automatic resume':'Save','Minimap':'Locator','Player locator':'Locator','Native camera guide':'Locator',
        'Live close-up in atlas':'Locator','Show full level map':'View','Show stage atlas':'View',
        'Performance overlay':'Headroom','Volume':'Audio','Mute':'Audio','Screen orientation':'Screen'}
    if key=='Manual quality':return 'Manual'
    if key in exact:return exact[key]
    if key in {'Quit','Resume','Restart cartridge'}:return 'Pause'
    if key in {'Close','Back','Yes','No','Confirm','Cancel','Done'}:return 'Help'
    if key=='Exact pixel 1:1':return 'Zoom'
    if key=='Original framing':return 'View'
    if key=='remote_recover':return 'XRControls'
    if key in {'Play','Load','Save','Clear slot'}:return 'Save' if key!='Play' else 'Library'
    if 'crown' in k or 'bezel' in k:return 'Crown'
    if 'zoom' in k or 'panorama height' in k:return 'Zoom'
    if 'aspect' in k:return 'Aspect'
    if 'filter' in k or 'reduced map' in k:return 'Filter'
    if 'shader' in k:return 'Shader'
    if 'quality' in k:return 'Quality'
    if 'smooth' in k and 'extra' in k:return 'Headroom'
    if 'sync' in k or 'refresh' in k:return 'Refresh'
    if 'blur' in k or k=='brightness':return 'Blur'
    if 'edge' in k:return 'Edges'
    if 'backdrop' in k or k=='background':return 'Backdrop'
    if 'camera' in k or 'follow' in k or 'freeze' in k:return 'Camera'
    if 'preview' in k or 'marker' in k:return 'Preview'
    if 'hint' in k or 'notification' in k or 'status' in k:return 'Hints'
    if 'language' in k or 'interface' in k or 'contrast' in k:return 'Interface'
    if 'help' in k:return 'Help'
    if 'rewind' in k or 'reverse sound' in k:return 'Rewind'
    if 'touch' in k or 'vibrat' in k or 'haptic' in k or 'handed' in k or 'button size' in k:return 'Touch'
    if 'orientation' in k or 'awake' in k:return 'Screen'
    if 'pad' in k or 'dead zone' in k or 'controls' in k:return 'Controls'
    if 'sound' in k or 'volume' in k:return 'Audio'
    if 'save' in k or 'slot' in k or 'resume' in k:return 'Save'
    if 'pause' in k:return 'Pause'
    if 'crt' in k or 'glass' in k or 'television' in k or 'scanline' in k:return 'XRCrt'
    if k.startswith('quest'):return 'XRPlacement'
    if 'presentation' in k:return 'XRCrt'
    if 'map' in k or 'picture' in k or 'view' in k:return 'View'
    if any(word in k for word in ['open','import','choose','cartridge','folder','project','path','backspace']):return 'Library'
    raise ValueError('Assign a specific help topic: '+key)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args()
    data=json.loads((R/'assets/settings-help.json').read_text(encoding='utf8'));topics={t['id']:t for t in data['topics']}
    keys=set()
    for name in ['src/app.cpp','src/ui_features.cpp','src/mobile_ui.cpp','src/watch_ui.cpp','src/quest.cpp']:
        s=(R/name).read_text(encoding='utf8')
        keys.update(re.findall(r'\b(?:choice|tick|slider|combo|check|settingTitle|button)\(\s*"([^"\n]+)"',s))
    keys.update(['Pixel zoom','Display','Language','Automatic quality','Performance reserve','React to device heat','questfoveation'])
    entries=[('Help',topics['Help'])]+[(key,topics[topic(key)]) for key in sorted(keys)]
    quote=lambda s:json.dumps(s,ensure_ascii=False)
    content='// Generated by tools/settings_help.py; original diagrams and explanations.\nstatic const SettingHelp HelpEntries[] = {\n'
    content+=''.join('    {'+quote(k)+', HelpPicture::'+t['picture']+', '+quote(t['text']['en'])+'},\n' for k,t in entries)+'};\n'
    target=R/'src/setting_help.inc'
    if a.check:
        assert target.read_text(encoding='utf8')==content,'Regenerate setting help'
        for t in topics.values():
            assert set(t['text'])=={'en','fr','es','ja','de','it','pt','ru','zh','ko'},'Translate topic '+t['id']
    else:
        target.write_text(content,encoding='utf8',newline='\n')
        languages=json.loads((R/'assets/locales/languages.json').read_text(encoding='utf8'))
        for language in languages:
            p=R/'assets/locales'/(language['code']+'.json');values=json.loads(p.read_text(encoding='utf8'))
            for t in topics.values():values[t['text']['en']]=t['text'].get(language['code'],t['text']['en'])
            p.write_text(json.dumps(values,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    print(len(keys),'setting keys,',len(topics),'semantic illustrated topics')
if __name__=='__main__':main()
