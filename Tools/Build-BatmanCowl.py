"""Import the authored FBX cowl to the existing interchangeable head library."""
import json,struct,copy,math
from pathlib import Path
j=json.load(open('Build/bat-hats.json',encoding='utf-8-sig'))
def T(k,v=None):return {'Type':k,'Value':v}
def section(k,values,count):return [T(k),T(7),T(4,count),T(8),T(5)]+values+[T(6)]
def end(t,start):
 p=start+3;depth=1
 while depth:depth+=(t[p]['Type']==5)-(t[p]['Type']==6);p+=1
 return p
for name in ['MENUPART/CBBODIES.GCB','GAMEPART/ICB_CHAR.GCB']:
 t=j[name];n=t[2]['Value'];v=[];tris=[]
 # Preserve the FBX corner normals: split vertices only at authored hard edges.
 cache={}
 mesh=json.loads(Path('Assets/Batman/cowl-mesh.json').read_text())
 for tri in mesh['triangles']:
  for corner in tri:
   # The corrected FBX owns the fit; do not expand the authored shell.
   point=tuple(round(x) for x in corner['position'])
   normal=corner['normal']
   length=math.sqrt(sum(x*x for x in normal))
   normal=tuple(round(x/length*127)&255 for x in normal)
   key=point+normal
   if key not in cache:
    cache[key]=n+len(v);v.append([T(4,x) for x in (*point,0,0,*normal)])
   tris.append(T(4,cache[key]))
 assert len(v)<450 and len(tris)//3<400
 s=next(i for i,x in enumerate(t[:-1]) if x['Type']==44 and t[i+1]['Type']==7);p=s+5
 def find(name):return next(i for i in range(p,len(t)-1) if t[i]['Type']==44 and t[i+1]['Value']==name)
 ear= [T(43),T(5),T(39),T(2,'bt_cowl')]+section(42,tris,len(tris)//3)+[T(6)]
 for source,dest in [('head','bathead'),('nohat','batcowl')]:
  i=find(source);block=copy.deepcopy(t[i:end(t,i)]);block[1]=T(2,dest)
  a=next(i for i,x in enumerate(block[:-1]) if x['Type']==43 and block[i+1]['Type']==7)
  b=a+5;depth=1
  while depth:depth+=(block[b]['Type']==5)-(block[b]['Type']==6);b+=1
  block[a+2]['Value']+=1;block[b-1:b-1]=ear
  t[-1:-1]=block;t[s+2]['Value']+=1
 t[5+n*8:5+n*8]=[x for row in v for x in row];t[2]['Value']+=len(v)
 path=Path('Assets/BatmanBuild/MENUDATA/PARTDB')/name
 with path.open('wb') as f:
  for x in t:
   k,x=x['Type'],x['Value'];f.write(bytes([k]))
   if k==2:f.write(x.encode()+b'\0')
   elif k==3:f.write(struct.pack('<f',x))
   elif k==4:f.write(struct.pack('<i',x))
t=j['BODYPART.PCB']
for typ,values in [(41,[T(2,'batcowl'),T(4,0)]),(45,[T(2,'bathead')])]:
 i=next(i for i,x in enumerate(t) if x['Type']==typ);p=i+5
 while t[p]['Type']!=6:p+=1
 t[p:p]=values;t[i+2]['Value']+=1
with Path('Assets/BatmanBuild/MENUDATA/PARTDB/BODYPART.PCB').open('wb') as f:
 for x in t:
  k,v=x['Type'],x['Value'];f.write(bytes([k]))
  if k==2:f.write(v.encode()+b'\0')
  elif k==3:f.write(struct.pack('<f',v))
  elif k==4:f.write(struct.pack('<i',v))
print('Appended full Batman cowl:',len(tris)//3,'triangles; hat index 36.')
