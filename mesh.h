#pragma once
#include "platform.h"

enum Index_Size
{
    IndexSize_u8  = sizeof(u8),
    IndexSize_u16 = sizeof(u16),
    IndexSize_u32 = sizeof(u32),
};

enum Texture_Map_Type
{
    TextureMapType_Diffuse,
    TextureMapType_Normal,
    TextureMapType_Specular,
    TextureMapType_Num_,
};

enum Primitive
{
    Primitive_Points,
    Primitive_Lines,
    Primitive_Triangles,
    Primitive_Num_,
};

struct Static_Mesh
{
    f32*  vertices = nullptr;
    f32*  normals  = nullptr;
    f32*  uvs      = nullptr;
    void* indices  = nullptr;

    u32 vertexCount = 0;
    u32 indexCount  = 0;

    Index_Size indexSize = IndexSize_u16;
    Primitive primitive  = Primitive_Triangles;

    b32 has_uvs()     const { return uvs     != nullptr; }
    b32 has_indices() const { return indices != nullptr; }
    b32 has_normals() const { return normals != nullptr; }
};
