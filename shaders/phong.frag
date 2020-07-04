#version 460 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_norm;

out vec4 out_col;

void main()
{
  vec3 light_col = vec3(0.7, 0.7, 0.0);
  vec3 obj_col = vec3(0.5, 0.5, 0.5);
  vec3 light_pos = vec3(5.0, 5.0, 5.0);

  float ambient_amplitude = 0.8;
  vec3 ambient = ambient_amplitude * light_col;

  vec3 norm = normalize(in_norm);
  vec3 light_dir = normalize(light_pos - in_pos);
  float diff = max(dot(norm, light_dir), 0.0);
  vec3 diffuse = diff * light_col;

  out_col = vec4((ambient * diffuse) * obj_col, 1.0);
}
