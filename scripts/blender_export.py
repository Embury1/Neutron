import bmesh
import bpy
from math import radians
from mathutils import Matrix
import os
import struct

os.system('cls')
file_base = os.path.splitext(bpy.data.filepath)[0]
model_file = '{}.nnm'.format(file_base)
log_file = '{}.log'.format(file_base)
y_up_rot = Matrix.Rotation(radians(-90.0), 4, 'X')

log = []

with open(model_file, 'wb') as file:
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue

        mesh_name = obj.name
        file.write(struct.pack('<B', len(mesh_name)))
        file.write(mesh_name.encode('utf-8'))
        log.append('Starting export of mesh "{}"'.format(mesh_name))

        mesh = bmesh.new()
        mesh.from_mesh(obj.data)
        mesh.transform(y_up_rot)
        bmesh.ops.triangulate(mesh, faces=mesh.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')

        vert_count = len(mesh.verts)
        file.write(struct.pack('<H', vert_count))
        log.append('{} vertices'.format(vert_count))

        for v in mesh.verts:
            if not v.is_valid or v.is_wire:
                continue

            vertex = v.co.xyz
            file.write(struct.pack('<fff', *vertex))
            log.append(vertex)

            normal = v.normal
            file.write(struct.pack('<fff', *normal))
            log.append(normal)

        index_count = len(mesh.faces) * 3
        file.write(struct.pack('<H', index_count))
        log.append('{} indices'.format(index_count))

        for f in mesh.faces:
            if not f.is_valid:
                continue

            indices = [v.index for v in f.verts]
            file.write(struct.pack('<III', *indices))
            log.append(indices)

with open(log_file, 'w') as file:
    file.writelines([str(line) + '\n' for line in log])
