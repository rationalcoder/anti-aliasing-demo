#pragma once
#include <GL/gl3w.h>
#include <glm/matrix.hpp>

#include "common.h"
#include "memory.h"
#include "mesh.h"
#include "containers.h"

#if 0 // @RemoveMe
struct Staged_Static_Mesh
{
    GLuint vertexBuffer = GL_INVALID_VALUE;
    GLuint normalBuffer = GL_INVALID_VALUE;
    GLuint uvBuffer     = GL_INVALID_VALUE;
    GLuint indexBuffer  = GL_INVALID_VALUE;
    GLuint arrayObject  = GL_INVALID_VALUE;

    GLuint diffuseMap  = GL_INVALID_VALUE;
    GLuint normalMap   = GL_INVALID_VALUE;
    GLuint specularMap = GL_INVALID_VALUE;

    GLenum primitive = GL_INVALID_ENUM;
    GLenum indexType = GL_INVALID_ENUM;

    u32 vertexCount = 0;
    u32 indexCount  = 0;

    b32 has_diffuse_map()  const { return diffuseMap  != GL_INVALID_VALUE; }
    b32 has_normal_map()   const { return normalMap   != GL_INVALID_VALUE; }
    b32 has_specular_map() const { return specularMap != GL_INVALID_VALUE; }
};
#endif

struct Staged_Colored_Index_Group
{
    GLuint diffuseMap  = GL_INVALID_VALUE;
    GLuint normalMap   = GL_INVALID_VALUE;
    GLuint specularMap = GL_INVALID_VALUE;
    GLuint emissiveMap = GL_INVALID_VALUE;

    v3 color;
    f32 specularExp = 0;

    u32 indexStart = 0;
    u32 indexCount = 0;
    GLenum indexType = GL_INVALID_ENUM;
};

struct Staged_Static_Mesh
{
    GLuint vao = GL_INVALID_VALUE;

    Staged_Colored_Index_Group* groups = nullptr;
    u32 groupCount = 0;
};

struct Static_Mesh_Program
{
    GLuint id;

    GLint modelViewMatrix;
    GLint projectionMatrix;
    GLint normalMatrix;

    GLint solid;
    GLint color;

    GLint diffuseMap;
    GLint normalMap;
    GLint specularMap;

    GLint hasNormalMap;
    GLint hasSpecularMap;
    GLint specularExp;

    GLint lit;
    GLint lightPos;
    GLint lightColor;
};

struct Lines_Program
{
    GLuint id;

    GLint mvp;
    GLint color;
};

struct Cubes_Program
{
    GLuint id;

    GLint viewProjection;
    GLint centers;
    GLint color;
    GLint scale;
};

struct Textured_Quad_Program
{
    GLuint program;
};

struct FXAA_Program
{
    GLuint program;

    GLint texelStep;
    GLint showEdges;
    GLint on;
    GLint lumaThreshold;
    GLint mulReduce;
    GLint minReduce;
    GLint maxSpan;
    GLint tex;
};

struct Shader_Catalog
{
    GLuint basicVertexShader          = GL_INVALID_VALUE;
    GLuint basicInstancedVertexShader = GL_INVALID_VALUE;
    GLuint solidFramentShader         = GL_INVALID_VALUE;
    GLuint staticMeshVertexShader     = GL_INVALID_VALUE;
    GLuint staticMeshFragmentShader   = GL_INVALID_VALUE;
};


struct Rolling_Handle
{
    GLuint handle;
    u32 id;
};

struct Rolling_Cache
{
    Rolling_Handle* handles = nullptr;
    u32 size      = 32;
    u32 filled    = 0;
    u32 oldest    = 0;
    u32 runningId = 1;

    auto reset(u32 size, Rolling_Handle* handles) -> void;
    auto add(Rolling_Handle** result) -> b32;
    auto get(u32 id, GLuint* handle) -> b32;
    auto evict() -> GLuint;
};

inline void
Rolling_Cache::reset(u32 size, Rolling_Handle* handles)
{
    filled    = 0;
    oldest    = 0;
    runningId = 1;

    this->size    = size;
    this->handles = handles;
}

inline b32
Rolling_Cache::add(Rolling_Handle** result)
{
    if (filled == size) return false;

    Rolling_Handle* overwritten = &handles[oldest];
    Rolling_Handle handle = { GL_INVALID_VALUE, runningId };
    *overwritten = handle;

    right_rotate_value(oldest,    0, size-1);
    right_rotate_value(runningId, 1, MAX_UINT(u32));

    *result = overwritten;
    filled++;
    return true;
}

inline b32
Rolling_Cache::get(u32 id, GLuint* handle)
{
    Rolling_Handle& candidate = handles[id-1];
    if (!id || candidate.id != id)
        return false;

    *handle = candidate.handle;
    return true;
}

inline GLuint
Rolling_Cache::evict()
{
    assert(filled);
    filled--;

    return handles[oldest].handle;
}


struct OpenGL_Renderer
{
    Shader_Catalog shaderCatalog;

    Lines_Program linesProgram;
    Cubes_Program cubesProgram;
    Static_Mesh_Program staticMeshProgram;

    GLuint debugCubeVertexBuffer;
    GLuint debugCubeIndexBuffer;

    Rolling_Cache debugLinesCache;
    Rolling_Cache debugCubesCache;
    Rolling_Cache staticMeshCache;

    mat4 viewMatrix = mat4(1);
    mat4 projectionMatrix = mat4(1);

    // TODO(blake): more lights!
    struct Render_Point_Light* pointLight;
};
