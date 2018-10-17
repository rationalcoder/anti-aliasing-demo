#include "obj_file.h"

#include <stdio.h>
#include "tanks.h"
#include "buffer.h"

extern OBJ_File
parse_obj_file(buffer32 buffer)
{
    temp_scope();

    OBJ_File result = {};

    u32 vertexCount = 0;
    u32 uvCount     = 0;
    u32 normalCount = 0;
    u32 indexCount  = 0;

    b32 gotMtllib = false;

    // NOTE(blake): first pass to get array bounds since I don't have a bucket array atm.
    for (buffer32 line = buffer; line; line = next_line(line)) {
        buffer32 type = first_word(line);

        if      (type == 'v')  vertexCount++;
        else if (type == "vt") uvCount++;
        else if (type == "vn") normalCount++;
        else if (type == 'f')  indexCount += 3;
        else if (!gotMtllib && type == "mtllib") {
            result.mtllib = dup(next_word(type, line));
            gotMtllib = true;
        }
    }

    if (indexCount == 0) {
        result.error = "Missing faces.";
        return result;
    }

    // NOTE(blake): we are keeping the vertices and indices around; the others are temporary buffers.
    v3* vertices      = allocate_array(vertexCount, v3);
    v3* normalCatalog = temp_array(normalCount,     v3);
    v2* uvCatalog     = temp_array(uvCount,         v2);

    u8* vIndexSeenSet = temp_array_zero(vertexCount, u8);
    v3* finalNormals  = allocate_array(vertexCount,  v3);
    v2* finalUvs      = allocate_array(vertexCount,  v2);

    // TODO(bmartin): figure out if the indices can fit in something smaller. This assumes u32.
    u32* indices   = allocate_array(indexCount, u32);
    u32  indexSize = sizeof(u32);

    u32 vIdx  = 0;
    u32 uvIdx = 0;
    u32 nIdx  = 0;
    u32 iIdx  = 0;

    struct Textured_Group
    {
        Textured_Group* next;
        OBJ_Textured_Group group;
    };

    Textured_Group* groupList  = nullptr;
    u32             groupCount = 0;

    for (buffer32 line = buffer; line; line = next_line(line)) {
        buffer32 type = first_word(line);

        if (type == 'v') {
            v3 value;
            sscanf(cstr_line(line), "v %f %f %f", &value.x, &value.y, &value.z);
            vertices[vIdx++] = value;
        }
        else if (type == "vt") {
            v2 value;
            sscanf(cstr_line(line), "vt %f %f", &value.x, &value.y);
            uvCatalog[uvIdx++] = value;
        }
        else if (type == "vn") {
            v3 value;
            sscanf(cstr_line(line), "vn %f %f %f", &value.x, &value.y, &value.z);
            normalCatalog[nIdx++] = value;
        }
        else if (type == 'f') {
            u32 vIndices [3] = {};
            u32 vtIndices[3] = {};
            u32 vnIndices[3] = {};

            int n = sscanf(cstr_line(line), "f %u/%u/%u %u/%u/%u %u/%u/%u",
                           &vIndices[0], &vtIndices[0], &vnIndices[0],
                           &vIndices[1], &vtIndices[1], &vnIndices[1],
                           &vIndices[2], &vtIndices[2], &vnIndices[2]);
            if (n != 9) {
                result.error = "Less than 3 vertex attributes";
                return result;
            }

            // NOTE(blake): we have to handle the case where all vertex attributes are
            // completely out of order and potentially not 1-1 with the number of vertices.
            // To do this, we make the other attributes fit the vertices. When we see the index
            // of a vertex that we haven't dealt with, we copy the corresponding normal and uv
            // to normals[index] and uvs[index] and add the index to the index buffer.
            // Otherwise, we just add the index to the index buffer and move on.
            //

            for (int i = 0; i < 3; i++) {
                u32 vIndex  = vIndices [i] - 1;
                u32 vtIndex = vtIndices[i] - 1;
                u32 vnIndex = vnIndices[i] - 1;

                if (!vIndexSeenSet[vIndex]) {
                    finalUvs    [vIndex] = uvCatalog[vtIndex];
                    finalNormals[vIndex] = normalCatalog[vnIndex];

                    vIndexSeenSet[vIndex] = true;
                }

                indices[iIdx++] = vIndex;
            }
        }
        else if (type == "usemtl") {
            Textured_Group* groupNode = temp_type(Textured_Group);
            groupNode->group.material = dup(next_word(type, line));
            groupNode->group.startingIndex = iIdx;

            groupList = list_push(groupList, groupNode);
            groupCount++;
        }
    }

    result.vertices = vertices;
    result.normals  = finalNormals;
    result.uvs      = finalUvs;
    result.indices  = indices;

    result.indexCount  = indexCount;
    result.indexSize   = indexSize;
    result.vertexCount = vertexCount;

    OBJ_Textured_Group* groups = allocate_array(groupCount, OBJ_Textured_Group);

    Textured_Group* group = groupList;
    for (int i = groupCount-1; i >= 0; i--, group = group->next)
        groups[i] = group->group;

    result.groups      = groups;
    result.groupCount  = groupCount;

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

    struct MTL_Material_Node
    {
        MTL_Material_Node* next;
        MTL_Material mat;
    };

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
