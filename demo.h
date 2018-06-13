#pragma once
#include <glm/common.hpp>
#include "stb_truetype.h"

#include "common.h"
#include "platform.h"
#include "mesh.h"
#include "camera.h"

struct Point_Light
{
    glm::vec4 position;
    glm::vec4 color;
};

struct XY_Plane
{
    GLuint vertexBuffer = GL_INVALID_VALUE;
    GLuint vao          = GL_INVALID_VALUE;
    u32 numLines        = 0;
};

struct Test_Mesh
{
    Staged_Mesh* subMeshes = nullptr;
    u32 subMeshCount       = 0;
    glm::mat4 modelMatrix = glm::mat4(1);
    b32 lit = true;
};

struct Common_Locations
{
    GLuint linesMVP;
    GLuint linesColor;

    GLuint modelMatrix;
    GLuint viewMatrix;
    GLuint projectionMatrix;
    GLuint normalMatrix;

    GLuint diffuseMap;
    GLuint normalMap;
    GLuint specularMap;

    GLuint hasNormalMap;
    GLuint hasSpecularMap;
    GLuint specularExp;

    GLuint lit;
    GLuint lightPos;
    GLuint lightColor;
};

struct FXAA_Locations
{
    GLuint texelStep;
    GLuint showEdges;
    GLuint on;
    GLuint lumaThreshold;
    GLuint mulReduce;
    GLuint minReduce;
    GLuint maxSpan;
    GLuint tex;
};

struct UI
{
    stbtt_fontinfo fontInfo;
    GLuint program = GL_INVALID_VALUE;
};

struct Common_Resources
{
    Camera camera;
    glm::mat4 projection;

    XY_Plane xyPlane;

    Staged_Mesh floor;
    Staged_Mesh debugCube;
    Point_Light pointLight;

    UI ui;

    Test_Mesh* testMeshes = nullptr;
    u32 testMeshCount     = 0;

    Common_Locations shaderLocations;

    GLuint staticMeshProgram = GL_INVALID_VALUE;
    GLuint linesProgram      = GL_INVALID_VALUE;
};

struct FXAA_Pass
{
    FXAA_Locations shaderLocations;

    GLuint program     = GL_INVALID_VALUE;
    GLuint emptyVao    = GL_INVALID_VALUE;

    GLuint tex         = GL_INVALID_VALUE;
    GLuint fb          = GL_INVALID_VALUE;
    GLuint depthBuffer = GL_INVALID_VALUE;
};

struct MSAA_Pass
{
    GLuint tex         = GL_INVALID_VALUE;
    GLuint fb          = GL_INVALID_VALUE;
    GLuint depthBuffer = GL_INVALID_VALUE;
    u32 sampleCount    = 0;
};

enum AA_Technique
{
    AAT_MSAA_4X,
    AAT_MSAA_8X,
    AAT_MSAA_16X,
    AAT_FXAA,
    AAT_None,
};

struct Demo_Scene
{
    Common_Resources common;
    FXAA_Pass fxaaPass;
    MSAA_Pass msaaPass;
    AA_Technique currentTechnique = AAT_MSAA_4X;
};


extern b32
demo_load(Demo_Scene* scene, Game_Resolution clientRes, Game_Resolution closestRes);

extern void
demo_update(Demo_Scene* scene, struct Game_Input* input, f32 dt);

extern void
demo_resize(Demo_Scene* scene, Game_Resolution clientRes, Game_Resolution closestRes);

extern void
demo_render(Demo_Scene* scene);

extern void
demo_next_technique(Demo_Scene* scene);

extern void
demo_free(Demo_Scene* scene);
