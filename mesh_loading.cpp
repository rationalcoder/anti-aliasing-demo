#include "mesh_loading.h"

#include <cassert>
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "common.h"
#include "platform.h"
#include "stb_image.h"

static inline Index_Size
index_size_from_count(u32 numIndices);

template <typename T_> static inline void
copy_indices(const aiMesh& in, void* out);

extern b32
load_static_meshes(const aiScene* scene, Raw_Mesh* outMeshes)
{
    aiMesh const* const* inMeshes = scene->mMeshes;
    u32 numMeshes = scene->mNumMeshes;
    for (u32 i = 0; i < numMeshes; i++) {
        const aiMesh& inMesh  = *inMeshes[i];
        Raw_Mesh& outMesh = outMeshes[i];

        // Skip non-triangle sub meshes.
        if (inMesh.mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
            printf("skipping non-triangle mesh: %s\n", inMesh.mName.C_Str());
            continue;
        }

        const u32 numVertices = inMesh.mNumVertices;
        const u32 numIndices  = inMesh.mNumFaces * 3;

        const Index_Size indexTypeSize    = index_size_from_count(numIndices);
        const u32        indexBufferSize  = numIndices  * 1 * indexTypeSize;
        const u32        uvBufferBytes    = numVertices * 2 * sizeof(f32);
        const u32        vertexBufferSize = numVertices * 3 * sizeof(f32);

        assert(inMesh.HasTextureCoords(0) && "No UVs not implemented.");
        assert(inMesh.HasFaces() && "No Indices not implemented.");
        //assert(inMesh.HasNormals() && "No normals not implemented.");

        outMesh.vertices = (f32*)malloc(vertexBufferSize);
        outMesh.uvs      = (f32*)malloc(uvBufferBytes);
        outMesh.indices  =       malloc(indexBufferSize);

        outMesh.vertexCount = numVertices;
        outMesh.indexCount  = numIndices;

        // We will always have vertices...
        memcpy(outMesh.vertices, inMesh.mVertices, vertexBufferSize);

        if (inMesh.HasNormals()) {
            outMesh.normals = (f32*)malloc(vertexBufferSize);
            memcpy(outMesh.normals,  inMesh.mNormals,  vertexBufferSize);
        }

        // We have to manually copy over uvs, skipping the third coord, w.
        const aiVector3D* inUvs  = inMesh.mTextureCoords[0];
        aiVector2D*       outUvs = (aiVector2D*)outMesh.uvs;

        for (u32 i = 0; i < numVertices; i++)
            outUvs[i] = aiVector2D(inUvs[i].x, inUvs[i].y);

        // Copy over indices, truncating to the right type.
        if (indexTypeSize == IndexSize_u32)
            copy_indices<u32>(inMesh, outMesh.indices);
        else if (indexTypeSize == IndexSize_u16)
            copy_indices<u16>(inMesh, outMesh.indices);
        else
            copy_indices<u8>(inMesh, outMesh.indices);

        outMesh.primitive = inMesh.mPrimitiveTypes;
        outMesh.indexSize = indexTypeSize;
    }

    return true;
}

extern b32
load_animated_meshes(const aiScene*, Raw_Mesh*)
{
    // TODO: what is needed for animation?
    return false;
}

extern void
free_meshes(Raw_Mesh* meshes, u32 n)
{
    for (u32 i = 0; i < n; i++)
        free_mesh(meshes + i);
}

extern void
free_mesh(Raw_Mesh* mesh)
{
    free(mesh->vertices);
    free(mesh->uvs);
    free(mesh->normals);
    free(mesh->indices);

    memset(mesh, 0, sizeof(*mesh));
}

extern b32
load_texture(const aiScene* scene, u32 meshIndex, const char* texdir,
             aiTextureType type, Texture_Result* result)
{
    u32 matIndex = scene->mMeshes[meshIndex]->mMaterialIndex;
    const aiMaterial* material = scene->mMaterials[matIndex];
    if (material->GetTextureCount(type) == 0)
        return false;

    u32 uvIndex = 0;
    material->GetTexture(type, 0, &result->fileName, &result->mappingType,
                         &uvIndex, &result->blendFactor, &result->op, &result->mappingMode);

    aiString fullPath;
    fullPath.Set(texdir);
    fullPath.Append(result->fileName.C_Str());

    // Get how many color channels are actually in the file.
    int numChannels = 0;
    result->bytes = stbi_load(fullPath.C_Str(), &result->x, &result->y, &numChannels, 0);
    if (!result->bytes) {
        fprintf(stderr, "STBI ERROR: %s\n", stbi_failure_reason());
        return false;
    }

    result->format = (Texture_Format)(numChannels - 1);
    return true;
}

extern void
free_texture(Texture_Result* result)
{
    stbi_image_free(result->bytes);
}

extern void
free_textures(Texture_Result* results, u32 n)
{
    for (u32 i = 0; i < n; i++)
        stbi_image_free(results[i].bytes);
}

// Helpers:

static inline Index_Size
index_size_from_count(u32 numIndices)
{
    if (numIndices <= UINT8_MAX) return IndexSize_u8;
    if (numIndices <= UINT16_MAX) return IndexSize_u16;

    return IndexSize_u32;
}

template <typename T_> static inline void
copy_indices(const aiMesh& in, void* out)
{
    u32  numFaces = in.mNumFaces;
    aiFace* faces = in.mFaces;

    T_* outIndices = (T_*)out;
    for (u32 i = 0; i < numFaces; i++) {
        const u32  indexStart = i*3;
        const u32* inIndices  = faces[i].mIndices;
        assert(faces[i].mNumIndices == 3 && "Excuse me, mister supposed to be triangle?");

        outIndices[indexStart + 0] = (T_)inIndices[0];
        outIndices[indexStart + 1] = (T_)inIndices[1];
        outIndices[indexStart + 2] = (T_)inIndices[2];
    }
}

// Testing / Debugging

extern void
make_debug_cube(Raw_Mesh* mesh, f32 halfWidth)
{
    f32 vertices[] = {
        -halfWidth, -halfWidth,  halfWidth, // ftl 0
         halfWidth, -halfWidth,  halfWidth, // ftr 1
        -halfWidth, -halfWidth, -halfWidth, // fbl 2
         halfWidth, -halfWidth, -halfWidth, // fbr 3

        -halfWidth,  halfWidth,  halfWidth, // btl 4
         halfWidth,  halfWidth,  halfWidth, // btr 5
        -halfWidth,  halfWidth, -halfWidth, // bbl 6
         halfWidth,  halfWidth, -halfWidth, // bbr 7
    };

    u8 indices[] = {
        // front
        0, 2, 3,
        3, 1, 0,

        // back
        5, 7, 6,
        6, 4, 5,

        // top
        4, 0, 5,
        5, 0, 1,

        // bottom
        2, 6, 7,
        7, 3, 2,

        // left
        4, 6, 2,
        2, 0, 4,

        // right
        1, 3, 7,
        7, 5, 1,
    };

    mesh->vertices = (f32*)malloc(sizeof(vertices));
    mesh->indices  =       malloc(sizeof(indices));

    memcpy(mesh->vertices, vertices, sizeof(vertices));
    memcpy(mesh->indices, indices, sizeof(indices));

    mesh->vertexCount = ArraySize(vertices);
    mesh->indexCount  = ArraySize(indices);

    mesh->indexSize = IndexSize_u8;
    mesh->primitive = aiPrimitiveType_TRIANGLE;
}
