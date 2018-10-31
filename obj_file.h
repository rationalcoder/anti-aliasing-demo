#pragma once
#include "common.h"
#include "memory.h"

struct OBJ_Material_Group
{
    buffer32 material;
    u32 startingIndex;
};

struct OBJ_File
{
    const char* error;
    buffer32 mtllib;

    OBJ_Material_Group* groups;

    v3* vertices;
    v3* normals;
    v2* uvs;

    void* indices;
    u32 indexSize;

    u32 vertexCount;
    u32 indexCount;
    u32 groupCount;
};

struct MTL_Material
{
    buffer32 name;

    v3 ambientColor;
    v3 diffuseColor;
    v3 specularColor;
    v3 emissiveColor;

    f32 specularExponent;
    f32 opacity;
    int illum;

    buffer32 ambientMap;
    buffer32 diffuseMap;
    buffer32 specularMap;
    buffer32 emissiveMap;
    buffer32 normalMap;
};

struct MTL_File
{
    const char* error;

    MTL_Material* materials;
    u32 materialCount;
};

extern OBJ_File
parse_obj_file(buffer32 buffer);

extern MTL_File
parse_mtl_file(buffer32 buffer);
