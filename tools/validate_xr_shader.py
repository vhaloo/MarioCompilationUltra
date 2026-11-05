"""Exercise the production GLES shaders with original procedural pixels on a real GL context.

Requires a GLES 3 driver, SDL2 and Pillow. This is GPU/visual regression evidence,
not headset qualification. The --baseline option accepts a previous xr_stage.cpp.
"""
from pathlib import Path
import argparse,ctypes as C,json,re,sys,time,statistics
from PIL import Image,ImageChops

def main():
 p=argparse.ArgumentParser();p.add_argument('--sdl',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--baseline',type=Path);a=p.parse_args()
 root=Path(__file__).resolve().parents[1];a.out.mkdir(parents=True,exist_ok=True)
 s=C.CDLL(str(a.sdl.resolve()))
 def bind(name,ret,args):
  f=getattr(s,name);f.restype=ret;f.argtypes=args;return f
 U=C.c_uint;I=C.c_int;F=C.c_float;P=C.c_void_p
 init=bind('SDL_Init',I,[U]);attr=bind('SDL_GL_SetAttribute',I,[I,I]);win=bind('SDL_CreateWindow',P,[C.c_char_p,I,I,I,I,U]);context=bind('SDL_GL_CreateContext',P,[P]);proc=bind('SDL_GL_GetProcAddress',P,[C.c_char_p]);error=bind('SDL_GetError',C.c_char_p,[])
 assert init(32)==0
 for k,v in [(21,4),(17,3),(18,0)]:assert attr(k,v)==0
 width,height=1280,720;window=win(b'MCU original shader QA',0,0,width,height,2|8);ctx=context(window);assert ctx,error().decode()
 factory=C.WINFUNCTYPE if sys.platform=='win32' else C.CFUNCTYPE
 def gl(name,ret,*args):
  address=proc(name.encode());assert address,name;return factory(ret,*args)(address)
 getString=gl('glGetString',C.c_char_p,U)
 result={k:getString(v).decode() for k,v in dict(renderer=0x1f01,version=0x1f02).items()}
 create=gl('glCreateShader',U,U);setSource=gl('glShaderSource',None,U,I,C.POINTER(C.c_char_p),C.POINTER(I));compileShader=gl('glCompileShader',None,U);getiv=gl('glGetShaderiv',None,U,U,C.POINTER(I));log=gl('glGetShaderInfoLog',None,U,I,C.POINTER(I),C.c_char_p)
 def program(path):
  text=path.read_text(encoding='utf8');sources=re.findall(r'const char \*(vertexSource|fragmentSource) = R"GLSL\((.*?)\)GLSL";',text,re.S);assert len(sources)==2
  compiled=[]
  for name,src in sources:
   shader=create(0x8b31 if name=='vertexSource' else 0x8b30);raw=C.c_char_p(src.encode());setSource(shader,1,C.byref(raw),None);compileShader(shader);ok=I();getiv(shader,0x8b81,C.byref(ok));buf=C.create_string_buffer(8192);log(shader,len(buf),None,buf);assert ok.value,(name,buf.value.decode());compiled.append(shader)
  q=gl('glCreateProgram',U)();attach=gl('glAttachShader',None,U,U)
  for shader in compiled:attach(q,shader);gl('glDeleteShader',None,U)(shader)
  gl('glLinkProgram',None,U)(q);ok=I();gl('glGetProgramiv',None,U,U,C.POINTER(I))(q,0x8b82,C.byref(ok));assert ok.value
  return q
 current=program(root/'src/xr_stage.cpp');baseline=program(a.baseline) if a.baseline else current
 gl('glViewport',None,I,I,I,I)(0,0,width,height)
 vao=U();gl('glGenVertexArrays',None,I,C.POINTER(U))(1,C.byref(vao));gl('glBindVertexArray',None,U)(vao)
 vertices=[]
 for x,y in [(-1,-1),(1,-1),(1,1),(-1,-1),(1,1),(-1,1)]:vertices += [x,y,0,0,0,1,(x+1)/2,(y+1)/2,.42,.62,.83,1,.4]
 raw=(F*len(vertices))(*vertices);buffer=U();gl('glGenBuffers',None,I,C.POINTER(U))(1,C.byref(buffer));gl('glBindBuffer',None,U,U)(0x8892,buffer);gl('glBufferData',None,U,C.c_ssize_t,P,U)(0x8892,C.sizeof(raw),raw,0x88e4)
 offset=0
 for i,size in enumerate([3,3,2,4,1]):
  gl('glEnableVertexAttribArray',None,U)(i);gl('glVertexAttribPointer',None,U,I,U,U,I,P)(i,size,0x1406,0,52,P(offset));offset+=size*4
 texture=U();gl('glGenTextures',None,I,C.POINTER(U))(1,C.byref(texture));gl('glBindTexture',None,U,U)(0xde1,texture)
 pixels=bytearray()
 for y in range(224):
  for x in range(256):pixels.extend((x,(x//16*97+y//16*43)%256,y,255))
 image=(C.c_ubyte*len(pixels)).from_buffer(pixels);gl('glTexImage2D',None,U,I,I,I,I,I,U,U,P)(0xde1,0,0x1908,256,224,0,0x1908,0x1401,image)
 for k,v in [(0x2801,0x2601),(0x2800,0x2601),(0x2802,0x812f),(0x2803,0x812f)]:gl('glTexParameteri',None,U,U,I)(0xde1,k,v)
 uniform=gl('glGetUniformLocation',I,U,C.c_char_p);u1i=gl('glUniform1i',None,I,I);u1f=gl('glUniform1f',None,I,F);u2=gl('glUniform2f',None,I,F,F);u3=gl('glUniform3f',None,I,F,F,F);u4=gl('glUniform4f',None,I,F,F,F,F);matrix=gl('glUniformMatrix4fv',None,I,I,U,C.POINTER(F));use=gl('glUseProgram',None,U)
 identity=(F*16)(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
 def setup(program,kind,crt,mask,srgb,efficient):
  use(program);at=lambda n:uniform(program,n.encode())
  for n,v in dict(image=0,kind=kind,crt=crt,maskMode=mask,srgbTarget=srgb,flipImage=0,clipNative=1,efficientShader=efficient).items():u1i(at(n),v)
  for n,v in dict(intensity=.8,glassStrength=.35,surroundBrightness=.9).items():u1f(at(n),v)
  u2(at('raster'),256,224);u3(at('eye'),0,1,3);u4(at('nativeHole'),.3,.2,.15,.2)
  for n in ['vp','model']:matrix(at(n),1,0,identity)
 draw=gl('glDrawArrays',None,U,I,I);clear=gl('glClear',None,U);finish=gl('glFinish',None);read=gl('glReadPixels',None,I,I,I,I,U,U,P)
 gl('glClearColor',None,F,F,F,F)(.03,.04,.07,1)
 def capture():
  clear(0x4000);draw(4,0,6);data=(C.c_ubyte*(width*height*4))();read(0,0,width,height,0x1908,0x1401,data);return Image.frombytes('RGBA',(width,height),bytes(data))
 records=[];maximum=0
 for kind in range(7):
  for crt in range(4) if kind==2 else [0]:
   for mask in range(3) if kind==2 else [0]:
    for srgb in [0,1]:
     setup(baseline,kind,crt,mask,srgb,0);ref=capture();setup(current,kind,crt,mask,srgb,1);got=capture();delta=max(high for low,high in ImageChops.difference(ref,got).getextrema());maximum=max(maximum,delta);assert delta<=2,(kind,crt,mask,srgb,delta)
     records.append(dict(kind=kind,crt=crt,mask=mask,srgb=srgb,max_channel_difference=delta))
     if kind==2 and crt==2 and mask==1 and not srgb:ref.save(a.out/'reference-original-pixels.png');got.save(a.out/'efficient-original-pixels.png')
 times={'baseline':[],'efficient':[]}
 for _ in range(5):
  for label,program,efficient in [('baseline',baseline,0),('efficient',current,1)]:
   setup(program,2,2,1,1,efficient)
   for n in range(30):draw(4,0,6)
   finish();t=time.perf_counter()
   for n in range(120):draw(4,0,6)
   finish();times[label].append((time.perf_counter()-t)*1000/120)
 result.update(result='passed',headset_tested=False,cases=len(records),max_channel_difference=maximum,records=records,fullscreen_shader_ms={k:statistics.median(v) for k,v in times.items()},samples_ms=times)
 (a.out/'proof.json').write_text(json.dumps(result,indent=2),encoding='utf8');print(json.dumps({k:v for k,v in result.items() if k not in ['records','samples_ms']}))
 bind('SDL_GL_DeleteContext',None,[P])(ctx);bind('SDL_DestroyWindow',None,[P])(window);bind('SDL_Quit',None,[])()
if __name__=='__main__':main()
