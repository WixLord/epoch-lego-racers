from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[1]
src=root/'Exports/Motorbike-3dsMax'
out=root/'Exports/Motorbike-Max-Fixed'
out.mkdir(exist_ok=True)
verts=[]; groups={}; mat='LightGrayFrame'
for line in (src/'Motorbike.obj').read_text().splitlines():
 p=line.split()
 if not p: continue
 if p[0]=='v': verts.append(tuple(map(float,p[1:])))
 elif p[0]=='o': name=p[1]; groups[name]=[]
 elif p[0]=='usemtl': mat=p[1]
 elif p[0]=='f': groups[name].append((tuple(verts[int(i)-1] for i in p[1:]),mat))
materials={'LightGrayFrame':(184,187,181),'BlackRubber':(32,32,32),'RedHub':(164,30,32),'DarkInset':(85,89,82)}
ms=['-- Adds the motorcycle only; does not clear your existing scene.','(','local parts = #()','local bikeMat = multimaterial numsubs:4 name:"Motorbike Materials"']
for i,(name,col) in enumerate(materials.items(),1):
 ms.append(f'bikeMat[{i}] = standardMaterial name:"{name}" diffuse:(color {col[0]} {col[1]} {col[2]})')
obj=['# Z up; game units. Import with welding disabled.','mtllib Motorbike.mtl']; faces=[]; offset=0
for name,tris in groups.items():
 unique={}; indices=[]; ids=[]
 for tri,mat in tris:
  face=[]
  for v in tri:
   if v not in unique: unique[v]=len(unique)+1
   face.append(unique[v])
  assert len(set(face))==3
  indices.append(face); ids.append(list(materials).index(mat)+1)
 obj += ['v '+' '.join(format(x,'.9g') for x in v) for v in unique]
 faces += ['o '+name,'g '+name,'s off']
 for face,mid in zip(indices,ids):
  faces += ['usemtl '+list(materials)[mid-1],'f '+' '.join(str(x+offset) for x in face)]
 offset+=len(unique)
 va=','.join('['+','.join(format(x,'.9g') for x in v)+']' for v in unique)
 fa=','.join('['+','.join(map(str,f))+']' for f in indices)
 ms += [f'local part = mesh name:"{name}" vertices:#({va}) faces:#({fa})','part.material = bikeMat','append parts part', 'local ids = #('+','.join(map(str,ids))+')','for f = 1 to ids.count do (setFaceMatID part f ids[f]; setFaceSmoothGroup part f 0)', 'update part']
 pivot={'Frame':'[0,0,0]','RearWheel':'[-2.7,0,1.3]','FrontWheel':'[2.7,0,1.3]'}[name]
 ms += ['part.pivot = '+pivot]
 print(name,len(unique),'vertices',len(indices),'triangles')
ms += ['select parts','max zoomext sel all',')']
(out/'CreateMotorbike.ms').write_text('\n'.join(ms))
(out/'Motorbike.obj').write_text('\n'.join(obj+faces)+'\n')
shutil.copy2(src/'Motorbike.mtl',out/'Motorbike.mtl')
shutil.copy2(src/'Preview.png',out/'Preview.png')
(out/'README.txt').write_text('Recommended: run CreateMotorbike.ms through Scripting > Run Script in 3ds Max. It adds the three editable meshes and selects/frames them without deleting existing objects. This bypasses OBJ import settings entirely.\n\nIf Motorbike.max is included, you can open that scene directly (Max 2023 or later).\n\nOBJ alternative: use Z-up, import all objects, disable welding. The geometry is only about 8 game units long; an excessive weld threshold can destroy it.\n\n600 triangles. Frame, RearWheel and FrontWheel are separate. Z up, X forward, Y wheel axle. Wheel pivots: rear (-2.7,0,1.3), front (2.7,0,1.3). Preserve scale for reimport into the game.\n')
