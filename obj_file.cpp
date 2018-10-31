#include "obj_file.h"

#include <stdio.h>
#include "tanks.h"
#include "buffer.h"
#include "containers.h"


struct Material_Group
{
    Material_Group* next;
    OBJ_Material_Group group;
};

struct MTL_Material_Node
{
    MTL_Material_Node* next;
    MTL_Material mat;
};

struct OBJ_Face
{
    u32 v0;
    u32 vt0;
    u32 vn0;

    u32 v1;
    u32 vt1;
    u32 vn1;

    u32 v2;
    u32 vt2;
    u32 vn2;
};

// NOTE(blake): @Speed. This "hash map" for mapping 3 indices to 1 is really bad.
// it basically just uses separate chaining with the vertex index as the hash.

struct OBJ_Index_Bucket
{
    OBJ_Index_Bucket* next;

    u32 v;
    u32 vt;
    u32 vn;
    u32 final;
};

struct Index_Map
{
    OBJ_Index_Bucket** buckets;
};

inline Index_Map
make_index_map(u32 vertexCount)
{
    Index_Map result;
    result.buckets = temp_array_zero(vertexCount, OBJ_Index_Bucket*);

    return result;
}

inline u32
hash_index(u32 v, u32 vt, u32 vn)
{
    (void)vt;
    (void)vn;
    return v;
}

inline u32*
find_index(Index_Map& map, u32 v, u32 vt, u32 vn)
{
    OBJ_Index_Bucket*& bucket = map.buckets[hash_index(v, vt, vn)];

    for (OBJ_Index_Bucket* b = bucket; b; b = b->next) {
        if (v == b->v && vt == b->vt && vn == b->vn) {
            log_debug("Reusing something\n");
            return &b->final;
        }
    }

    OBJ_Index_Bucket* newBucket = temp_type(OBJ_Index_Bucket);
    newBucket->v     = v;
    newBucket->vt    = vt;
    newBucket->vn    = vn;
    newBucket->final = ~0u;

    bucket = list_push(bucket, newBucket);

    return &newBucket->final;
}


extern OBJ_File
parse_obj_file(buffer32 buffer)
{
    temp_scope();

    OBJ_File result = {};

    u32 vertexCount = 0;
    u32 uvCount     = 0;
    u32 normalCount = 0;
    u32 faceCount   = 0;

    b32 gotMtllib = false;

    // NOTE(blake): first pass to get array bounds since I don't have a bucket array atm.
    for (buffer32 line = buffer; line; line = next_line(line)) {
        buffer32 type = first_word(line);

        if      (type == 'v')  vertexCount++;
        else if (type == "vt") uvCount++;
        else if (type == "vn") normalCount++;
        else if (type == 'f')  faceCount++;
        else if (!gotMtllib && type == "mtllib") {
            result.mtllib = dup(next_word(type, line));
            gotMtllib = true;
        }
    }

    if (faceCount == 0) {
        result.error = "Missing faces.";
        return result;
    }

    OBJ_Face* faceCatalog   = temp_array(faceCount,   OBJ_Face);
    v3*       vertexCatalog = temp_array(vertexCount, v3);
    v2*       uvCatalog     = temp_array(uvCount,     v2);
    v3*       normalCatalog = nullptr;

    if (normalCount)
        normalCatalog = temp_array(normalCount, v3);

    u32 vIdx  = 0;
    u32 uvIdx = 0;
    u32 nIdx  = 0;
    u32 fIdx  = 0;

    Material_Group* groupList  = nullptr;
    u32             groupCount = 0;

    for (buffer32 line = buffer; line; line = next_line(line)) {
        buffer32 type = first_word(line);

        if (type == 'v') {
            v3 value;
            sscanf(cstr_line(line), "v %f %f %f", &value.x, &value.y, &value.z);
            vertexCatalog[vIdx++] = value;
        }
        else if (type == "vt") {
            v2 value;
            sscanf(cstr_line(line), "vt %f %f", &value.x, &value.y);
            uvCatalog[uvIdx++] = value;
        }
        else if (normalCount && type == "vn") {
            v3 value;
            sscanf(cstr_line(line), "vn %f %f %f", &value.x, &value.y, &value.z);
            normalCatalog[nIdx++] = value;
        }
        else if (type == 'f') {
            u32 vIndices [3] = {};
            u32 vtIndices[3] = {};
            u32 vnIndices[3] = { 1, 1, 1 };

            if (normalCount) {
                int n = sscanf(cstr_line(line), "f %u/%u/%u %u/%u/%u %u/%u/%u",
                               &vIndices[0], &vtIndices[0], &vnIndices[0],
                               &vIndices[1], &vtIndices[1], &vnIndices[1],
                               &vIndices[2], &vtIndices[2], &vnIndices[2]);
                if (n < 9) {
                    result.error = "Less than 3 vertex attributes on face.";
                    return result;
                }
            }
            else {
                int n = sscanf(cstr_line(line), "f %u/%u %u/%u %u/%u",
                               &vIndices[0], &vtIndices[0],
                               &vIndices[1], &vtIndices[1],
                               &vIndices[2], &vtIndices[2]);
                if (n < 6) {
                    result.error = "Less than 2 vertex attributes on face.";
                    return result;
                }
            }

            OBJ_Face& face = faceCatalog[fIdx++];

            face.v0  = vIndices[0] - 1;
            face.v1  = vIndices[1] - 1;
            face.v2  = vIndices[2] - 1;
            face.vt0 = vtIndices[0] - 1;
            face.vt1 = vtIndices[1] - 1;
            face.vt2 = vtIndices[2] - 1;
            face.vn0 = vnIndices[0] - 1;
            face.vn1 = vnIndices[1] - 1;
            face.vn2 = vnIndices[2] - 1;
        }
        else if (type == "usemtl") {
            Material_Group* groupNode = temp_type(Material_Group);
            groupNode->group.material = dup(next_word(type, line));
            groupNode->group.startingIndex = fIdx;

            groupList = list_push(groupList, groupNode);
            groupCount++;
        }
    }

    // NOTE(blake): we need to flatten/reverse the material groups before doing the faces
    // b/c group starting "indexes" are in terms of faces. They need to be mapped to index buffer indices.

    OBJ_Material_Group* groups = allocate_array(groupCount, OBJ_Material_Group);

    Material_Group* group = groupList;
    for (int i = groupCount-1; i >= 0; i--, group = group->next)
        groups[i] = group->group;


    Bucket_List<v3,  128> finalVertices(gMem->temp);
    Bucket_List<v3,  128> finalNormals(gMem->temp);
    Bucket_List<v2,  128> finalUvs(gMem->temp);
    Bucket_List<u32, 128> finalIndices(gMem->temp);

    Index_Map map = make_index_map(vertexCount);

    u32 groupToFixIdx = 0;

    for (u32 i = 0; i < faceCount; i++) {
        OBJ_Face& f = faceCatalog[i];

        u32* idx0 = find_index(map, f.v0, f.vt0, f.vn0);
        u32* idx1 = find_index(map, f.v1, f.vt1, f.vn1);
        u32* idx2 = find_index(map, f.v2, f.vt2, f.vn2);

        if (*idx0 == ~0u) {
            *idx0 = finalVertices.size();
            finalVertices.add(vertexCatalog[f.v0]);
            finalUvs.add(uvCatalog[f.vt0]);

            if (normalCount)
                finalNormals.add(normalCatalog[f.vn0]);
        }

        if (*idx1 == ~0u) {
            *idx1 = finalVertices.size();
            finalVertices.add(vertexCatalog[f.v1]);
            finalUvs.add(uvCatalog[f.vt1]);

            if (normalCount)
                finalNormals.add(normalCatalog[f.vn1]);
        }

        if (*idx2 == ~0u) {
            *idx2 = finalVertices.size();
            finalVertices.add(vertexCatalog[f.v2]);
            finalUvs.add(uvCatalog[f.vt2]);

            if (normalCount)
                finalNormals.add(normalCatalog[f.vn2]);
        }

        if (groupToFixIdx < groupCount && groups[groupToFixIdx].startingIndex == i) {
            groups[groupToFixIdx].startingIndex = *idx0;
            groupToFixIdx++;
        }

        finalIndices.add(*idx0);
        finalIndices.add(*idx1);
        finalIndices.add(*idx2);
    }

    result.vertices = flatten(finalVertices);
    result.normals  = normalCount ? flatten(finalNormals) : nullptr;
    result.uvs      = flatten(finalUvs);
    result.indices  = flatten(finalIndices);

    result.indexSize   = sizeof(u32); // TODO(blake): see if we can use anything smaller.
    result.indexCount  = finalIndices.size();
    result.vertexCount = finalVertices.size();

    result.groups     = groups;
    result.groupCount = groupCount;

#if 0
    for (u32 i = 0; i < vertexCount; i++)
        log_debug("Vertex: %f %f %f\n", vertices[i].x, vertices[i].y, vertices[i].z);
#endif

    return result;
}

extern MTL_File
parse_mtl_file(buffer32 buffer)
{
    temp_scope();

    MTL_File result = {};

    MTL_Material_Node* materialList = nullptr;
    MTL_Material_Node* matNode      = nullptr;

    u32 materialCount = 0;
    b32 inMaterial    = false;

    for (buffer32 line = buffer; line; line = next_line(line)) {
        buffer32 type = first_word(line);

        if (!inMaterial) {
            if (type != "newmtl") continue;

            matNode = temp_new(MTL_Material_Node);
            matNode->mat.name = dup(next_word(type, line));
            inMaterial = true;
            continue;
        }
        else if (type == "newmtl") {
            materialList = list_push(materialList, matNode);

            matNode = temp_new(MTL_Material_Node);
            matNode->mat.name = dup(next_word(type, line));
            materialCount++;
            continue;
        }

        MTL_Material* mat = &matNode->mat;

        if (type == "Ns") {
            sscanf(cstr_line(line), "Ns %f", &mat->specularExponent);
        }
        else if (type == "Ka") {
            sscanf(cstr_line(line), "Ka %f%f%f",
                   &mat->ambientColor.r, &mat->ambientColor.g, &mat->ambientColor.b);
        }
        else if (type == "Kd") {
            sscanf(cstr_line(line), "Kd %f%f%f",
                   &mat->diffuseColor.r, &mat->diffuseColor.g, &mat->diffuseColor.b);
        }
        else if (type == "Ks") {
            sscanf(cstr_line(line), "Ks %f%f%f",
                   &mat->specularColor.r, &mat->specularColor.g, &mat->specularColor.b);
        }
        else if (type == "Ke") {
            sscanf(cstr_line(line), "Ke %f%f%f",
                   &mat->emissiveColor.r, &mat->emissiveColor.g, &mat->emissiveColor.b);
        }
        else if (type == 'd') {
            sscanf(cstr_line(line), "d %f", &mat->opacity);
        }
        else if (type == "illum") {
            mat->illum = *next_word(type, line).data;
        }
        else if (type == "map_Ka") {
            mat->ambientMap = dup(next_word(type, line));
        }
        else if (type == "map_Kd") {
            mat->diffuseMap = dup(next_word(type, line));
        }
        else if (type == "map_Ks") {
            mat->specularMap = dup(next_word(type, line));
        }
        else if (type == "map_Ke") {
            mat->emissiveMap = dup(next_word(type, line));
        }
        else if (type == "map_Bump") {
            mat->normalMap = dup(next_word(type, line));
        }
    }

    if (inMaterial) {
        materialList = list_push(materialList, matNode);
        materialCount++;
    }

    if (materialCount == 0) {
        result.error = "No materials.";
        return result;
    }

    MTL_Material* materials = allocate_array(materialCount, MTL_Material);

    MTL_Material_Node* node = materialList;
    for (int i = materialCount-1; i >= 0; i--, node = node->next)
        materials[i] = node->mat;

    result.materials     = materials;
    result.materialCount = materialCount;

    return result;
}
