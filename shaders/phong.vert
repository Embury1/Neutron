#version 460 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_texcoords;
layout (location = 3) in uvec4 bone_ids;
layout (location = 4) in vec4 bone_weights;

layout (location = 0) out vec3 out_pos;
layout (location = 1) out vec3 out_norm;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 bones[64];

void main()
{
  mat4 bone_transform = bones[bone_ids[0]] * bone_weights[0];
  bone_transform += bones[bone_ids[1]] * bone_weights[1];
  bone_transform += bones[bone_ids[2]] * bone_weights[2];
  bone_transform += bones[bone_ids[3]] * bone_weights[3];

  gl_Position = proj * view * model * vec4(in_pos, 1.0);
  out_pos = vec3(model * vec4(in_pos, 1.0));
  out_norm = mat3(transpose(inverse(model))) * in_norm;
}
