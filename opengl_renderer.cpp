#include "renderer.h"

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <GL/gl3w.h>

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
to_gl_format(Texture_Format format, b32 srgb)
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
            result.internal = GL_SRGB8;
            break;
        case TextureFormat_RGBA:
            result.upload   = GL_RGBA;
            result.internal = GL_SRGB8_ALPHA8;
            break;
        }
    }

    return result;
}

static inline GLuint
stage_texture(Texture texture, b32 srgb, b32 mipmaps, GLenum wrapType)
{
    GLuint handle = GL_INVALID_VALUE;
    glGenTextures(1, &handle);

    glBindTexture(GL_TEXTURE_2D, handle);

    GL_Format format = to_gl_format(texture.format, srgb);
    glTexImage2D(GL_TEXTURE_2D, 0, format.internal, texture.x, texture.y, 0, format.upload, GL_UNSIGNED_BYTE, texture.data);

    GLuint minFilter = GL_LINEAR;
    if (mipmaps) {
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

        stagedGroup.diffuseMap  = stage_texture(group.diffuseMap,  true, true, GL_REPEAT);
        stagedGroup.normalMap   = stage_texture(group.normalMap,   true, true, GL_NEAREST);
        stagedGroup.specularMap = stage_texture(group.specularMap, true, true, GL_NEAREST);
        stagedGroup.emissiveMap = stage_texture(group.emissiveMap, true, true, GL_REPEAT);
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

    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_CULL_FACE);

    glCullFace(GL_BACK);

    return true;
}

extern b32
renderer_exec(Memory_Arena* workspace, void* commands, u32 count)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;
    (void)renderer;

    renderer->pointLight = nullptr;

    allocator_scope(workspace);

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
        case RenderCommand_Begin_Render_Pass: {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            break;
        }
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

            glUniform1i(program.diffuseMap,  0);
            glUniform1i(program.normalMap,   1);
            glUniform1i(program.specularMap, 2);

            if (renderer->pointLight) {
                const Render_Point_Light& light = *renderer->pointLight;

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
