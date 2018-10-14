#include "renderer.h"

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

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

    program->modelMatrix      = glGetUniformLocation(id, "u_modelMatrix");
    program->viewMatrix       = glGetUniformLocation(id, "u_viewMatrix");
    program->projectionMatrix = glGetUniformLocation(id, "u_projectionMatrix");
    program->normalMatrix     = glGetUniformLocation(id, "u_normalMatrix");

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
bind_debug_cubes(GLuint vbo, GLuint ebo, Rolling_Cache& cache, Render_Debug_Cubes* cmd)
{
    ((void)cache);
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

static inline GLuint
bind_static_mesh(Rolling_Cache&, Render_Debug_Lines* cmd)
{
    if (cmd->_staged) return *((GLuint*)cmd->_staged);
    return GL_INVALID_VALUE;
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

    return true;
}

extern b32
renderer_exec(Memory_Arena* workspace, void* commands, u32 count)
{
    OpenGL_Renderer* renderer = (OpenGL_Renderer*)workspace->start;
    (void)renderer;

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
            glUseProgram(renderer->staticMeshProgram.id);
            break;
        }
        case RenderCommand_Render_Point_Light: {
            break;
        }
        default:
            break;
        }
    }

    return true;
}
