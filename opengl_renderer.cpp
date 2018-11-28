#include "renderer.h"

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <GL/gl3w.h>

#include "imgui.h"

#include "memory.h"
#include "tanks.h"
#include "opengl_renderer.h"
#include "game_rendering.h"
#include "buffer.h"

#define USE_CRAPPY_CACHE 0

static inline b32
compile_shader_from_source(GLuint shader, buffer32 source)
{
    glShaderSource(shader, 1, (GLchar**)&source.data, (GLint*)&source.size);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    return status == GL_TRUE;
}

static inline b32
create_and_link_program(GLuint vs, GLuint fs, GLuint* program)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);

    glLinkProgram(p);

    GLint status = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &status);

    *program = p;
    return status == GL_TRUE;
}

static inline b32
load_shader(const char* file, GLenum type, GLuint* shader)
{
    buffer32 source = read_file_buffer(file);
    if (!source) return false;

    GLuint s = glCreateShader(type);
    if (s == GL_INVALID_VALUE) return false;

    GLchar errorBuffer[256];
    if (!compile_shader_from_source(s, source)) {
        glGetShaderInfoLog(s, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to compile \"%s\": %s\n", file, errorBuffer);
        return false;
    }

    *shader = s;
    return true;
}

static inline b32
load_program(const char* vsFile, const char* fsFile, GLuint* program)
{
    buffer32 vsSource = read_file_buffer(vsFile);
    if (!vsSource) return false;

    buffer32 fsSource = read_file_buffer(fsFile);
    if (!fsSource) return false;

    GLuint vs = GL_INVALID_VALUE;
    if (!load_shader(vsFile, GL_VERTEX_SHADER, &vs)) return false;
    defer( glDeleteShader(vs) );

    GLuint fs = GL_INVALID_VALUE;
    if (!load_shader(fsFile, GL_FRAGMENT_SHADER, &fs)) return false;
    defer( glDeleteShader(fs) );

    char errorBuffer[256];
    if (!create_and_link_program(vs, fs, program)) {
        glGetProgramInfoLog(*program, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to link program from \"%s\" and \"%s\": %s\n", vsFile, fsFile, errorBuffer);
        return false;
    }

    return true;
}

static inline b32
load_lines_program(const Shader_Catalog& catalog, Lines_Program* program)
{
    if (!create_and_link_program(catalog.basicVertexShader, catalog.solidFramentShader, &program->id)) {
        log_crit("Failed to load lines program!\n");
        return false;
    }

    GLuint id = program->id;

    program->mvp   = glGetUniformLocation(id, "u_mvp");
    program->color = glGetUniformLocation(id, "u_color");

    return true;
}

static inline b32
load_cubes_program(const Shader_Catalog& catalog, Cubes_Program* program)
{
    if (!create_and_link_program(catalog.basicInstancedVertexShader, catalog.solidFramentShader, &program->id)) {
        log_crit("Failed to load cubes program.\n");
        return false;
    }

    GLuint id = program->id;

    program->viewProjection = glGetUniformLocation(id, "u_viewProjection");
    program->color          = glGetUniformLocation(id, "u_color");
    program->scale          = glGetUniformLocation(id, "u_scale");

    return true;
}

static inline b32
load_static_mesh_program(const Shader_Catalog& catalog, Static_Mesh_Program* program)
{
    if (!create_and_link_program(catalog.staticMeshVertexShader, catalog.staticMeshFragmentShader, &program->id)) {
        log_crit("Failed to load static mesh program.\n");
        return false;
    }

    GLuint id = program->id;

    program->modelViewMatrix  = glGetUniformLocation(id, "u_modelViewMatrix");
    program->projectionMatrix = glGetUniformLocation(id, "u_projectionMatrix");
    program->normalMatrix     = glGetUniformLocation(id, "u_normalMatrix");

    program->solid = glGetUniformLocation(id, "u_solid");
    program->color = glGetUniformLocation(id, "u_color");

    program->diffuseMap  = glGetUniformLocation(id, "u_diffuse");
    program->normalMap   = glGetUniformLocation(id, "u_normal");
    program->specularMap = glGetUniformLocation(id, "u_specular");

    program->hasNormalMap   = glGetUniformLocation(id, "u_hasNormalMap");
    program->hasSpecularMap = glGetUniformLocation(id, "u_hasSpecularMap");

    program->specularExp = glGetUniformLocation(id, "u_specularExp");
    program->lit         = glGetUniformLocation(id, "u_lit");
    program->lightPos    = glGetUniformLocation(id, "u_pointLightP");
    program->lightColor  = glGetUniformLocation(id, "u_pointLightC");

    return true;
}

static inline GLuint
bind_debug_lines(Rolling_Cache& cache, Render_Debug_Lines* cmd)
{
#if 0
    // If this command has already been staged and hasn't been evicted, return its vao.
    u32 id = down_cast<u32>(cmd->_staged);

    GLuint vao = GL_INVALID_VALUE;
    if (cache.get(id, &vao)) {
        glBindVertexArray(vao);
        return vao;
    }

    Rolling_Handle* handle = nullptr;
    if (!cache.add(&handle)) {
        GLuint evicted = cache.evict();
        glDeleteVertexArrays(1, &evicted);
        cache.add(&handle);
    }

    GLuint* newVao = &handle->handle;
#endif
    ((void)cache);
    if (cmd->_staged) {
        GLuint vao = (GLuint)(umm)cmd->_staged;
        glBindVertexArray(vao);
        return vao;
    }

    GLuint newVao = GL_INVALID_VALUE;

    glGenVertexArrays(1, &newVao);
    glBindVertexArray(newVao);
    glEnableVertexAttribArray(0);

    GLuint vbo = GL_INVALID_VALUE;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cmd->vertexCount * 3 * sizeof(f32), cmd->vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glBindVertexArray(newVao); // left bound on return

#if 0
    cmd->_staged = (void*)(uptr)handle->id;
#else
    cmd->_staged = (void*)(umm)newVao;
#endif
    return newVao;
}

static inline GLuint
bind_debug_cubes(GLuint vbo, GLuint ebo, Rolling_Cache&, Render_Debug_Cubes* cmd)
{
    if (cmd->_staged) {
        GLuint vao = (GLuint)(umm)cmd->_staged;
        glBindVertexArray(vao);
        return vao;
    }

    GLuint newVao = GL_INVALID_VALUE;
    glGenVertexArrays(1, &newVao);
    glBindVertexArray(newVao);

    GLuint centerBuffer = GL_INVALID_VALUE;
    glGenBuffers(1, &centerBuffer);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, centerBuffer);
    glBufferData(GL_ARRAY_BUFFER, cmd->count * 3 * sizeof(f32), cmd->centers, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glVertexAttribDivisor(1, 1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(newVao);

    cmd->_staged = (void*)(umm)newVao;
    return newVao;
}

static inline GLenum
to_gl_index_type(Index_Size size)
{
    switch (size) {
    case IndexSize_u8:  return GL_UNSIGNED_BYTE;
    case IndexSize_u16: return GL_UNSIGNED_SHORT;
    case IndexSize_u32: return GL_UNSIGNED_INT;
    }

    return GL_INVALID_ENUM;
}

struct GL_Format
{
    GLenum upload   = GL_INVALID_ENUM;
    GLenum internal = GL_INVALID_ENUM;
};

// TODO(blake): handle more than 8 bits ber channel.
static inline GL_Format
to_gl_format(Texture_Format format, bool srgb)
{
    GL_Format result;

    if (srgb) {
        switch (format) {
        case TextureFormat_Grey:
            result.upload   = GL_RED;
            result.internal = GL_R8;
            break;
        case TextureFormat_GreyAlpha:
            result.upload   = GL_RG;
            result.internal = GL_RG8;
            break;
        case TextureFormat_RGB:
            result.upload   = GL_RGB;
            result.internal = GL_SRGB8;
            break;
        case TextureFormat_RGBA:
            result.upload   = GL_RGBA;
            result.internal = GL_SRGB8_ALPHA8;
            break;
        }
    }
    else {
        switch (format) {
        case TextureFormat_Grey:
            result.upload   = GL_RED;
            result.internal = GL_R8;
            break;
        case TextureFormat_GreyAlpha:
            result.upload   = GL_RG;
            result.internal = GL_RG8;
            break;
        case TextureFormat_RGB:
            result.upload   = GL_RGB;
            result.internal = GL_RGB8;
            break;
        case TextureFormat_RGBA:
            result.upload   = GL_RGBA;
            result.internal = GL_RGBA8;
            break;
        }
    }

    return result;
}

enum Texture_Options
{
    TexOpt_None          = 0x0,
    TexOpt_SRGB          = 0x1,
    TexOpt_Mipmap        = 0x2,
    TexOpt_UnpackRowZero = 0x4,
};

static inline GLuint
stage_texture(Texture texture, u32 options, GLenum wrapType)
{
    if (!texture.data) return GL_INVALID_VALUE;

    GLuint handle = GL_INVALID_VALUE;
    glGenTextures(1, &handle);

    glBindTexture(GL_TEXTURE_2D, handle);

    if (options & TexOpt_UnpackRowZero)
        glPixelStorei(GL_TEXTURE_2D, 0);

    GL_Format format = to_gl_format(texture.format, options & TexOpt_SRGB);
    glTexImage2D(GL_TEXTURE_2D, 0, format.internal, texture.x, texture.y, 0, format.upload, GL_UNSIGNED_BYTE, texture.data);

    GLuint minFilter = GL_LINEAR;
    if (options & TexOpt_Mipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
        minFilter = GL_NEAREST_MIPMAP_LINEAR;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    return handle;
}

static inline Staged_Static_Mesh*
stage_static_mesh(Rolling_Cache&, Render_Static_Mesh* cmd)
{
    if (cmd->_staged) return (Staged_Static_Mesh*)cmd->_staged;

    Static_Mesh& mesh = cmd->mesh;

    GLuint vao = GL_INVALID_VALUE;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vertices = GL_INVALID_VALUE;
    glGenBuffers(1, &vertices);

    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vertices);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(f32), mesh.vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    GLuint uvs = GL_INVALID_VALUE;
    glGenBuffers(1, &uvs);

    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, uvs);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 2 * sizeof(f32), mesh.uvs, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

    if (mesh.has_normals()) {
        GLuint normals = GL_INVALID_VALUE;
        glGenBuffers(1, &normals);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, normals);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(f32), mesh.normals, GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }

    // TODO: tangents
    if (mesh.has_tangents()) {
        GLuint tangents = GL_INVALID_VALUE;
        glGenBuffers(1, &tangents);

        glEnableVertexAttribArray(3);
        glBindBuffer(GL_ARRAY_BUFFER, tangents);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * 3 * sizeof(f32), mesh.tangents, GL_STATIC_DRAW);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, 0);
    }


    GLuint ebo = GL_INVALID_VALUE;
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indexCount * mesh.indexSize, mesh.indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Textures

    u32 groupCount = mesh.material->coloredGroupCount;

    Staged_Static_Mesh* stagedMesh = allocate_new(Staged_Static_Mesh);
    stagedMesh->vao        = vao;
    stagedMesh->groups     = allocate_array(groupCount, Staged_Colored_Index_Group);
    stagedMesh->groupCount = groupCount;

    for (u32 i = 0; i < mesh.material->coloredGroupCount; i++) {
        Colored_Index_Group&        group       = mesh.material->coloredIndexGroups[i];
        Staged_Colored_Index_Group& stagedGroup = stagedMesh->groups[i];

        stagedGroup.indexStart = group.start;
        stagedGroup.indexCount = group.count;
        stagedGroup.indexType  = to_gl_index_type(mesh.indexSize);

        if (!group.has_diffuse_map()) {
            stagedGroup.color       = group.color;
            stagedGroup.specularExp = group.specularExp;
            continue;
        }

        stagedGroup.diffuseMap  = stage_texture(group.diffuseMap,  TexOpt_SRGB | TexOpt_Mipmap, GL_REPEAT);
        //stagedGroup.diffuseMap  = stage_texture(group.diffuseMap,  TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.emissiveMap = stage_texture(group.emissiveMap, TexOpt_SRGB | TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.normalMap   = stage_texture(group.normalMap,   TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.specularMap = stage_texture(group.specularMap, TexOpt_Mipmap, GL_REPEAT);
        stagedGroup.specularExp = group.specularExp;
    }

    cmd->_staged = stagedMesh;

    return stagedMesh;
}

static inline b32
load_debug_cube_buffers(GLuint* vertexBuffer, GLuint* indexBuffer)
{
    Static_Mesh cube = push_debug_cube(.5f);

    GLuint vbo = GL_INVALID_VALUE;
    GLuint ebo = GL_INVALID_VALUE;

    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cube.vertexCount * 3 * sizeof(f32), cube.vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, ebo);
    glBufferData(GL_ARRAY_BUFFER, cube.indexCount * cube.indexSize, cube.indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    *vertexBuffer = vbo;
    *indexBuffer  = ebo;
    return true;
}

static inline b32
load_imgui_texture_atlas(ImGui_Resources* res)
{
    u8* out = nullptr;
    int w   = 0;
    int h   = 0;
    int bbp = 0;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetTexDataAsAlpha8(&out, &w, &h, &bbp);

    Texture tex;
    tex.data = out;
    tex.format = TextureFormat_Grey;
    tex.x = w;
    tex.y = h;

    GLuint atlasHandle = stage_texture(tex, TexOpt_UnpackRowZero, GL_CLAMP_TO_EDGE);
    res->textureAtlas = atlasHandle;
    io.Fonts->TexID   = (void*)(umm)atlasHandle;

    ImGui::MemFree(out);

    return true;
}

static inline b32
load_imgui(ImGui_Resources* res)
{
    ImGui_Program& program = res->program;

    GLuint vs = GL_INVALID_VALUE;
    GLuint fs = GL_INVALID_VALUE;
    if (!load_shader("demo/imgui.vs", GL_VERTEX_SHADER,   &vs)) return false;
    defer( glDeleteShader(vs); );

    if (!load_shader("demo/imgui.fs", GL_FRAGMENT_SHADER, &fs)) return false;
    defer( glDeleteShader(fs); );

    if (!create_and_link_program(vs, fs, &program.id)) return false;

    program.projectionMatrix = glGetUniformLocation(program.id, "u_projectionMatrix");
    program.texture          = glGetUniformLocation(program.id, "u_texture");

    glGenVertexArrays(1, &res->vao);

    glGenBuffers(1, &res->vertexBuffer);
    glGenBuffers(1, &res->indexBuffer);

    glBindVertexArray(res->vao);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    if (!load_imgui_texture_atlas(res)) return false;

    return true;
}

extern b32
renderer_init(Memory_Arena* storage, Memory_Arena* workspace)
{
    *workspace = sub_allocate(*storage, Kilobytes(4), 16, "Rendering Workspace");

    OpenGL_Renderer* renderer = push_new(*workspace, OpenGL_Renderer);
    renderer->debugLinesCache.reset(32, push_array(*workspace, 32, Rolling_Handle));
    renderer->debugCubesCache.reset(32, push_array(*workspace, 32, Rolling_Handle));
    renderer->staticMeshCache.reset(32, push_array(*workspace, 32, Rolling_Handle));

    Shader_Catalog& catalog = renderer->shaderCatalog;

    if (!load_shader("demo/basic.vs",           GL_VERTEX_SHADER,   &catalog.basicVertexShader))          return false;
    if (!load_shader("demo/basic_instanced.vs", GL_VERTEX_SHADER,   &catalog.basicInstancedVertexShader)) return false;
    if (!load_shader("demo/static_mesh.vs",     GL_VERTEX_SHADER,   &catalog.staticMeshVertexShader))     return false;
    if (!load_shader("demo/solid.fs",           GL_FRAGMENT_SHADER, &catalog.solidFramentShader))         return false;
    if (!load_shader("demo/static_mesh.fs",     GL_FRAGMENT_SHADER, &catalog.staticMeshFragmentShader))   return false;
    if (!load_lines_program(catalog, &renderer->linesProgram))            return false;
    if (!load_cubes_program(catalog, &renderer->cubesProgram))            return false;
    if (!load_static_mesh_program(catalog, &renderer->staticMeshProgram)) return false;

    if (!load_debug_cube_buffers(&renderer->debugCubeVertexBuffer, &renderer->debugCubeIndexBuffer)) return false;
#if USING_IMGUI
    if (!load_imgui(&renderer->imgui)) return false;
#endif

    // NOTE(blake): you _need_ to specify the blend equation/func.
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_FRAMEBUFFER_SRGB);

    glCullFace(GL_BACK);

    return true;
}

static void
renderer_begin_frame_internal(Memory_Arena* workspace, void* commands, u32 count, GLbitfield toClear)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;

    Render_Command_Header* header = (Render_Command_Header*)commands;
    for (u32 i = 0; i < count; i++, header = next_header(header, header->size)) {
        switch (header->type) {
        case RenderCommand_Set_Clear_Color: {
            Set_Clear_Color* cmd = render_command_after<Set_Clear_Color>(header);
            glClearColor(cmd->r, cmd->g, cmd->b, cmd->a);
            break;
        }
        case RenderCommand_Set_Viewport: {
            Set_Viewport* cmd = render_command_after<Set_Viewport>(header);
            glViewport(cmd->x, cmd->y, cmd->w, cmd->h);
            break;
        }
        case RenderCommand_Set_View_Matrix: {
            Set_View_Matrix* cmd = render_command_after<Set_View_Matrix>(header);
            memcpy(&renderer->viewMatrix, cmd->mat4, sizeof(renderer->viewMatrix));
            break;
        }
        case RenderCommand_Set_Projection_Matrix: {
            Set_Projection_Matrix* cmd = render_command_after<Set_Projection_Matrix>(header);
            memcpy(&renderer->projectionMatrix, cmd->mat4, sizeof(renderer->projectionMatrix));
            break;
        }
        }
    }

    glClear(toClear);
}

extern void
renderer_begin_frame(Memory_Arena* workspace, void* commands, u32 count)
{
    renderer_begin_frame_internal(workspace, commands, count,
                                  GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

extern b32
renderer_exec(Memory_Arena* workspace, void* commands, u32 count)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;

    renderer->pointLight = nullptr;

    allocator_scope(workspace);

    Render_Command_Header* header = (Render_Command_Header*)commands;
    for (u32 i = 0; i < count; i++, header = next_header(header, header->size)) {
        switch (header->type) {
        case RenderCommand_Render_Debug_Lines: {
            Render_Debug_Lines* cmd = render_command_after<Render_Debug_Lines>(header);

            Lines_Program& program = renderer->linesProgram;
            glUseProgram(program.id);

            mat4 mvp = renderer->projectionMatrix * renderer->viewMatrix;
            glUniformMatrix4fv(program.mvp, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform3f(program.color, cmd->r, cmd->g, cmd->b);

            bind_debug_lines(renderer->debugLinesCache, cmd);
            glDrawArrays(GL_LINES, 0, cmd->vertexCount);
            glBindVertexArray(0);
            glUseProgram(0);

            break;
        }
        case RenderCommand_Render_Debug_Cubes: {
            Render_Debug_Cubes* cmd = render_command_after<Render_Debug_Cubes>(header);

            Cubes_Program& program = renderer->cubesProgram;
            glUseProgram(program.id);

            bind_debug_cubes(renderer->debugCubeVertexBuffer,
                             renderer->debugCubeIndexBuffer,
                             renderer->debugCubesCache,
                             cmd);

            mat4 viewProjection = renderer->projectionMatrix * renderer->viewMatrix;
            glUniformMatrix4fv(program.viewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
            glUniform3f(program.color, cmd->r, cmd->g, cmd->b);
            glUniform1f(program.scale, cmd->halfWidth);

            glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, 0, cmd->count);

            glUseProgram(0);
            break;
        }
        case RenderCommand_Render_Textured_Quad: {
            break;
        }
        case RenderCommand_Render_Static_Mesh: {
            Render_Static_Mesh* cmd = render_command_after<Render_Static_Mesh>(header);

            Static_Mesh_Program& program = renderer->staticMeshProgram;
            glUseProgram(program.id);

            mat4 modelView = renderer->viewMatrix * *(mat4*)&cmd->modelMatrix;
            glUniformMatrix4fv(program.modelViewMatrix,  1, GL_FALSE, glm::value_ptr(modelView));
            glUniformMatrix4fv(program.projectionMatrix, 1, GL_FALSE, glm::value_ptr(renderer->projectionMatrix));

            glm::mat3 normalMatrix = mat3(glm::inverseTranspose(modelView));
            glUniformMatrix3fv(program.normalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));

            if (renderer->pointLight) {
                const Render_Point_Light& light = *renderer->pointLight;

                //v3 cSpaceLightPos = v3(renderer->viewMatrix * v4(light.x, light.y, light.z, 1));
                v3 cSpaceLightPos = v3(renderer->viewMatrix * v4(light.x, light.y, light.z, 1));
                glUniform1i(program.lit, 1);
                glUniform3fv(program.lightPos, 1, glm::value_ptr(cSpaceLightPos));
                glUniform4f(program.lightColor, light.r, light.g, light.b, 1);
            }
            else {
                glUniform1i(program.lit, 0);
            }

            Staged_Static_Mesh* stagedMesh = stage_static_mesh(renderer->staticMeshCache, cmd);
            glBindVertexArray(stagedMesh->vao);

            for (u32 i = 0; i < stagedMesh->groupCount; i++) {
                Staged_Colored_Index_Group& group = stagedMesh->groups[i];

                glUniform1f(program.specularExp, group.specularExp);

                if (group.diffuseMap == GL_INVALID_VALUE) {
                    glUniform1i(program.solid, 1);
                    glUniform3fv(program.color, 1, glm::value_ptr(group.color));
                }
                else {
                    glUniform1i(program.solid, 0);
                    glUniform1i(program.diffuseMap, 0);

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, group.diffuseMap);
                }

                if (group.normalMap == GL_INVALID_VALUE) {
                    glUniform1i(program.hasNormalMap, 0);
                }
                else {
                    glUniform1i(program.hasNormalMap, 1);
                    glUniform1i(program.normalMap, 1);

                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, group.normalMap);
                }

                // TODO: other maps.

                glDrawElements(GL_TRIANGLES, group.indexCount, group.indexType, (void*)(umm)group.indexStart);
            }

            glUseProgram(0);
            glBindVertexArray(0);

            break;
        }
        case RenderCommand_Render_Point_Light: {
            Render_Point_Light* cmd = render_command_after<Render_Point_Light>(header);
            renderer->pointLight = cmd;

            break;
        }
        default:
            break;
        }
    }

    return true;
}

extern void
renderer_end_frame(Memory_Arena* ws, struct ImDrawData* drawData)
{
    // TODO: framebuffer business.

#if !USING_IMGUI
    return;
#endif

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO& io  = ImGui::GetIO();
    int fbWidth  = (int)(drawData->DisplaySize.x * io.DisplayFramebufferScale.x);
    int fbHeight = (int)(drawData->DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0)
        return;

    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    OpenGL_Renderer* renderer = (OpenGL_Renderer*)ws->start;
    ImGui_Resources& imgui    = renderer->imgui;

    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(imgui.program.id);
    glUniform1i(imgui.program.texture, 0);

    v2 topLeft      = drawData->DisplayPos;
    v2 bottomRight  = (v2)drawData->DisplayPos + (v2)drawData->DisplaySize;
    mat4 projection = glm::ortho(topLeft.x, bottomRight.x, bottomRight.y, topLeft.y);
    glUniformMatrix4fv(imgui.program.projectionMatrix, 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(imgui.vao);

    glBindBuffer(GL_ARRAY_BUFFER, imgui.vertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, imgui.indexBuffer);

    glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (void*)offsetof(ImDrawVert, pos));
    glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (void*)offsetof(ImDrawVert, uv));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (void*)offsetof(ImDrawVert, col));

    // TODO: Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
    // TODO: Setup viewport using draw_data->DisplaySize
    // TODO: Setup orthographic projection matrix cover draw_data->DisplayPos to draw_data->DisplayPos + draw_data->DisplaySize
    // TODO: Setup shader: vertex { float2 pos, float2 uv, u32 color }, fragment shader sample color from 1 texture, multiply by vertex color.
    for (int listI = 0; listI < drawData->CmdListsCount; listI++) {
        const ImDrawList* cmdList      = drawData->CmdLists[listI];
        const ImDrawVert* vertexBuffer = cmdList->VtxBuffer.Data;
        const ImDrawIdx*  indexBuffer  = cmdList->IdxBuffer.Data;

        glBufferData(GL_ARRAY_BUFFER, cmdList->VtxBuffer.Size * sizeof(ImDrawVert), vertexBuffer, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx), indexBuffer, GL_STREAM_DRAW);

        const ImDrawIdx* indexBufferOffset = 0;

        for (int cmdI = 0; cmdI < cmdList->CmdBuffer.Size; cmdI++) {
            const ImDrawCmd* cmd = &cmdList->CmdBuffer[cmdI];
            if (cmd->UserCallback) {
                cmd->UserCallback(cmdList, cmd);
            }
            else {
                GLuint texture = (GLuint)(umm)cmd->TextureId;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texture);

                v4 clipRect = v4(cmd->ClipRect.x - topLeft.x, cmd->ClipRect.y - topLeft.y, cmd->ClipRect.z - topLeft.x, cmd->ClipRect.w - topLeft.y);
                if (clipRect.x < fbWidth && clipRect.y < fbHeight && clipRect.z >= 0.0f && clipRect.q >= 0.0f) {
                    // We are using scissoring to clip some objects. All low-level graphics API should supports it.
                    // - If your engine doesn't support scissoring yet, you may ignore this at first. You will get some small glitches
                    //   (some elements visible outside their bounds) but you can fix that once everything else works!
                    // - Clipping coordinates are provided in imgui coordinates space (from draw_data->DisplayPos to draw_data->DisplayPos + draw_data->DisplaySize)
                    //   In a single viewport application, draw_data->DisplayPos will always be (0,0) and draw_data->DisplaySize will always be == io.DisplaySize.
                    //   However, in the interest of supporting multi-viewport applications in the future (see 'viewport' branch on github),
                    //   always subtract draw_data->DisplayPos from clipping bounds to convert them to your viewport space.
                    // - Note that pcmd->ClipRect contains Min+Max bounds. Some graphics API may use Min+Max, other may use Min+Size (size being Max-Min)
                    glScissor((int)cmd->ClipRect.x, (int)(fbHeight - cmd->ClipRect.w),
                              (int)(cmd->ClipRect.z - cmd->ClipRect.x), (int)(cmd->ClipRect.w - cmd->ClipRect.y));

                    // Render 'pcmd->ElemCount/3' indexed triangles.
                    // By default the indices ImDrawIdx are 16-bits, you can change them to 32-bits in imconfig.h if your engine doesn't support 16-bits indices.
                    constexpr GLenum kIndexType = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
                    glDrawElements(GL_TRIANGLES, cmd->ElemCount, kIndexType, indexBufferOffset);
                }
            }

            indexBufferOffset += cmd->ElemCount;
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
}

// AA Demo

static inline void
free_msaa_pass(MSAA_Pass* pass)
{
    assert(pass->sampleCount);

    glDeleteTextures(1, &pass->colorBuffer);
    glDeleteRenderbuffers(1, &pass->depthBuffer);
    glDeleteFramebuffers(1, &pass->framebuffer);

    pass->colorBuffer = GL_INVALID_VALUE;
    pass->depthBuffer = GL_INVALID_VALUE;
    pass->framebuffer = GL_INVALID_VALUE;
    pass->sampleCount = 0;
}

static inline b32
load_msaa_pass(Game_Resolution res, u32 sampleCount, MSAA_Pass* pass)
{
    glGenTextures(1, &pass->colorBuffer);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, pass->colorBuffer);

    // NOTE(blake): if textures(read/write) and render buffers(read only) are mixed, fixedsamplelocations must be GL_TRUE!
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, sampleCount, GL_RGBA8, res.w, res.h, GL_TRUE);

    glGenRenderbuffers(1, &pass->depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, pass->depthBuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCount, GL_DEPTH_COMPONENT32F, res.w, res.h);

    glGenFramebuffers(1, &pass->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, pass->framebuffer);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, pass->colorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pass->depthBuffer);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        log_debug("MSAA %dX framebuffer incomplete! (GL Error: %x)\n", sampleCount, status);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    pass->sampleCount = sampleCount;
    return true;
}

static inline b32
create_color_framebuffer(Game_Resolution res, Color_Framebuffer* fb)
{
    glGenTextures(1, &fb->colorBuffer);
    glBindTexture(GL_TEXTURE_2D, fb->colorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, res.w, res.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &fb->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->colorBuffer, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        log_debug("Final color framebuffer incomplete! (GL Error: %x)\n", status);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

static inline void
render_all_techniques(Memory_Arena* ws, Game_Resolution res,
                      void* execCommands, u32 execCount)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)ws->start;
    OpenGL_AA_Demo&  demo     = renderer->demo;

    // NOTE(blake): In order to keep the final blitting code the same for all techniques and allow
    // the final color buffer to stretch to fit the windows client area, we need to resolve
    // the multisampled color buffers into a non-multisampled color buffer here.

    load_msaa_pass(res, 2, &demo.msaaPass);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, demo.msaaPass.framebuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer_exec(ws, execCommands, execCount);

    // Final, non-multisampled RGB8 color buffer.
    Color_Framebuffer msaa2xColorFb;
    create_color_framebuffer(res, &msaa2xColorFb);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, demo.msaaPass.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, msaa2xColorFb.framebuffer);
    glBlitFramebuffer(0, 0, res.w, res.h, 0, 0, res.w, res.h, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    demo.finalColorFramebuffers[AA_MSAA_2X] = msaa2xColorFb.framebuffer;

    free_msaa_pass(&demo.msaaPass);

    //load_msaa_pass(res, 4, &demo.msaaPass);
    //load_msaa_pass(res, 8, &demo.msaaPass);
    //load_msaa_pass(res, 16, &demo.msaaPass);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

// NOTE(blake): the default win32 framebuffer has a color buffer and a depth buffer.
// This is wasteful for everything but no AA passes, but it's not that big of a deal.

extern void
renderer_demo_aa(Memory_Arena* ws, Game_Resolution res, AA_Technique technique,
                 void* beginCommands, u32 beginCount,
                 void* execCommands,  u32 execCount,
                 struct ImDrawData* endFrameDrawData)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)ws->start;
    OpenGL_AA_Demo&  demo     = renderer->demo;

    // Handle commands like `set viewport`, but don't clear any framebuffer attachments.
    renderer_begin_frame(ws, beginCommands, beginCount);

    if (!demo.on) {
        render_all_techniques(ws, res, execCommands, execCount);
        demo.on = true;
    }

    // Blit the final color buffer to the default back buffer.

    GLuint finalFramebuffer = demo.finalColorFramebuffers[AA_MSAA_2X];
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, finalFramebuffer);
    glDrawBuffer(GL_BACK);

    log_debug("Res: %u %u\n", gGame->clientRes.w, gGame->clientRes.h);
    log_debug("Res2: %u %u\n", res.w, res.h);
    glBlitFramebuffer(0, 0, res.w, res.h, 0, 0, gGame->clientRes.w, gGame->clientRes.h, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // Avoid having to clear the depth buffer to draw the UI. Currently, this is redundant.
    glDisable(GL_DEPTH_TEST);
    renderer_end_frame(ws, endFrameDrawData);
    glEnable(GL_DEPTH_TEST);
}

extern void
renderer_stop_aa_demo(Memory_Arena* ws)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)ws->start;
    OpenGL_AA_Demo&  demo     = renderer->demo;

    // TODO: free framebuffers
    demo.on = false;
}
