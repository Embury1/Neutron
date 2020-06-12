#include "neutron.h"

#include "glad.c"

void GLAPIENTRY dbg_msg(
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

internal PlatformState* platform_state_pointer(const GameMemory *memory)
{
  return (PlatformState*)((uint8*)memory->persistent_store);
}

internal GameState* game_state_pointer(const GameMemory *memory)
{
  return (GameState*)((uint8*)memory->persistent_store + sizeof(PlatformState));
}

internal void aimat4_to_glm(const aiMatrix4x4 *ai_mat, glm::mat4 *glm_mat)
{
  (*glm_mat)[0][0] = (GLfloat)ai_mat->a1;
  (*glm_mat)[0][1] = (GLfloat)ai_mat->b1;
  (*glm_mat)[0][2] = (GLfloat)ai_mat->c1;
  (*glm_mat)[0][3] = (GLfloat)ai_mat->d1;
  (*glm_mat)[1][0] = (GLfloat)ai_mat->a2;
  (*glm_mat)[1][1] = (GLfloat)ai_mat->b2;
  (*glm_mat)[1][2] = (GLfloat)ai_mat->c2;
  (*glm_mat)[1][3] = (GLfloat)ai_mat->d2;
  (*glm_mat)[2][0] = (GLfloat)ai_mat->a3;
  (*glm_mat)[2][1] = (GLfloat)ai_mat->b3;
  (*glm_mat)[2][2] = (GLfloat)ai_mat->c3;
  (*glm_mat)[2][3] = (GLfloat)ai_mat->d3;
  (*glm_mat)[3][0] = (GLfloat)ai_mat->a4;
  (*glm_mat)[3][1] = (GLfloat)ai_mat->b4;
  (*glm_mat)[3][2] = (GLfloat)ai_mat->c4;
  (*glm_mat)[3][3] = (GLfloat)ai_mat->d4;
}

// uint32 texture_from_file(const char *path, bool gamma)
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

// internal void load_material_textures(GameState *state, aiMaterial *mat, aiTextureType type, char *type_name)
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

internal void process_ainode(GameMemory *memory, const aiNode *ainode, const aiScene *aiscene)
{
  GameState *state = game_state_pointer(memory);
  assert((nnlen(state->meshes) - state->mesh_count) >= ainode->mNumMeshes);
  for (uint32 i = 0; i < ainode->mNumMeshes; i++) {
    aiMesh *aimesh = aiscene->mMeshes[ainode->mMeshes[i]];
    assert(aimesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);
    Mesh *mesh = &state->meshes[state->mesh_count];

    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    glBindVertexArray(mesh->vao);

    const char *mesh_name = ainode->mName.C_Str();
    strncpy_s(mesh->name, sizeof(mesh->name), mesh_name, sizeof(mesh_name));

    Vertex *vertices = (Vertex*)memory->transient_store;
    for (uint32 i = 0; i < aimesh->mNumVertices; i++) {
      vertices[i].position.x = aimesh->mVertices[i].x;
      vertices[i].position.y = aimesh->mVertices[i].y;
      vertices[i].position.z = aimesh->mVertices[i].z;

      if (aimesh->mNormals) {
        vertices[i].normal.x = aimesh->mNormals[i].x;
        vertices[i].normal.y = aimesh->mNormals[i].y;
        vertices[i].normal.z = aimesh->mNormals[i].z;
      }

      if (aimesh->mTextureCoords[0]) {
        vertices[i].tex_coords.x = aimesh->mTextureCoords[0][i].x;
        vertices[i].tex_coords.y = aimesh->mTextureCoords[0][i].y;
      }

      mesh->vertex_count++;
    }

    Bone *bones = (Bone*)memory->transient_store;
    for (uint32 i = 0; i < aimesh->mNumBones; i++) {
      aiBone *aibone = aimesh->mBones[i];

      const char *bone_name = aibone->mName.C_Str();
      uint8 bone_index = UCHAR_MAX;

      for (uint32 j = 0; j < mesh->bone_count; j++) {
        if (strncmp(bones[j].name, bone_name, sizeof(bones[j].name)) == 0) {
          bone_index = j;
          break;
        }
      }

      if (bone_index == UCHAR_MAX) {
        bone_index = mesh->bone_count;
        Bone *bone = &bones[bone_index];
        // strncpy_s(bone->name, sizeof(bone->name) - 1, bone_name, strnlen_s(bone_name, sizeof(bone->name) - 1));
        nnstrcpy(bone->name, bone_name);
        aimat4_to_glm(&aibone->mOffsetMatrix, &bone->offset);
        mesh->bone_count++;
      }

      for (uint32 j = 0; j < aibone->mNumWeights; j++) {
        aiVertexWeight *aiweight = &aibone->mWeights[j];
        Vertex *vertex = &vertices[aiweight->mVertexId];
        for (uint8 k = 0; k < WEIGHT_COUNT_LIMIT; k++) {
          if (vertex->bone_weights[k] == 0.0) {
            vertex->bone_ids[k] = bone_index;
            vertex->bone_weights[k] = aiweight->mWeight;
            break;
          }
        }
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertex_count * sizeof(Vertex), vertices, GL_STATIC_DRAW);

    uint32 *indices = (uint32*)memory->transient_store;
    for (uint32 i = 0; i < aimesh->mNumFaces; i++) {
      aiFace *aiface = &aimesh->mFaces[i];
      for (uint32 j = 0; j < aiface->mNumIndices; j++) {
        indices[i * 3 + j] = aiface->mIndices[j];
        mesh->index_count++;
      }
    }

    state->mesh_count++;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->index_count * sizeof(uint32), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(real32)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(real32)));

    // glEnableVertexAttribArray(3);
    // glVertexAttribIPointer(3, 8, GL_UNSIGNED_INT, sizeof(Vertex), (void*)32);

    // glEnableVertexAttribArray(4);
    // glVertexAttribPointer(4, 8, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)64);

    glBindVertexArray(0);
  }

  for (uint32 i = 0; i < ainode->mNumChildren; i++) {
    process_ainode(memory, ainode->mChildren[i], aiscene);
  }
}

void load_model(const char *path, GameMemory *memory)
{
  aiPropertyStore *props = aiCreatePropertyStore();
  aiSetImportPropertyInteger(props, AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_LINE | aiPrimitiveType_POINT);
  const aiScene *aiscene = aiImportFileExWithProperties(
    path,
    aiProcess_Triangulate
    | aiProcess_FlipUVs
    // | aiProcess_FixInfacingNormals
    // | aiProcess_FlipWindingOrder
    | aiProcess_GenSmoothNormals
    | aiProcess_SortByPType
    | aiProcess_JoinIdenticalVertices
    | aiProcess_OptimizeMeshes
    | aiProcess_OptimizeGraph,
    null,
    props);
  if (!aiscene || aiscene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !aiscene->mRootNode) {
    printf("Failed to load model %s: %s\n", path, aiGetErrorString());
    return;
  }

  process_ainode(memory, aiscene->mRootNode, aiscene);
  aiReleasePropertyStore(props);
}

void load_shader(const char *vs_path, const char *fs_path, GameState *state)
{
  HANDLE vs_file = CreateFileA(
    vs_path,
    GENERIC_READ,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    null,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_READONLY,
    null
  );

  if (vs_file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND)
      printf("%s was not found\n", vs_path);
    return;
  }

  HANDLE fs_file = CreateFileA(
    fs_path,
    GENERIC_READ,
    FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
    null,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_READONLY,
    null
  );

  if (fs_file == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    if (error == ERROR_PATH_NOT_FOUND || error == ERROR_FILE_NOT_FOUND)
      printf("%s was not found\n", vs_path);
    return;
  }

  DWORD bytes_read;
  char vs_buf[kilobytes(5)] = {};
  char fs_buf[kilobytes(5)] = {};

  if (!ReadFile(
    vs_file,
    vs_buf,
    sizeof(vs_buf),
    &bytes_read,
    null
  ) || !bytes_read) {
    printf("%s could not be read\n", vs_path);
    return;
  }

  if (!ReadFile(
    fs_file,
    fs_buf,
    sizeof(fs_buf),
    &bytes_read,
    null
  ) || !bytes_read) {
    printf("%s could not be read\n", fs_path);
    return;
  }

  char log[512];
  int32 success;

  uint32 vs_id = glCreateShader(GL_VERTEX_SHADER);
  const char *vs_code = vs_buf;
  glShaderSource(vs_id, 1, &vs_code, null);
  glCompileShader(vs_id);
  glGetShaderiv(vs_id, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vs_id, sizeof(log), null, log);
    printf("Vertex shader compile error: %s\n", log);
    return;
  }

  uint32 fs_id = glCreateShader(GL_FRAGMENT_SHADER);
  const char *fs_code = fs_buf;
  glShaderSource(fs_id, 1, &fs_code, null);
  glCompileShader(fs_id);
  glGetShaderiv(fs_id, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fs_id, sizeof(log), null, log);
    printf("Fragment shader compile error: %s\n", log);
    return;
  }

  Shader *shader = &state->shaders[state->shader_count];
  shader->id = glCreateProgram();
  strcpy_s(shader->vs_path, sizeof(shader->vs_path), vs_path);
  strcpy_s(shader->fs_path, sizeof(shader->fs_path), fs_path);
  glAttachShader(shader->id, vs_id);
  glAttachShader(shader->id, fs_id);
  glLinkProgram(shader->id);
  glGetProgramiv(shader->id, GL_LINK_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader->id, sizeof(log), null, log);
    printf("Shader linking error: %s\n", log);
    return;
  }

  state->shader_count++;

  glDeleteShader(vs_id);
  glDeleteShader(fs_id);
}

int32 main(int32 argc, int8 **argv)
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

  load_shader("test.vert", "test.frag", game_state);
  // load_model("test.dae", game_state);
  // load_model("cube.dae", game_state);
  // load_model("basemodel.dae", game_state);
  load_model("rig.dae", &memory);

  Camera camera = {
    { 5.0f, 7.0f, 5.0f },
    { 0.0f, 2.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f }
  };

  glUseProgram(game_state->shaders[0].id);

  camera.view = glm::lookAt(camera.position, camera.target, camera.up);
  glUniformMatrix4fv(glGetUniformLocation(game_state->shaders[0].id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));

  glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1920.0f / 1080.0f, 0.01f, 1000.0f);
  glUniformMatrix4fv(glGetUniformLocation(game_state->shaders[0].id, "proj"), 1, GL_FALSE, glm::value_ptr(proj));

  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, glm::vec3(1.0f, 1.0f, 0.0f));
  glUniformMatrix4fv(glGetUniformLocation(game_state->shaders[0].id, "model"), 1, GL_FALSE, glm::value_ptr(model));

  while (!glfwWindowShouldClose(window)) {
    glClearColor(0.75f, 1.0f, 0.01f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float radius = 10.0f;
    camera.position.x = glm::sin(glfwGetTime()) * radius;
    camera.position.z = glm::cos(glfwGetTime()) * radius;
    camera.view = glm::lookAt(camera.position, camera.target, camera.up);
    glUniformMatrix4fv(glGetUniformLocation(game_state->shaders[0].id, "view"), 1, GL_FALSE, glm::value_ptr(camera.view));

    for (uint32 i = 0; i < game_state->mesh_count; i++) {
      glUseProgram(game_state->shaders[0].id);
      glBindVertexArray(game_state->meshes[i].vao);
      glDrawElements(GL_TRIANGLES, game_state->meshes[i].index_count, GL_UNSIGNED_INT, 0);
      glBindVertexArray(0);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  return 0;
}
