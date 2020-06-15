import bmesh
import bpy
import os

os.system('cls')

scene = bpy.context.scene
for obj in scene.objects:
    if obj.type != 'MESH':
        continue
    print(obj.name)
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    bmesh.ops.triangulate(mesh, faces=mesh.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    for v in mesh.verts:
        print(v.index, [v for v in v.co])
    for f in mesh.faces:
        print([v.index for v in f.verts])

