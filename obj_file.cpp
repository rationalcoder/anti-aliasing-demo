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
    u32 vtg0;

    u32 v1;
    u32 vt1;
    u32 vn1;
    u32 vtg1;

    u32 v2;
    u32 vt2;
    u32 vn2;
    u32 vtg2;
};

// NOTE(blake): @Speed. This "hash map" for mapping 3 indices to 1 is really bad.
// it basically just uses separate chaining with the vertex index as the hash.

struct OBJ_Index_Bucket
{
    OBJ_Index_Bucket* next;

    v3 vx;
    v2 vt;
    v3 vn;
    v3 vtg;
    u32 index;
};

struct Index_Map
{
    OBJ_Index_Bucket** buckets;
    u32 bucketCount;
};

static inline Index_Map
make_index_map(u32 vertexCount)
{
    Index_Map result;
    result.buckets     = temp_array_zero(vertexCount, OBJ_Index_Bucket*);
    result.bucketCount = vertexCount;

    return result;
}

static inline void
hash_combine(umm& seed, umm hash)
{
    hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= hash;
}

template <umm Size_> static inline umm
hash_obj_vertex(char (&buffer)[Size_])
{
    umm seed = 0;
    for (umm i = 0; i < Size_/sizeof(umm); i++)
        hash_combine(seed, ((umm*)buffer)[i]);

    return seed;
}

inline OBJ_Index_Bucket*
find_index(Index_Map& map, v3 vx, v2 vt, v3 vn, v3 vtg)
{
    char hashInput[sizeof(vx) + sizeof(vt) + sizeof(vn) + sizeof(vtg)];
    *((v3*)(hashInput + 0))                                    = vx;
    *((v2*)(hashInput + sizeof(vx)))                           = vt;
    *((v3*)(hashInput + sizeof(vx) + sizeof(vt)))              = vn;
    *((v3*)(hashInput + sizeof(vx) + sizeof(vt) + sizeof(vn))) = vtg;

    OBJ_Index_Bucket*& bucket = map.buckets[hash_obj_vertex(hashInput) % map.bucketCount];

    for (OBJ_Index_Bucket* b = bucket; b; b = b->next) {
        if (vx == b->vx && vt == b->vt && vn == b->vn && vtg == b->vtg)
            return b;
    }

    OBJ_Index_Bucket* newBucket = temp_type(OBJ_Index_Bucket);
    newBucket->vx    = vx;
    newBucket->vt    = vt;
    newBucket->vn    = vn;
    newBucket->vtg   = vn;
    newBucket->index = ~0u;

    bucket = list_push(bucket, newBucket);

    return newBucket;
}


extern OBJ_File
parse_obj_file(buffer32 buffer, u32 processFlags)
{
    temp_scope();

    OBJ_File result = {};

    u32 vertexCount  = 0;
    u32 uvCount      = 0;
    u32 normalCount  = 0;
    u32 tangentCount = 0; // NOTE(blake): not present in obj files; just here for organization.
    u32 faceCount    = 0;

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

    // Potentially generated attributes.
    v3* normalCatalog  = nullptr;
    v3* tangentCatalog = nullptr;

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

            if (processFlags & PostProcess_FlipUVs) value.y = -value.y;
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


    // Generate normals.
    if (!normalCount && processFlags & PostProcess_GenNormals) {
        temp_scope();

        Bucket_List<v3, 128> tempNormalCatalog(gMem->temp);

        for (u32 i = 0; i < faceCount; i++) {
            OBJ_Face& f = faceCatalog[i];

            v3 ab = vertexCatalog[f.v1] - vertexCatalog[f.v0];
            v3 ac = vertexCatalog[f.v2] - vertexCatalog[f.v0];
            v3 n  = glm::normalize(glm::cross(ab, ac));

            u32 catalogIndex = tempNormalCatalog.size();
            tempNormalCatalog.add(n);

            f.vn0 = catalogIndex;
            f.vn1 = catalogIndex;
            f.vn2 = catalogIndex;
        }

        normalCatalog = flatten(tempNormalCatalog);
        normalCount   = vertexCount;
    }

#if 1
    // Generate tangents.
    if (normalCount && processFlags & PostProcess_GenTangents) {
        temp_scope();

#if 1
        Bucket_List<v3, 128> tempTangentCatalog(gMem->temp);

        for (u32 i = 0; i < faceCount; i++) {
            OBJ_Face& f = faceCatalog[i];

            v3 ab = vertexCatalog[f.v1] - vertexCatalog[f.v0];
            v3 ac = vertexCatalog[f.v2] - vertexCatalog[f.v0];

            v2 duv0 = uvCatalog[f.vt1] - uvCatalog[f.vt0];
            v2 duv1 = uvCatalog[f.vt2] - uvCatalog[f.vt0];

            f32 r = 1.0f / (duv0.x * duv1.y - duv0.y * duv1.x);
            v3 tangent = glm::normalize(r * (ab * duv1.y - ac * duv0.y));
            //v3 tangent = glm::normalize(r * (ac * duv0.x - ab * duv1.x));

            u32 idx = tempTangentCatalog.size();

            f.vtg0 = idx + 0;
            f.vtg1 = idx + 1;
            f.vtg2 = idx + 2;

            tempTangentCatalog.add(tangent);
            tempTangentCatalog.add(tangent);
            tempTangentCatalog.add(tangent);
        }

#else // Averaging (broken)
        // Since we only care about direction here, averaging is as simple as adding.
        // Bigger tangent vectors (for bigger triangles) will be weighted more, but that's even better.
        v3* tAverages = temp_array_zero(vertexCount, v3);
        v3* bAverages = temp_array_zero(vertexCount, v3);

        // Calculate tangents and fill out the averages array (which is 1-1 with the vertex catalog).
        for (u32 i = 0; i < faceCount; i++) {
            OBJ_Face& f = faceCatalog[i];

            v3 ab = vertexCatalog[f.v1] - vertexCatalog[f.v0];
            v3 ac = vertexCatalog[f.v2] - vertexCatalog[f.v0];

            v2 duv0 = uvCatalog[f.vt1] - uvCatalog[f.vt0];
            v2 duv1 = uvCatalog[f.vt2] - uvCatalog[f.vt0];

            f32 r = 1.0f / (duv0.x * duv1.y - duv0.y * duv1.x);

            v3 tangent   = r * (ab * duv1.y - ac * duv0.y);
            v3 bitangent = r * (ac * duv0.x - ab * duv1.x);

            tAverages[f.v0] += tangent;
            tAverages[f.v1] += tangent;
            tAverages[f.v2] += tangent;

            bAverages[f.v0] += bitangent;
            bAverages[f.v1] += bitangent;
            bAverages[f.v2] += bitangent;
        }

        Bucket_List<v3, 128> tempTangentCatalog(gMem->temp);

        // Gram-Schmidt re-orthogonalize, make sure the resulting TBN bases will form a
        // right-handed coordinate system, and add the tangents to their catalog.
        for (u32 i = 0; i < faceCount; i++) {
            OBJ_Face& f = faceCatalog[i];

            v3& t0 = tAverages[f.v0];
            v3& t1 = tAverages[f.v1];
            v3& t2 = tAverages[f.v2];

            v3& b0 = bAverages[f.v0];
            v3& b1 = bAverages[f.v1];
            v3& b2 = bAverages[f.v2];

            v3 n = normalCatalog[f.v0]; // normals are constant across the face.

            t0 = glm::normalize(t0 - n * glm::dot(n, t0));
            t1 = glm::normalize(t1 - n * glm::dot(n, t1));
            t2 = glm::normalize(t2 - n * glm::dot(n, t2));

            if (glm::dot(glm::cross(n, t0), b0) < 0.0f) t0 = -t0;
            if (glm::dot(glm::cross(n, t1), b1) < 0.0f) t1 = -t1;
            if (glm::dot(glm::cross(n, t2), b2) < 0.0f) t2 = -t2;

            u32 idx = tempTangentCatalog.size();

            f.vtg0 = idx + 0;
            f.vtg1 = idx + 1;
            f.vtg2 = idx + 2;

            tempTangentCatalog.add(t0);
            tempTangentCatalog.add(t1);
            tempTangentCatalog.add(t2);
        }
#endif

        tangentCatalog = flatten(tempTangentCatalog);
        tangentCount   = vertexCount;
    }
#endif


    Bucket_List<v3,  128> finalVertices(gMem->temp);
    Bucket_List<v3,  128> finalNormals(gMem->temp);
    Bucket_List<v3,  128> finalTangents(gMem->temp);
    Bucket_List<v2,  128> finalUvs(gMem->temp);
    Bucket_List<u32, 128> finalIndices(gMem->temp);

    Index_Map map = make_index_map(vertexCount);

    u32 groupToFixIdx = 0;

    for (u32 i = 0; i < faceCount; i++) {
        OBJ_Face& f = faceCatalog[i];

        v3 vx0  = vertexCatalog[f.v0];
        v2 vt0  = uvCatalog[f.vt0];
        v3 vn0  = normalCount ? normalCatalog[f.vn0] : v3();
        v3 vtg0 = tangentCount ? tangentCatalog[f.vtg0] : v3();

        v3 vx1  = vertexCatalog[f.v1];
        v2 vt1  = uvCatalog[f.vt1];
        v3 vn1  = normalCount ? normalCatalog[f.vn1] : v3();
        v3 vtg1 = tangentCount ? tangentCatalog[f.vtg1] : v3();

        v3 vx2  = vertexCatalog[f.v2];
        v2 vt2  = uvCatalog[f.vt2];
        v3 vn2  = normalCount ? normalCatalog[f.vn2] : v3();
        v3 vtg2 = tangentCount ? tangentCatalog[f.vtg2] : v3();

        OBJ_Index_Bucket* idx0 = find_index(map, vx0, vt0, vn0, vtg0);
        OBJ_Index_Bucket* idx1 = find_index(map, vx1, vt1, vn1, vtg1);
        OBJ_Index_Bucket* idx2 = find_index(map, vx2, vt2, vn2, vtg2);

        // A value of ~0u means we haven't seen this vertex before,
        // so we need to create a new one, fillout the bucket, and use the index of this new vertex.
        if (idx0->index == ~0u) {
            idx0->index = finalVertices.size();

            finalVertices.add(vx0);
            finalUvs.add(vt0);

            if (normalCount)  finalNormals.add(vn0);
            if (tangentCount) finalTangents.add(vtg0);
        }

        if (idx1->index == ~0u) {
            idx1->index = finalVertices.size();

            finalVertices.add(vx1);
            finalUvs.add(vt1);

            if (normalCount)  finalNormals.add(vn1);
            if (tangentCount) finalTangents.add(vtg1);
        }

        if (idx2->index == ~0u) {
            idx2->index = finalVertices.size();

            finalVertices.add(vx2);
            finalUvs.add(vt2);

            if (normalCount)  finalNormals.add(vn2);
            if (tangentCount) finalTangents.add(vtg2);
        }

        finalIndices.add(idx0->index);
        finalIndices.add(idx1->index);
        finalIndices.add(idx2->index);

        // Translate the starting face index to the starting final vertex index while we're here.
        if (groupToFixIdx < groupCount && groups[groupToFixIdx].startingIndex == i) {
            groups[groupToFixIdx].startingIndex = idx0->index;
            groupToFixIdx++;
        }
    }

    result.vertices = flatten(finalVertices);
    result.uvs      = flatten(finalUvs);
    result.indices  = flatten(finalIndices);
    result.normals  = normalCount  ? flatten(finalNormals)  : nullptr;
    result.tangents = tangentCount ? flatten(finalTangents) : nullptr;

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

