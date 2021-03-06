#ifndef NEUTRON_H
#define NEUTRON_H

#include <windows.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assert.h>
#include <limits.h>
// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" {
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
}

#define global      static
#define internal    static
#define persistent  static

#define null        0
#define false       0
#define true        !false

#define keyup       1
#define keydown     2
#define keyhold     3

#define kilobytes(sz)           (sz * 1024LL)
#define megabytes(sz)           (kilobytes(sz) * 1024LL)
#define gigabytes(sz)           (megabytes(sz) * 1024LL)
#define terabytes(sz)           (gigabytes(sz) * 1024LL)
#define nnlen(arr)              (sizeof((arr)) / (sizeof((arr)[0])))
#define minimum(a, b)           ((a) < (b) ? (a) : (b))
#define maximum(a, b)           ((a) > (b) ? (a) : (b))
#define nnstrcpy(dst, src)      (strncpy_s((dst), sizeof((dst)) - 1, (src), strnlen_s((src), sizeof((dst)) - 1)))
#define keystate(curr, prev)    ((curr << 1) | prev)

#define WEIGHT_COUNT_LIMIT 4

typedef int8_t        int8;
typedef int16_t       int16;
typedef int32_t       int32;
typedef int64_t       int64;
typedef uint8_t       uint8;
typedef uint16_t      uint16;
typedef uint32_t      uint32;
typedef uint64_t      uint64;
typedef int32         bool32;
typedef float         real32;
typedef double        real64;

struct GameMemory {
  uint64  persistent_store_size;
  void    *persistent_store;
  uint64  transient_store_size;
  void    *transient_store;
};

struct PlatformState {
  lua_State *L;
};

struct Bone {
  char name[32];
  glm::mat4 armature_transform;
  glm::mat4 bind_transform;
  glm::mat4 inv_bind_transform;
  Bone *parent;
  char parent_name[32];
  Bone *children[16];
  uint8 child_count;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 tex_coords;
  uint32 bone_ids[WEIGHT_COUNT_LIMIT];
  real32 bone_weights[WEIGHT_COUNT_LIMIT];
};

struct Texture {
  uint16 id;
  char type[36];
  char path[64];
};

struct Mesh {
  uint32 vertex_count;
  uint32 index_count;
  Bone bones[64];
  uint32 bone_count;
  char name[24];
  Texture *textures;
  uint32 texture_count;
  uint32 vao;
  uint32 vbo;
  uint32 ebo;
  uint32 bone_vao;
  uint32 bone_vbo;
  glm::mat4 transform;
  glm::mat4 armature_transform;
  glm::mat4 inverse_armature_transform;
};

struct Shader {
  uint32 id;
  char vs_path[32];
  char gs_path[32];
  char fs_path[32];
};

struct GameState {
  Texture textures[128];
  uint32 texture_count;
  Mesh meshes[16];
  uint32 mesh_count;
  Shader shaders[16];
  uint32 shader_count;
};

struct Camera {
  glm::vec3 position;
  glm::vec3 target;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 inverse_direction;
  glm::mat4 view;
};

struct KeyBinding {
  uint16 key;
  uint8 curr_state;
  uint8 prev_state;
};

#endif // NEUTRON_H
