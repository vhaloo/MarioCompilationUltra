"""Test the same package selection, verification and activation used at startup."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

exe = Path(sys.argv[1]).resolve()
edition = sys.argv[2]
with tempfile.TemporaryDirectory(prefix="mcu-update-") as directory:
    root = Path(directory)
    def call(op="select", current="0.12.0", platform=edition, good=True):
        source = root/"installed" if op=="active" else root
        p = subprocess.run([exe,op,source,platform,current],capture_output=True)
        assert (p.returncode==0)==good, (op,p.returncode,p.stderr)
        return p.stdout.decode("utf8")
    def release(v, platform=edition):
        suffix = platform+"-public.apk" if platform in ("Wear","Quest","Android") else platform+".zip"
        name=f"MarioCompilationUltra-{v}-{suffix}"
        return dict(tag_name="v"+v,draft=False,prerelease=True,assets=[dict(name=name,state="uploaded",size=12,digest="sha256:"+"b"*64,browser_download_url=f"https://github.com/vhaloo/MarioCompilationUltra/releases/download/v{v}/{name}")])
    def feed(*items):
        (root/"feed.json").write_text(json.dumps(items),encoding="utf8")
    for platform in ("windows-x64","linux-x64","macos-x64","macos-arm64","Quest","Wear","Android"):
        good=release("0.13.0",platform);new=release("0.20.0",platform)
        feed(release("0.9.0",platform),new,good)
        assert call(platform=platform)=="0.20.0"
        for mutate in ("draft","url","digest","size","state","name","version"):
            invalid=copy.deepcopy(new)
            a=invalid["assets"][0]
            if mutate=="draft":invalid["draft"]=True
            elif mutate=="url":a["browser_download_url"]="https://example.org/other"
            elif mutate=="digest":a["digest"]="sha256:XYZ"
            elif mutate=="size":a["size"]=-1
            elif mutate=="state":a["state"]="new"
            elif mutate=="name":a["name"]+=".bak"
            else:invalid["tag_name"]="v0.99.0-preview"
            feed(invalid,good)
            assert call(platform=platform)=="0.13.0"
        feed(good)
        assert call(current="0.13.0",platform=platform)=="none"
        assert call(current="1.0.0",platform=platform)=="none"
    p=root/"package.zip"; entry={"windows-x64":"MarioCompilationUltra.exe","linux-x64":"MarioCompilationUltra"}.get(edition,"MarioCompilationUltra.app/Contents/MacOS/MarioCompilationUltra")
    def package(v,extra=None):
        rel=release(v);asset=rel["assets"][0];prefix=asset["name"][:-4]+"/"
        with zipfile.ZipFile(p,"w",zipfile.ZIP_DEFLATED) as z:
            z.writestr(prefix+entry,b"fixture")
            if extra:z.writestr(prefix+extra,b"bad")
        asset.update(size=p.stat().st_size,digest="sha256:"+hashlib.sha256(p.read_bytes()).hexdigest())
        feed(rel)
    data=root/"UserData";data.mkdir();save=data/"progress";save.write_bytes(b"existing progress")
    package("0.13.0")
    assert not call("active")
    active=Path(call("stage"))
    assert active.read_bytes()==b"fixture" and Path(call("active"))==active
    assert Path(call("stage"))==active
    assert not call("active",current="0.13.0")
    package("0.14.0","../escape")
    call("stage",good=False)
    assert Path(call("active"))==active and not (root/"escape").exists()
    package("0.14.0")
    damaged=bytearray(p.read_bytes());damaged[10]^=1;p.write_bytes(damaged)
    call("stage",good=False)
    assert Path(call("active"))==active and save.read_bytes()==b"existing progress"
    assert not list((root/"installed").glob(".pending-*"))
print("Seven editions, numeric ordering, consent-time staging, digest, ZIP bounds, fallback and profile preservation passed.")
