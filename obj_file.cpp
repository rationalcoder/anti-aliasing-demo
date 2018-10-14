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
            result.mtllib = next_word(type, line);
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
    v3* uvCatalog     = temp_array(uvCount,         v3);

    u8* vIndexSeenSet = temp_array_zero(vertexCount, u8);
    v3* finalNormals  = allocate_array(vertexCount,  v3);
    v3* finalUvs      = allocate_array(vertexCount,  v3);

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
            f32 value[3] = {};
            sscanf((char*)line.data, "v %f %f %f", &value[0], &value[1], &value[2]);
            vertices[vIdx++];
        }
        else if (type == "vn") {
            f32 value[3] = {};
            sscanf((char*)line.data, "vn %f %f %f", &value[0], &value[1], &value[2]);
            normalCatalog[nIdx++];
        }
        else if (type == "vt") {
            f32 value[3] = {};
            sscanf((char*)line.data, "vt %f %f %f", &value[0], &value[1], &value[2]);
            uvCatalog[uvIdx++];
        }
        else if (type == 'f') {
            u32 vIndices [3] = {};
            u32 vtIndices[3] = {};
            u32 vnIndices[3] = {};

            int n = sscanf((char*)line.data, "f %u/%u/%u %u/%u/%u %u/%u/%u",
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
            groupNode->group.material = next_word(type, line);
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

    return result;
}
