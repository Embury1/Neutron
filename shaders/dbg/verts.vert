#version 460 core

layout (location = 0) in vec3 in_pos;
layout (location = 3) in uvec4 bone_ids;
layout (location = 4) in vec4 bone_weights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;
uniform mat4 bones[64];
uniform bool use_bone_transform;

void main()
{
  if (use_bone_transform) {
    mat4 bone_transform = mat4(0);
    for (uint i = 0; i < 4; i++)
      bone_transform += bones[bone_ids[i]] * bone_weights[i];

    gl_Position = proj * view * model * bone_transform * vec4(in_pos, 1.0);
  } else {
    gl_Position = proj * view * model * vec4(in_pos, 1.0);
  }
}
