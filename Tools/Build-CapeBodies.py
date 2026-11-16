"""Create torso-bound cape variants with matching skeleton and face placeholder."""
import json,copy,struct
from pathlib import Path
src=Path('Tools/Build-DriverDetail.py').read_text();exec(src[src.index('def tokens('):src.index('def skeleton(')])
data=json.load(open('Build/bat-bodies.json',encoding='utf-8-sig'));output=Path('Assets/BatmanBuild/MENUDATA/PARTDB')
for name in data:
 faces,scale=mesh(name)
 # The chest print belongs to the torso's rigid skeleton node.
 nodes=[node for m,c in faces if m=='torso' for v,node in c if v[3]>.12]
 if not nodes:continue
 # The menu chest also has a thin collar joint. Attach to the full torso,
 # not whichever joint happens to have the most printed triangle corners.
 node=max(set(nodes),key=lambda k: max(v[2] for m,c in faces if m=='torso' for v,n in c if n==k)-min(v[2] for m,c in faces if m=='torso' for v,n in c if n==k))
 vs=[v for m,c in faces if m=='torso' for v,n in c if n==node]
 if name.endswith('CHESTS.GDB'):
  # Selector torso includes the arms on node zero; size the cape to the chest print.
  vs=[v for m,c in faces if m=='torso' for v,n in c if v[3]>.12]
 x=min(v[0] for v in vs)-5;z0=min(v[2] for v in vs);z1=max(v[2] for v in vs)-5
 w=max(abs(v[1]) for v in vs)*.95
 points=[(x,-w,z1),(x,w,z1),(x-34,w*1.8,z0-82),(x-34,w*.85,z0-62),(x-34,w*.45,z0-90),(x-34,0,z0-68),(x-34,-w*.45,z0-90),(x-34,-w*.85,z0-62),(x-34,-w*1.8,z0-82)]
 # Keep the collar fit; flare the hem 25% wider and extend the drop 20%.
 points=points[:2]+[(px,py*1.25,z1+(pz-z1)*1.20) for px,py,pz in points[2:]]
 cape=[]
 for i in range(1,len(points)-1):
  for inds in [(0,i,i+1),(0,i+1,i)]:
   verts=[]
   for k in inds:verts.append(([ *points[k],.039,.30, -1 if inds[1]<inds[2] else 1,0,0],node))
   cape.append(('torso',verts))
 # Keep the face at the end: the runtime replaces that final group with the hat.
 faces=[f for f in faces if f[0]!='face']+cape+[f for f in faces if f[0]=='face']
 p=Path(name);dest=p.parent/('C'+p.name)
 write_mesh(str(dest),faces,scale,rigid=name.endswith('CHESTS.GDB'))
 print(dest,len(cape),'cape triangles')

# Expose the caped torso as its own selector entity, keeping other torsos untouched.
t=[(x['Type'],x['Value']) for x in json.load(open('Build/bat-selector.json',encoding='utf-8-sig'))]
new_index=t[2][1]
t[2]=(4,new_index+1)
t.insert(5+new_index,(2,'cchests'))
start=next(i for i in range(len(t)-1) if t[i][0]==46 and t[i+1][0]==7)
t[start+2]=(4,t[start+2][1]+1)
i=next(i for i in range(len(t)-1) if t[i][0]==46 and t[i+1]==(2,'chests'))
end=i+3
while t[end][0]!=6:end+=1
block=list(t[i:end+1]);block[1]=(2,'cchests')
index=next(i for i,x in enumerate(block) if x[0]==42)
block[index+1]=(4,new_index)
t[-1:-1]=block
write('MENUPART/SELECTOR.WDB',t)
