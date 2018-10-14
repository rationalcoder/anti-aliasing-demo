#pragma once
#include "common.h"
#include "memory.h"

struct OBJ_Textured_Group
{
    buffer32 material;
    u32 startingIndex;
};

struct OBJ_File
{
    const char* error;
    buffer32 mtllib;

    OBJ_Textured_Group* groups;

    v3* vertices;
    v3* normals;
    v3* uvs;

    void* indices;
    u32 indexSize;

    u32 vertexCount;
    u32 indexCount;
    u32 groupCount;
};

extern OBJ_File
parse_obj_file(buffer32 buffer);

