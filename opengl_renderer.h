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
    GLuint id;

    GLint texelStep;
    GLint showEdges;
    GLint on;
    GLint lumaThreshold;
    GLint mulReduce;
    GLint minReduce;
    GLint maxSpan;
    GLint colorTexture;
};

struct Shader_Catalog
{
    GLuint basicVertexShader          = GL_INVALID_VALUE;
    GLuint basicInstancedVertexShader = GL_INVALID_VALUE;
    GLuint solidFramentShader         = GL_INVALID_VALUE;
    GLuint staticMeshVertexShader     = GL_INVALID_VALUE;
    GLuint staticMeshFragmentShader   = GL_INVALID_VALUE;
    GLuint fxaaVertexShader           = GL_INVALID_VALUE;
    GLuint fxaaFragmentShader         = GL_INVALID_VALUE;
};

// @RemoveMe(blake): OpenGL can do a better job w/o context, and
// we can do a better job _with_ context. This just adds complexity.
struct Rolling_Cache {};
#if 0
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
#endif

struct ImGui_Program
{
    GLuint id               = GL_INVALID_VALUE;
    GLint  projectionMatrix = GL_INVALID_VALUE;
    GLint  texture          = GL_INVALID_VALUE;
};

struct ImGui_Resources
{
    ImGui_Program program;

    GLuint textureAtlas = GL_INVALID_VALUE;
    GLuint vao          = GL_INVALID_VALUE;
    GLuint vertexBuffer = GL_INVALID_VALUE;
    GLuint indexBuffer  = GL_INVALID_VALUE;
};

struct FXAA_Pass
{
    GLuint emptyVao     = GL_INVALID_VALUE;

    // Uniforms
    bool on           = true;
    bool showEdges    = false;
    //f32 lumaThreshold = 1/8.0f;
    f32 lumaThreshold = 1/2.0f;
    //f32 mulReduce     = 1.0f; // super sharp/weak
    f32 mulReduce     = 1.0f/3.0f;
    f32 minReduce     = 1/256.0f;
    f32 maxSpan       = 6;
};

struct MSAA_Pass
{
    GLuint framebuffer = GL_INVALID_VALUE;
    GLuint colorBuffer = GL_INVALID_VALUE; // texture
    GLuint depthBuffer = GL_INVALID_VALUE; // renderbuffer
    u32    sampleCount = 0;
};

struct Framebuffer
{
    GLuint id       = GL_INVALID_VALUE;
    GLuint color    = GL_INVALID_VALUE;
    GLuint depth    = GL_INVALID_VALUE;
    u32 sampleCount = 0;
};

struct OpenGL_AA_Demo
{
    b32 on = false;

    FXAA_Pass fxaaPass;
    MSAA_Pass msaaPass;

    Framebuffer finalColorFramebuffers[AA_COUNT_]; // [AA_INVALID] == invalid values.
};

struct OpenGL_AA_State
{
    AA_Technique technique = AA_NONE;

    Framebuffer fxaaInputFbo; // for AA_FXAA

    // NOTE(blake): I decided this was a bad idea. I originally thought it would be nice
    // to have MSAA work with arbitrary window sizes. To do that, you have to blit to
    // a separate off-screen buffer and blit that to the final arbitrary resolution.
    // This is REALLY slow, enough to warrant the disabling of arbitrary window resizing
    // completely. We should query supported display resolutions somehow and allow
    // switching to just those.
    //
    // Framebuffer msaaResolveFbo; // for AA_MSAA with no FXAA

    // Needed b/c my FXAA shader doesn't do a custom multisample resolve.
    // NOTE(blake): this makes MSAA + FXAA performance comparisons unfair.
    //
    Framebuffer msaaResolveFbo; // for AA_MSAA _with_ FXAA

    MSAA_Pass msaaPass;
    FXAA_Pass fxaaPass;

    b32 msaaOn = false;
    b32 fxaaOn = false;
};

struct OpenGL_Renderer
{
    Shader_Catalog shaderCatalog;

    Lines_Program linesProgram;
    Cubes_Program cubesProgram;
    Static_Mesh_Program staticMeshProgram;
    FXAA_Program fxaaProgram;

    GLuint debugCubeVertexBuffer = GL_INVALID_VALUE;
    GLuint debugCubeIndexBuffer  = GL_INVALID_VALUE;

    ImGui_Resources imgui;

    mat4 viewMatrix       = mat4(1);
    mat4 projectionMatrix = mat4(1);

    OpenGL_AA_State aaState;
    Game_Resolution res = { 1280, 720 }; // @Temporary

    // TODO(blake): more lights! Light groups!
    struct Render_Point_Light* pointLight;


    // @Temporary
    OpenGL_AA_Demo aaDemo;
    b32 aaDemoThisFrame = false;
};

