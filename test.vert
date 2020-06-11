#version 460 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;
layout (location = 2) in vec2 in_texcoords;
// layout (location = 3) in uint bone_ids[8];
// layout (location = 11) in float bone_weights[8];

layout (location = 0) out vec3 out_pos;
layout (location = 1) out vec3 out_norm;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
  gl_Position = proj * view * model * vec4(in_pos, 1.0);
  out_pos = vec3(model * vec4(in_pos, 1.0));
  out_norm = mat3(transpose(inverse(model))) * in_norm;
}
