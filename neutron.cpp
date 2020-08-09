#include "neutron.h"

#include "glad.c"

void GLAPIENTRY
dbg_msg(
  GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  GLsizei length,
  const GLchar *message,
  const void *user_param)
{
  fprintf(stderr, "%s0x%x 0x%x %s\n", type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR ** " : "", type, severity, message);
}

internal PlatformState*
platform_state_pointer(const GameMemory *memory)
{
  return (PlatformState*)((uint8*)memory->persistent_store);
}

internal GameState*
game_state_pointer(const GameMemory *memory)
{
  return (GameState*)((uint8*)memory->persistent_store + sizeof(PlatformState));
}

// uint32
// texture_from_file(const char *path, bool gamma)
// {
//   uint32 textureID;
//   glGenTextures(1, &textureID);

//   int32 width, height, component_count;
//   unsigned char *data = stbi_load(path, &width, &height, &component_count, 0);
//   if (data) {
//     GLenum format;
//     if (component_count == 1)
//       format = GL_RED;
//     else if (component_count == 3)
//       format = GL_RGB;
//     else if (component_count == 4)
//       format = GL_RGBA;

//     glBindTexture(GL_TEXTURE_2D, textureID);
//     glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
//     glGenerateMipmap(GL_TEXTURE_2D);

//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//     stbi_image_free(data);
//   } else {
//     printf("Texture failed to load at path: %s\n", path);
//     stbi_image_free(data);
//   }

//   return textureID;
// }

// internal void
// load_material_textures(GameState *state, aiMaterial *mat, aiTextureType type, char *type_name)
// {
//   for (uint32 i = 0; i < mat->GetTexturennlen(type); i++) {
//     aiString str;
//     mat->GetTexture(type, i, &str);
//     Texture *texture = &state->textures[state->texture_count];
//     *texture = {};
//     texture->id = texture_from_file(str.C_Str(), false);
//     strcpy_s(texture->type, sizeof(texture->type), type_name);
//     strcpy_s(texture->path, sizeof(texture->path), str.C_Str());
//     state->texture_count++;
//   }
// }

internal void
calc_bone_transform(Bone *bone, const glm::mat4 *parent_transform)
{
  bone->bind_transform = *parent_transform * bone->armature_transform;
  bone->inv_bind_transform = glm::inverse(bone->bind_transform);
  for (uint32 child_index = 0; child_index < bone->child_count; child_index++) {
    calc_bone_transform(bone->children[child_index], &bone->bind_transform);
  }
}

void
load_model(const char *path, GameMemory *memory)
{
  HANDLE file = CreateFileA(
    path,
    GENERIC_READ,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    null,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_READONLY,
    null
  );

  if (file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND)
      printf("%s was not found\n", path);
    return;
  }

  DWORD bytes_read;
  char buffer[kilobytes(500)] = {};

  if (!ReadFile(
    file,
    buffer,
    sizeof(buffer),
    &bytes_read,
    null
  ) || !bytes_read) {
    printf("%s could not be read\n", path);
    return;
  }

  GameState *state = game_state_pointer(memory);
  Mesh *mesh = &state->meshes[state->mesh_count++];
  const char *cursor = buffer;

  uint8 mesh_name_len = (uint8)*cursor++;
  char mesh_name[UCHAR_MAX];
  strncpy_s(mesh_name, sizeof(mesh_name), cursor, mesh_name_len);
  cursor += mesh_name_len;

  mesh->transform = glm::make_mat4((real32*)cursor);
  cursor += 16 * sizeof(real32);

  mesh->vertex_count = (uint8)cursor[0] + ((uint8)cursor[1] << 8);
  cursor += sizeof(uint16);

  assert(memory->transient_store_size >= (mesh->vertex_count * sizeof(Vertex)));
  Vertex *vertices = (Vertex*)memory->transient_store;
  for (uint16 i = 0; i < mesh->vertex_count; i++) {
    real32 *real32_cursor = (real32*)cursor;
    vertices[i].position.x = real32_cursor[0];
    vertices[i].position.y = real32_cursor[1];
    vertices[i].position.z = real32_cursor[2];
    vertices[i].normal.x = real32_cursor[3];
    vertices[i].normal.y = real32_cursor[4];
    vertices[i].normal.z = real32_cursor[5];
    cursor += 6 * sizeof(real32);

    uint8 group_count = (uint8)*cursor++;

    for (uint8 j = 0; j < group_count; j++) {
      vertices[i].bone_ids[j] = (uint8)*cursor++;
      vertices[i].bone_weights[j] = *(real32*)cursor;
      cursor += sizeof(real32);
    }
  }
  
  glGenBuffers(1, &mesh->vbo);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * sizeof(Vertex), vertices, GL_STATIC_DRAW);

  mesh->index_count = *(uint16*)cursor;
  cursor += sizeof(uint16);

  assert(memory->transient_store_size >= (mesh->index_count * sizeof(uint32)));
  uint32 *indices = (uint32*)memory->transient_store;
  memcpy_s(
    indices,
    mesh->index_count * sizeof(uint32),
    (uint32*)cursor,
    mesh->index_count * sizeof(uint32)
  );
  cursor += mesh->index_count * sizeof(uint32);

  glGenVertexArrays(1, &mesh->vao);
  glBindVertexArray(mesh->vao);

  // Remember, the target GL_ELEMENT_ARRAY_BUFFER modifies the VAO, so make sure it's bound first!
  glGenBuffers(1, &mesh->ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->index_count * sizeof(uint32), indices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(real32)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(real32)));

  glEnableVertexAttribArray(3);
  glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, sizeof(Vertex), (void*)(8 * sizeof(real32)));

  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(8 * sizeof(real32) + 4 * sizeof(uint32)));
  glBindVertexArray(0);

  mesh->armature_transform = glm::make_mat4((real32*)cursor);
  mesh->inverse_armature_transform = glm::inverse(mesh->armature_transform);
  cursor += 16 * sizeof(real32);

  mesh->bone_count = *(uint16*)cursor;
  cursor += sizeof(uint16);

  assert(sizeof(mesh->bones) >= (mesh->bone_count * sizeof(Bone)));

  for (uint32 bone_index = 0; bone_index < mesh->bone_count; bone_index++) {
    Bone *bone = &mesh->bones[bone_index];
    uint8 bone_name_length = (uint8)*cursor++;
    strncpy_s(bone->name, sizeof(bone->name), cursor, bone_name_length);
    cursor += bone_name_length;

    bone->armature_transform = glm::make_mat4((real32*)cursor);
    cursor += 16 * sizeof(real32);

    uint8 parent_name_length = (uint8)*cursor++;
    strncpy_s(bone->parent_name, sizeof(bone->parent_name), cursor, parent_name_length);
    cursor += parent_name_length;
  }

  for (uint16 bone_index = 0; bone_index < mesh->bone_count; bone_index++) {
    Bone *bone = &mesh->bones[bone_index];
    if (!bone->parent_name[0])
      continue;

    for (uint16 parent_index = 0; parent_index < bone_index; parent_index++) {
      if (strncmp(bone->parent_name, mesh->bones[parent_index].name, sizeof(bone->parent_name)) == 0) {
        Bone *parent = &mesh->bones[parent_index];
        bone->parent = parent;
        parent->children[parent->child_count++] = bone;
        break;
      }
    }
  }

  assert(memory->transient_store_size >= (mesh->bone_count * sizeof(glm::vec3)));
  glm::vec3 *bone_vertices = (glm::vec3*)memory->transient_store;
  for (uint32 bone_index = 0; bone_index < mesh->bone_count; bone_index++) {
    Bone *bone = &mesh->bones[bone_index];
    glm::mat4 transposed = glm::transpose(bone->armature_transform);
    bone_vertices[bone_index] = glm::vec3(transposed[3]);
    if (!bone->parent) {
      glm::mat4 root_transform(1.0f);
      calc_bone_transform(bone, &root_transform);
    }
  }

  glGenBuffers(1, &mesh->bone_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->bone_vbo);
  glBufferData(GL_ARRAY_BUFFER, mesh->bone_count * sizeof(glm::vec3), bone_vertices, GL_STATIC_DRAW);
  glGenVertexArrays(1, &mesh->bone_vao);
  glBindVertexArray(mesh->bone_vao);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
  glBindVertexArray(0);
}

internal uint32
read_shader_code_from_file(char *path, uint16 shader_type)
{
  HANDLE file = CreateFileA(
    path,
    GENERIC_READ,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    null,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_READONLY,
    null
  );

  if (file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND)
      fprintf(stderr, "%s was not found\n", path);
    return 0;
  }

  DWORD bytes_read;
  char buffer[kilobytes(5)] = {};
  char log[512];
  int32 success;

  if (!ReadFile(
    file,
    buffer,
    sizeof(buffer),
    &bytes_read,
    null
  ) || !bytes_read) {
    fprintf(stderr, "%s could not be read\n", path);
    return 0;
  }

  uint32 id = glCreateShader(shader_type);
  const char *code = buffer;
  glShaderSource(id, 1, &code, null);
  glCompileShader(id);
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(id, sizeof(log), null, log);
    fprintf(stderr, "Shader compile error: %s\n", log);
    return 0;
  }

  return id;
}

void
load_shader(Shader *shader)
{
  assert(shader->vs_path);
  assert(shader->fs_path);

  uint32 vs_id = read_shader_code_from_file(shader->vs_path, GL_VERTEX_SHADER);
  uint32 gs_id = *shader->gs_path != '\0' ? read_shader_code_from_file(shader->gs_path, GL_GEOMETRY_SHADER) : 0;
  uint32 fs_id = read_shader_code_from_file(shader->fs_path, GL_FRAGMENT_SHADER);

  shader->id = glCreateProgram();
  glAttachShader(shader->id, vs_id);
  if (gs_id)
    glAttachShader(shader->id, gs_id);
  glAttachShader(shader->id, fs_id);
  glLinkProgram(shader->id);
  int32 success;
  glGetProgramiv(shader->id, GL_LINK_STATUS, &success);
  if (!success) {
    char log[512];
    glGetProgramInfoLog(shader->id, sizeof(log), null, log);
    fprintf(stderr, "Shader linking error: %s\n", log);
    return;
  }

  glDeleteShader(vs_id);
  if (gs_id)
    glDeleteShader(gs_id);
  glDeleteShader(fs_id);
}

int32
main(int32 argc, int8 *argv[])
{
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(1920, 1080, "Neutron", 0, 0);
  if (!window) {
    printf("Failed to create GLFW window\n");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int32 width, int32 height) { glViewport(0, 0, width, height); });
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    printf("Failed to initialize GLAD\n");
    glfwTerminate();
    return -1;
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(dbg_msg, 0);
  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);

  GameMemory memory = {};
  memory.persistent_store_size = gigabytes(1);
  memory.transient_store_size = gigabytes(4);

#ifdef NN_INTERNAL
  LPVOID base_addr = (LPVOID)terabytes(2);
#else
  LPVOID base_addr = 0;
#endif

  memory.persistent_store = VirtualAlloc(
    base_addr,
    (size_t)(memory.persistent_store_size + memory.transient_store_size),
    MEM_RESERVE | MEM_COMMIT,
    PAGE_READWRITE
  );

  memory.transient_store = (uint8*)memory.persistent_store + memory.persistent_store_size;

  PlatformState *platform_state = platform_state_pointer(&memory);
  GameState *game_state = game_state_pointer(&memory);

  platform_state->L = luaL_newstate();
  luaL_openlibs(platform_state->L);
  luaL_loadfile(platform_state->L, "test.lua");

  Shader *phong_shader = &game_state->shaders[game_state->shader_count++];
  nnstrcpy(phong_shader->vs_path, "shaders/phong.vert");
  nnstrcpy(phong_shader->fs_path, "shaders/phong.frag");
  load_shader(phong_shader);

  Shader *vert_dbg_shader = &game_state->shaders[game_state->shader_count++];
  nnstrcpy(vert_dbg_shader->vs_path, "shaders/dbg/verts.vert");
  nnstrcpy(vert_dbg_shader->gs_path, "shaders/dbg/verts.geom");
  nnstrcpy(vert_dbg_shader->fs_path, "shaders/dbg/verts.frag");
  load_shader(vert_dbg_shader);

  Shader *bone_dbg_shader = &game_state->shaders[game_state->shader_count++];
  nnstrcpy(bone_dbg_shader->vs_path, "shaders/dbg/bones.vert");
  nnstrcpy(bone_dbg_shader->gs_path, "shaders/dbg/bones.geom");
  nnstrcpy(bone_dbg_shader->fs_path, "shaders/dbg/bones.frag");
  load_shader(bone_dbg_shader);

  load_model("pillar.nnm", &memory);

  Camera camera = {
    { 1.0f, 1.0f, 1.0f },
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f }
  };

  bool32 draw_mesh = true;
  bool32 debug_vertices = true;
  bool32 debug_bones = true;
  bool32 use_bone_transform = true;

  camera.view = glm::lookAt(camera.position, camera.target, camera.up);
  glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1920.0f / 1080.0f, 0.01f, 1000.0f);
  glm::mat4 model(1.0f);
  // model = glm::translate(model, glm::vec3(1.0f, 1.0f, 0.0f));

  if (draw_mesh) {
    glUseProgram(phong_shader->id);
    glUniformMatrix4fv(glGetUniformLocation(phong_shader->id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(phong_shader->id, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(phong_shader->id, "model"), 1, GL_FALSE, glm::value_ptr(model));
  }

  if (debug_vertices) {
    glUseProgram(vert_dbg_shader->id);
    glUniformMatrix4fv(glGetUniformLocation(vert_dbg_shader->id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(vert_dbg_shader->id, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(vert_dbg_shader->id, "model"), 1, GL_FALSE, glm::value_ptr(model));
  }

  if (debug_bones) {
    glUseProgram(bone_dbg_shader->id);
    glUniformMatrix4fv(glGetUniformLocation(bone_dbg_shader->id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    glUniformMatrix4fv(glGetUniformLocation(bone_dbg_shader->id, "proj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(bone_dbg_shader->id, "model"), 1, GL_FALSE, glm::value_ptr(model));
  }

  persistent KeyBinding key_bindings[] = {
    { GLFW_KEY_ESCAPE },
    { GLFW_KEY_F1 },
    { GLFW_KEY_F2 },
    { GLFW_KEY_F3 },
    { GLFW_KEY_F4 }
  };

  while (!glfwWindowShouldClose(window)) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    real64 time = glfwGetTime();

    for (uint16 binding_index = 0; binding_index < nnlen(key_bindings); binding_index++) {
      KeyBinding *binding = &key_bindings[binding_index];
      binding->curr_state = glfwGetKey(window, binding->key);
      uint8 key_state = keystate(binding->curr_state, binding->prev_state);

      if (!key_state)
        continue;

      switch (binding->key) {
        case GLFW_KEY_ESCAPE:
          glfwSetWindowShouldClose(window, true);
          break;
        case GLFW_KEY_F1:
          if (key_state == keydown)
            draw_mesh = !draw_mesh;
          break;
        case GLFW_KEY_F2:
          if (key_state == keydown)
            debug_vertices = !debug_vertices;
          break;
        case GLFW_KEY_F3:
          if (key_state == keydown)
            debug_bones = !debug_bones;
          break;
        case GLFW_KEY_F4:
          if (key_state == keydown)
            use_bone_transform = !use_bone_transform;
          break;
      }

      binding->prev_state = binding->curr_state;
    }

    const real32 radius(10.0f);
    camera.position.x = glm::sin(time) * radius;
    camera.position.z = glm::cos(time) * radius;
    camera.view = glm::lookAt(camera.position, camera.target, camera.up);

    if (draw_mesh) {
      glUseProgram(phong_shader->id);
      glUniformMatrix4fv(glGetUniformLocation(phong_shader->id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
      glUniform1ui(glGetUniformLocation(vert_dbg_shader->id, "use_bone_transform"), use_bone_transform);
    }

    if (debug_vertices) {
      glUseProgram(vert_dbg_shader->id);
      glUniformMatrix4fv(glGetUniformLocation(vert_dbg_shader->id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
      glUniform1ui(glGetUniformLocation(vert_dbg_shader->id, "use_bone_transform"), use_bone_transform);
    }

    if (debug_bones) {
      glUseProgram(bone_dbg_shader->id);
      glUniformMatrix4fv(glGetUniformLocation(bone_dbg_shader->id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));
    }

    for (uint32 mesh_index = 0; mesh_index < game_state->mesh_count; mesh_index++) {
      Mesh *mesh = &game_state->meshes[mesh_index];

      for (uint8 bone_index = 0; bone_index < mesh->bone_count; bone_index++) {
        char uniform_name[40] = {};
        sprintf_s(uniform_name, sizeof(uniform_name), "bones[%d]", bone_index);
        Bone *bone = &mesh->bones[bone_index];

        if (draw_mesh) {
          glUseProgram(phong_shader->id);
          glm::mat4 bone_transform = bone->bind_transform * bone->inv_bind_transform;
          glUniformMatrix4fv(glGetUniformLocation(phong_shader->id, uniform_name), 1, GL_TRUE, glm::value_ptr(bone_transform));
        }

        if (debug_vertices) {
          glUseProgram(vert_dbg_shader->id);
          glm::mat4 bone_transform = bone->bind_transform * bone->inv_bind_transform;
          glUniformMatrix4fv(glGetUniformLocation(vert_dbg_shader->id, uniform_name), 1, GL_TRUE, glm::value_ptr(bone_transform));
        }
      }

      glBindVertexArray(mesh->vao);

      if (draw_mesh) {
        glUseProgram(phong_shader->id);
        glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, 0);
      }

      if (debug_vertices) {
        glUseProgram(vert_dbg_shader->id);
        glDrawElements(GL_POINTS, mesh->index_count, GL_UNSIGNED_INT, 0);
      }

      if (debug_bones) {
        glBindVertexArray(mesh->bone_vao);
        glUseProgram(bone_dbg_shader->id);
        glDrawArrays(GL_POINTS, 0, mesh->bone_count);
      }

      glBindVertexArray(0);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  return 0;
}
