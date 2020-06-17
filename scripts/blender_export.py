import bmesh
import bpy
from math import radians
from mathutils import Matrix
import os
import struct

MAX_WEIGHTS = 4

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

        vertex_count = len(mesh.verts)
        file.write(struct.pack('<H', vertex_count))
        log.append('{} vertices'.format(vertex_count))

        for i, v in enumerate(mesh.verts):
            if not v.is_valid or v.is_wire:
                continue

            vertex = v.co.xyz
            file.write(struct.pack('<fff', *vertex))
            log.append('v ' + str(vertex))

            normal = v.normal
            file.write(struct.pack('<fff', *normal))
            log.append('n ' + str(normal))

            groups = sorted(obj.data.vertices[v.index].groups, key=lambda g: g.weight, reverse=True)[:MAX_WEIGHTS]
            group_count = len(groups)
            file.write(struct.pack('<B', group_count))
            log.append('{} vertex groups'.format(group_count))

            for grp in groups:
                group = [grp.group, grp.weight]
                file.write(struct.pack('<Bf', *group))
                log.append('g ' + str(group))

        index_count = len(mesh.faces) * 3
        file.write(struct.pack('<H', index_count))
        log.append('{} indices'.format(index_count))

        for f in mesh.faces:
            if not f.is_valid:
                continue

            indices = [v.index for v in f.verts]
            file.write(struct.pack('<III', *indices))
            log.append(indices)

        if obj.parent and obj.parent.type == 'ARMATURE':
            armature = obj.parent

with open(log_file, 'w') as file:
    file.writelines([str(line) + '\n' for line in log])
