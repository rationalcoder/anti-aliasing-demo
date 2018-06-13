#pragma once

#include <assimp/scene.h>
#include "platform.h"
#include "mesh.h"

enum Texture_Format
{
    TextureFormat_Grey,
    TextureFormat_GreyAlpha,
    TextureFormat_RGB,
    TextureFormat_RGBA,
    TextureFormat_Num_
};

struct Texture_Result
{
    u8* bytes                    = nullptr;
    Texture_Format format        = TextureFormat_Num_;
    aiTextureMapping mappingType = aiTextureMapping_UV;
    aiTextureMapMode mappingMode = aiTextureMapMode_Wrap;
    aiTextureOp op               = aiTextureOp_Add;
    f32 blendFactor              = 1.0f;
    s32 x                        = 0;
    s32 y                        = 0;
    aiString fileName;
};

b32 load_static_meshes(const aiScene* scene, Raw_Mesh* meshes);
b32 load_animated_meshes(const aiScene* scene, Raw_Mesh* meshes);
b32 load_texture(const aiScene* scene, u32 meshIndex, const char* texDir, aiTextureType type, Texture_Result* result);

void make_hello_triangle(Raw_Mesh* mesh);
void make_debug_cube(Raw_Mesh* mesh, f32 halfWidth);
void free_mesh(Raw_Mesh* mesh);
void free_meshes(Raw_Mesh* meshes, u32 n);
void free_texture(Texture_Result* result);
void free_textures(Texture_Result* results, u32 n);
