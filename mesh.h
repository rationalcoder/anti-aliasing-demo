#pragma once
#include <GL/gl3w.h>
#include "platform.h"

enum Index_Size
{
    IndexSize_u8  = sizeof(u8),
    IndexSize_u16 = sizeof(u16),
    IndexSize_u32 = sizeof(u32),
};

enum Texture_Map_Type : GLuint
{
    TextureMapType_Diffuse,
    TextureMapType_Normal,
    TextureMapType_Specular,
    TextureMapType_Num_,
};

struct Raw_Mesh
{
    f32*  vertices = nullptr;
    f32*  normals  = nullptr;
    f32*  uvs      = nullptr;
    void* indices  = nullptr;

    u32 vertexCount = 0;
    u32 indexCount  = 0;

    Index_Size indexSize = IndexSize_u16;
    u32 primitive = 0; // assimp primitive values

    b32 has_uvs()     const { return uvs     != nullptr; }
    b32 has_indices() const { return indices != nullptr; }
    b32 has_normals() const { return normals != nullptr; }
};

struct Staged_Mesh
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
