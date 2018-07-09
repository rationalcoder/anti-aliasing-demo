#pragma once
#include <GL/gl3w.h>
#include <glm/matrix.hpp>
#include "memory.h"

struct Static_Mesh_Program
{
    GLuint id;

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

struct Lines_Program
{
    GLuint id;

    GLuint mvp;
    GLuint color;
};

struct Textured_Quad_Program
{
    GLuint program;
};

struct FXAA_Program
{
    GLuint program;
    GLuint texelStep;
    GLuint showEdges;
    GLuint on;
    GLuint lumaThreshold;
    GLuint mulReduce;
    GLuint minReduce;
    GLuint maxSpan;
    GLuint tex;
};

struct Referenced_Handle
{
    GLuint handle;
    u32 id;
};

struct Debug_Lines_Storage
{
    Referenced_Handle handles[32];
    u32 filled = 0;
    u32 runningId = 1;
};

struct Static_Mesh_Storage
{
};

struct OpenGL_Renderer
{
    Lines_Program linesProgram;
    Static_Mesh_Program staticMeshProgram;

    Debug_Lines_Storage debugLinesStorage;
    Static_Mesh_Storage staticMeshStorage;

    glm::mat4 viewMatrix = glm::mat4(1);
    glm::mat4 projectionMatrix = glm::mat4(1);
};
