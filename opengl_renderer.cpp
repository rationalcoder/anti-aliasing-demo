#include "renderer.h"

#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/gl3w.h>
#include "memory.h"
#include "tanks.h"
#include "opengl_renderer.h"

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

static inline buffer32
read_file_buffer(const char* file)
{
    buffer32 result(uninitialized);

    umm size = 0;
    result.data = (u8*)gPlatform->read_entire_file(file, &gMem->file, &size, 1);
    if (!result.data) log_debug("Failed to read \"%s\"\n", file);

    result.size = down_cast<u32>(size);
    return result;
}

static inline b32
load_program(const char* vsFile, const char* fsFile, GLuint* program)
{
    buffer32 vsSource = read_file_buffer(vsFile);
    if (!vsSource) return false;

    buffer32 fsSource = read_file_buffer(fsFile);
    if (!fsSource) return false;

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    if (vs == GL_INVALID_VALUE) return false;
    defer( glDeleteShader(vs) );

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    if (!fs) { glDeleteShader(vs); return false; }
    defer( glDeleteShader(fs) );

    GLchar errorBuffer[256];
    if (!compile_shader_from_source(vs, vsSource)) {
        glGetShaderInfoLog(vs, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to compile \"%s\": %s\n", vsFile, errorBuffer);
        return false;
    }

    if (!compile_shader_from_source(fs, fsSource)) {
        glGetShaderInfoLog(fs, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to compile \"%s\": %s\n", fsFile, errorBuffer);
        return false;
    }

    if (!create_and_link_program(vs, fs, program)) {
        glGetProgramInfoLog(*program, sizeof(errorBuffer), nullptr, errorBuffer);
        log_crit("Failed to link program from \"%s\" and \"%s\": %s\n", vsFile, fsFile, errorBuffer);
        return false;
    }

    return true;
}

static inline b32
load_lines_program(Lines_Program* program)
{
    if (!load_program("demo/lines.vs", "demo/lines.fs", &program->id)) {
        log_crit("Failed to load lines program!\n");
        return false;
    }

    program->mvp   = glGetUniformLocation(program->id, "u_mvp");
    program->color = glGetUniformLocation(program->id, "u_color");

    return true;
}

static inline b32
load_static_mesh_program(Static_Mesh_Program* program)
{
    if (!load_program("demo/static_mesh.vs", "demo/static_mesh.fs", &program->id)) {
        log_crit("Failed to load static mesh program!\n");
        return false;
    }

    program->modelMatrix      = glGetUniformLocation(program->id, "u_modelMatrix");
    program->viewMatrix       = glGetUniformLocation(program->id, "u_viewMatrix");
    program->projectionMatrix = glGetUniformLocation(program->id, "u_projectionMatrix");
    program->normalMatrix     = glGetUniformLocation(program->id, "u_normalMatrix");

    program->diffuseMap  = glGetUniformLocation(program->id, "u_diffuse");
    program->normalMap   = glGetUniformLocation(program->id, "u_normal");
    program->specularMap = glGetUniformLocation(program->id, "u_specular");

    program->hasNormalMap   = glGetUniformLocation(program->id, "u_hasNormalMap");
    program->hasSpecularMap = glGetUniformLocation(program->id, "u_hasSpecularMap");

    program->specularExp = glGetUniformLocation(program->id, "u_specularExp");
    program->lit         = glGetUniformLocation(program->id, "u_lit");
    program->lightPos    = glGetUniformLocation(program->id, "u_pointLightP");
    program->lightColor  = glGetUniformLocation(program->id, "u_pointLightC");

    return true;
}

static inline GLuint
bind_debug_lines(Debug_Lines_Storage* storage, Render_Debug_Lines* cmd)
{
    // If this command has already been staged and hasn't been evicted, return its vao.
    u32 id = down_cast<u32>((uptr)cmd->_staged);
    if (id) {
        Referenced_Handle handle = storage->handles[id-1];
        if (id == handle.id) {
            glBindVertexArray(handle.handle);
            return handle.handle;
        }
    }

    u32 filled = storage->filled;

    if (filled == ArraySize(storage->handles)) {
        glDeleteVertexArrays(1, &storage->handles[0].handle);
        storage->handles[0] = storage->handles[filled-1];
        filled--;
    }

    Referenced_Handle& invalidatedHandle = storage->handles[filled];
    invalidatedHandle.id = storage->runningId;
    cmd->_staged = (void*)(uptr)storage->runningId;
    storage->runningId++;

    GLuint* newVao = &invalidatedHandle.handle;
    glGenVertexArrays(1, newVao);
    glBindVertexArray(*newVao); // left bound on return.
    glEnableVertexAttribArray(0);

    GLuint vbo = GL_INVALID_VALUE;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, cmd->vertexCount * 3 * sizeof(f32), cmd->vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glBindVertexArray(*newVao);

    storage->filled = filled + 1;
    return *newVao;
}

static inline GLuint
bind_static_mesh(Static_Mesh_Storage*, Render_Debug_Lines* cmd)
{
    if (cmd->_staged) return *((GLuint*)cmd->_staged);
    return GL_INVALID_VALUE;
}

extern b32
renderer_init(Memory_Arena* storage, Memory_Arena* workspace)
{
    *workspace = arena_sub_allocate(*storage, Kilobytes(4), 16, "Rendering Workspace");
    OpenGL_Renderer* renderer = arena_push_new(*workspace, OpenGL_Renderer);

    if (!load_lines_program(&renderer->linesProgram)) return false;
    if (!load_static_mesh_program(&renderer->staticMeshProgram)) return false;

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
            glViewport(cmd->x, cmd->y, cmd->width, cmd->height);
            break;
        }
        case RenderCommand_Set_View_Matrix: {
            Set_View_Matrix* cmd = render_command_after<Set_View_Matrix>(header);
            copy_memory(&renderer->viewMatrix, cmd->mat4, sizeof(renderer->viewMatrix));
            break;
        }
        case RenderCommand_Set_Projection_Matrix: {
            Set_Projection_Matrix* cmd = render_command_after<Set_Projection_Matrix>(header);
            copy_memory(&renderer->projectionMatrix, cmd->mat4, sizeof(renderer->projectionMatrix));
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

            glm::mat4 mvp = renderer->projectionMatrix * renderer->viewMatrix;
            glUniformMatrix4fv(program.mvp, 1, GL_FALSE, glm::value_ptr(mvp));
            glUniform4f(program.color, cmd->r, cmd->g, cmd->b, cmd->a);

            bind_debug_lines(&renderer->debugLinesStorage, cmd);
            glDrawArrays(GL_LINES, 0, cmd->vertexCount);
            glBindVertexArray(0);
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
