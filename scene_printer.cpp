#include "scene_printer.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <cstring>

void ifprintf(FILE* file, uint32_t indent, const char* fmt, ...)
{
    char* fullFormat = (char*)alloca(strlen(fmt) + indent + 1);
    memset(fullFormat, ' ', indent);
    strcpy(fullFormat + indent, fmt);

    va_list vlist;
    va_start(vlist, fmt);

    vfprintf(file, fullFormat, vlist);

    va_end(vlist);
}

void iprintf(uint32_t indent, const char* fmt, ...)
{
    char* fullFormat = (char*)alloca(strlen(fmt) + indent + 1);
    memset(fullFormat, ' ', indent);
    strcpy(fullFormat + indent, fmt);

    va_list vlist;
    va_start(vlist, fmt);

    vprintf(fullFormat, vlist);

    va_end(vlist);
}

void print_section_heading(const char* name)
{
    printf("---------------%s---------------\n", name);
}

void print_mesh_info(const aiScene* scene)
{
    print_section_heading("Meshes");
    printf("Count: %u\n\n", scene->mNumMeshes);
    printf("Mesh Summaries:\n");
    for (int i = 0; i < (int)scene->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[i];
        iprintf(4, "Mesh %d (Name: \"%s\"):\n", i, mesh->mName.data);
        iprintf(4, "Contents:\n");
        if (mesh->HasPositions()) iprintf(8, "-Positions (%u)\n", mesh->mNumVertices);
        if (mesh->HasNormals()) iprintf(8, "-Normals\n");
        if (mesh->HasTangentsAndBitangents()) iprintf(8, "-Tangents/Bitangents");
        if (mesh->GetNumColorChannels()) iprintf(8, "-Colors (Channels: %u)\n", mesh->GetNumColorChannels());
        if (mesh->GetNumUVChannels()) iprintf(8, "-UVs (Channels: %u)\n", mesh->GetNumUVChannels());
        if (mesh->HasFaces()) iprintf(8, "-Triangles (%u)\n", mesh->mNumFaces);
        iprintf(8, "-Material Index: %u\n", mesh->mMaterialIndex);

        if (mesh->HasBones()) iprintf(8, "-Bones (%u):\n", mesh->mNumBones);
        for (int i = 0; i < (int)mesh->mNumBones; i++) {
            const aiBone* bone = mesh->mBones[i];
            iprintf(12, "Bone %d:\n", i);
            iprintf(16, "-Name: %s\n", bone->mName.data);
            iprintf(16, "-Weights: %u\n", bone->mNumWeights);
        }
    }

    printf("\n");
}

template <typename T_>
const char* ai_to_string(T_);

template <>
const char* ai_to_string<aiTextureType>(aiTextureType type)
{
    switch (type) {
    case aiTextureType_AMBIENT: return "Ambient";
    case aiTextureType_DIFFUSE: return "Diffuse";
    case aiTextureType_DISPLACEMENT: return "Displacement";
    case aiTextureType_EMISSIVE: return "Emissive";
    case aiTextureType_HEIGHT: return "Height";
    case aiTextureType_LIGHTMAP: return "Light";
    case aiTextureType_NORMALS: return "Normal";
    case aiTextureType_OPACITY: return "Opacity";
    case aiTextureType_REFLECTION: return "Reflection";
    case aiTextureType_SHININESS: return "Shininess";
    case aiTextureType_SPECULAR: return "Specular";
    case aiTextureType_UNKNOWN: return "Unknown";
    default: return "Invalid";
    }
}

template <>
const char* ai_to_string<aiTextureMapping>(aiTextureMapping mapping)
{
    switch (mapping) {
    case aiTextureMapping_BOX: return "Box";
    case aiTextureMapping_CYLINDER: return "Cylinder";
    case aiTextureMapping_PLANE: return "Plane";
    case aiTextureMapping_SPHERE: return "Sphere";
    case aiTextureMapping_UV: return "UV";
    case aiTextureMapping_OTHER: return "Other";
    default: return "Invalid";
    }
}

template <>
const char* ai_to_string<aiTextureMapMode>(aiTextureMapMode mode)
{
    switch (mode) {
    case aiTextureMapMode_Clamp: return "Clamp";
    case aiTextureMapMode_Decal: return "Decal";
    case aiTextureMapMode_Mirror: return "Mirror";
    case aiTextureMapMode_Wrap: return "Wrap";
    default: return "Invalid";
    }
}

template <>
const char* ai_to_string<aiTextureOp>(aiTextureOp op)
{
    switch (op) {
    case aiTextureOp_Add: return "Add";
    case aiTextureOp_Divide: return "Divide";
    case aiTextureOp_Multiply: return "Multiply";
    case aiTextureOp_SignedAdd: return "Signed Add";
    case aiTextureOp_SmoothAdd: return "Smooth Add";
    case aiTextureOp_Subtract: return "Subtract";
    default: return "Invalid";
    }
}

void print_material_info(const aiScene* scene)
{
    print_section_heading("Materials");
    printf("Count: %u\n", scene->mNumMaterials);

    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        const aiMaterial* material = scene->mMaterials[i];
        aiString name;
        material->Get(AI_MATKEY_NAME, name);

        iprintf(4, "Material %d (Name: \"%s\", Num Properies: %u):\n", i, name.data, material->mNumProperties);
        iprintf(4, "Contents:\n");

        for (uint32_t type = 1; type <= aiTextureType_UNKNOWN; type++) {
            uint32_t count = material->GetTextureCount((aiTextureType)type);
            aiString fileName;
            if (count) iprintf(4, "-Texture(Type: %s, Count: %u):\n", ai_to_string<>((aiTextureType)type), count);

            for (uint32_t i = 0; i < count; i++) {
                aiTextureMapping mapping = aiTextureMapping_UV;
                aiTextureMapMode mode = aiTextureMapMode_Wrap;
                aiTextureOp op = aiTextureOp_Add;
                uint32_t uvIndex = 0;
                float blendFactor = 1.0;
                material->GetTexture((aiTextureType)type, i, &fileName, &mapping, &uvIndex, &blendFactor, &op, &mode);
                iprintf(8, "Texture %d (File: \"%s\")\n", i, fileName.data);
                //iprintf(12, "-Mapping: %s\n", ai_to_string(mapping));
                //iprintf(12, "-Mode: %s\n", ai_to_string(mode));
                //iprintf(12, "-Op: %s\n", ai_to_string(op));
                //iprintf(12, "-UV Index: %u\n", uvIndex);
                //iprintf(12, "-Blend Factor: %f\n", blendFactor);
                printf("\n");
            }
        }

        aiColor4D color;
        if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
            iprintf(8, "-Color(Type: Ambient, Value(RGBA): %f, %f, %f, %f)\n", color.r, color.g, color.b, color.a);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            iprintf(8, "-Color(Type: Diffuse, Value(RGBA): %f, %f, %f, %f)\n", color.r, color.g, color.b, color.a);
        if (material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
            iprintf(8, "-Color(Type: Emissive, Value(RGBA): %f, %f, %f, %f)\n", color.r, color.g, color.b, color.a);
        if (material->Get(AI_MATKEY_COLOR_REFLECTIVE, color) == AI_SUCCESS)
            iprintf(8, "-Color(Type: Reflective, Value(RGBA): %f, %f, %f, %f)\n", color.r, color.g, color.b, color.a);
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
            iprintf(8, "-Color(Type: Specular, Value(RGBA): %f, %f, %f, %f)\n", color.r, color.g, color.b, color.a);
        if (material->Get(AI_MATKEY_COLOR_TRANSPARENT, color) == AI_SUCCESS)
            iprintf(8, "-Color(Type: Transparent, Value(RGBA): %f, %f, %f, %f)\n", color.r, color.g, color.b, color.a);
        printf("\n");
    }

    printf("\n");
}

void print_light_info(const aiScene* scene)
{
    print_section_heading("Lights");
    printf("Count: %u\n", scene->mNumLights);
    printf("\n");
}

void print_camera_info(const aiScene* scene)
{
    print_section_heading("Camera");
    printf("Count: %u\n", scene->mNumCameras);
    printf("\n");
}

void print_animation_info(const aiScene* scene)
{
    print_section_heading("Animations");
    printf("Count: %u\n", scene->mNumAnimations);

    for (int i = 0; i < (int)scene->mNumAnimations; i++) {
        const aiAnimation* anim = scene->mAnimations[i];
        printf("Animation %d (Name: \"%s\"):\n", i, anim->mName.C_Str());
        printf("Contents:\n");
        iprintf(4, "-Duration: %fs (%f Frames)\n", anim->mDuration/anim->mTicksPerSecond, anim->mDuration);
        iprintf(4, "-Key FPS: %f\n", anim->mTicksPerSecond);
        iprintf(4, "-Skeletal Animation Channels: %u\n", anim->mNumChannels);
        for (int i = 0; i < (int)anim->mNumChannels; i++) {
            const aiNodeAnim* nodeAnim = anim->mChannels[i];
            iprintf(8, "Channel %d (For Node: \"%s\"):\n", i, nodeAnim->mNodeName.data);
            iprintf(12, "-Position Keys: %u\n", nodeAnim->mNumPositionKeys);
            iprintf(12, "-Rotation Keys: %u\n", nodeAnim->mNumRotationKeys);
            iprintf(12, "-Scaling Keys: %u\n", nodeAnim->mNumScalingKeys);
            iprintf(12, "-Pre State: %s\n", anim_behavior_to_string(nodeAnim->mPreState));
            iprintf(12, "-Post State: %s\n", anim_behavior_to_string(nodeAnim->mPostState));
            printf("\n");
        }

        iprintf(4, "-Vertex Animation Channels: %u\n", anim->mNumMeshChannels);
        printf("\n");
    }
}

void print_node_info(const aiNode* node, uint32_t indent)
{
    if (node->mNumMeshes) {
        iprintf(indent, "Node (Name: \"%s\", Includes Meshes(%u): ", node->mName.data, node->mNumMeshes);
        printf("%u", node->mMeshes[0]);
        for (int i = 1; i < (int)node->mNumMeshes; i++) {
            printf(", %u", node->mMeshes[i]);
        }
        printf(")\n");
    }
    else {
        iprintf(indent, "Node (Name: \"%s\")\n", node->mName.data);
    }

    for (int i = 0; i < (int)node->mNumChildren; i++)
        print_node_info(node->mChildren[i], indent + 4);
}

void print_node_summary(const aiScene* scene)
{
    print_section_heading("Node Summary");
    print_node_info(scene->mRootNode);
}

void print_scene_summary(const char* fileName, const aiScene* scene)
{
    std::string heading("Scene Summary for ");
    heading += fileName;
    print_section_heading(heading.c_str());
    printf("\n");

    printf("Contents:\n");
    if (scene->HasMeshes()) printf("-Meshes\n");
    if (scene->HasMaterials()) printf("-Materials\n");
    if (scene->HasAnimations()) printf("-Animations\n");
    if (scene->HasCameras()) printf("-Cameras\n");
    if (scene->HasLights()) printf("-Lights\n");
    printf("\n");

    if (scene->HasMeshes()) print_mesh_info(scene);
    if (scene->HasMaterials()) print_material_info(scene);
    if (scene->HasAnimations()) print_animation_info(scene);
    if (scene->HasCameras()) print_camera_info(scene);
    if (scene->HasLights()) print_light_info(scene);

    print_node_summary(scene);
}
