"""Extract the authored Batman FBX with split normals for the game's cowl library.
Run with Blender --background --python Tools/Export-BatmanCowl.py.
"""
from pathlib import Path
import bpy,json
root=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'Assets/Batman/BatmanCowl.fbx'),global_scale=100,use_custom_normals=True)
objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
assert len(objects)==1,'Expected one authored cowl mesh'
ob=objects[0];me=ob.data;me.calc_loop_triangles()
normal_matrix=ob.matrix_world.to_3x3().inverted().transposed()
triangles=[]
for tri in me.loop_triangles:
 corners=[]
 for vi,li in zip(tri.vertices,tri.loops):
  p=ob.matrix_world@me.vertices[vi].co
  n=(normal_matrix@me.corner_normals[li].vector).normalized()
  # Max OBJ import rotated the original Z-up mesh into -Y-up.
  corners.append({'position':[p.x,p.z,-p.y],'normal':[n.x,n.z,-n.y]})
 triangles.append(corners)
(root/'Assets/Batman/cowl-mesh.json').write_text(json.dumps({'triangles':triangles},indent=2))
print('Exported',len(triangles),'triangles; custom normals:',me.has_custom_normals)
