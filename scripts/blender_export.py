import bmesh
import bpy
from math import radians
from mathutils import Matrix
import os
import struct

os.system('cls')
filepath = '{}.nnm'.format(os.path.splitext(bpy.data.filepath)[0])
y_up_rot = Matrix.Rotation(radians(-90.0), 4, 'X')

with open(filepath, 'wb') as file:
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue

        mesh_name = obj.name.encode('utf-8')
        file.write(struct.pack('<B', len(mesh_name)))
        file.write(mesh_name)

        mesh = bmesh.new()
        mesh.from_mesh(obj.data)
        mesh.transform(y_up_rot)
        bmesh.ops.triangulate(mesh, faces=mesh.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')

        file.write(struct.pack('<H', len(mesh.verts)))

        for v in mesh.verts:
            if not v.is_valid or v.is_wire:
                continue

            file.write(struct.pack('<fff', *v.co.xyz))
            file.write(struct.pack('<fff', *v.normal))

        file.write(struct.pack('<H', len(mesh.faces) * 3))

        for f in mesh.faces:
            if not f.is_valid:
                continue

            file.write(struct.pack('<III', *[v.index for v in f.verts]))
