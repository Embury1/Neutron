import bmesh
import bpy
from itertools import chain
from math import radians
from mathutils import Matrix
import os
import struct

MAX_WEIGHTS = 4

file_base = os.path.splitext(bpy.data.filepath)[0]
model_file = '{}.nnm'.format(file_base)
log_file = '{}.log'.format(file_base)
y_up_rot = Matrix.Rotation(radians(-90.0), 4, 'X')

log = []

with open(model_file, 'wb') as file:
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH':
            continue

        mesh_name = obj.name.encode('utf-8')
        mesh_name_len = len(mesh_name)
        file.write(struct.pack('<B', mesh_name_len))
        file.write(mesh_name)
        log.append('{} {}'.format(mesh_name_len, mesh_name))

        mesh_world_matrix = list(chain.from_iterable(obj.matrix_world))
        file.write(struct.pack('<ffffffffffffffff', *mesh_world_matrix))
        log.append('w {}'.format(mesh_world_matrix))

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
            armature.data.pose_position = 'REST'

            armature_world_matrix = list(chain.from_iterable(obj.matrix_world))
            file.write(struct.pack('<ffffffffffffffff', *armature_world_matrix))
            log.append('w {}'.format(armature_world_matrix))

            bones = armature.data.bones
            bone_count = len(bones)
            file.write(struct.pack('<H', bone_count))
            log.append('{} bones'.format(bone_count))

            for bone in bones:
                if not bone.use_deform:
                    continue

                bone_name = bone.name.encode('utf-8')
                bone_name_len = len(bone_name)
                file.write(struct.pack('<B', bone_name_len))
                file.write(bone_name)
                log.append('b {} {}'.format(bone_name_len, bone_name))

                bone_matrix = y_up_rot @ bone.matrix_local
                bone_matrix_list = list(chain.from_iterable(bone_matrix))
                file.write(struct.pack('<ffffffffffffffff', *bone_matrix_list))
                log.append(bone_matrix_list)

                if bone.parent:
                    parent_name = bone.parent.name.encode('utf-8')
                    parent_name_len = len(parent_name)
                    file.write(struct.pack('<B', parent_name_len))
                    file.write(parent_name)
                    log.append('p {} {}'.format(parent_name_len, parent_name))
                else:
                    file.write(struct.pack('<B', 0))
                    log.append('p 0')

        else:
            file.write(struct.pack('<H', 0))
            log.append('0 bones')

with open(log_file, 'w') as file:
    file.writelines([str(line) + '\n' for line in log])
