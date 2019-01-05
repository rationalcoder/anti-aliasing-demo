#pragma once

#include "stb_image.h"

#include "primitives.h"
#include "tanks.h"
#include "renderer.h"
#include "obj_file.h"
#include "buffer.h"

// Utility

//: Color

inline f32
srgb_to_linear(f32 val)
{
    return val*val;
}

inline v3
srgb_to_linear(v3 c)
{
    c.r = srgb_to_linear(c.r);
    c.g = srgb_to_linear(c.g);
    c.b = srgb_to_linear(c.b);

    return c;
}

inline v4
srgb_to_linear(v4 c)
{
    return v4(srgb_to_linear(v3(c.r, c.g, c.b)), c.a);
}

inline v3
srgb_to_linear(f32 r, f32 g, f32 b)
{
    return srgb_to_linear(v3(r, g, b));
}

inline v4
srgb_to_linear(f32 r, f32 g, f32 b, f32 a)
{
    return srgb_to_linear(v4(r, g, b, a));
}

//: Geometry

inline void
points_by_step(u32 n, v3 from, v3 step, v3** out)
{
    v3* cur = *out;
    v3* onePastLast = cur + n;
    for (; cur != onePastLast; cur++, from += step)
        *cur = from;

    *out = onePastLast;
}

inline void
points_by_step(u32 n, v3 from, v3 step, v3* out)
{
    points_by_step(n, from, step, &out);
}

inline void
lines_by_step(u32 n, v3 p1, v3 p2, v3 step, v3** out)
{
    v3* iter = *out;
    v3* onePastLast = iter + 2*n;
    for (; iter != onePastLast; iter += 2, p1 += step, p2 += step) {
        iter[0] = p1;
        iter[1] = p2;
    }

    *out = onePastLast;
}

inline void
lines_by_step(u32 n, v3 p1, v3 p2, v3 step, v3* out)
{
    lines_by_step(n, p1, p2, step, &out);
}

inline void
lines_by_step(u32 n, v3 p1, v3 p2, v3 step1, v3 step2, v3** out)
{
    v3* iter = *out;
    v3* onePastLast = iter + 2*n;
    for (; iter != onePastLast; iter += 2, p1 += step1, p2 += step2) {
        iter[0] = p1;
        iter[1] = p2;
    }

    *out = onePastLast;
}

struct Grid
{
    v3* vertices;
    u32 vertexCount;
};

inline Grid
make_xy_grid(int xMax, int yMax)
{
    u32 xSpan = 2*xMax + 1;
    u32 ySpan = 2*yMax + 1;
    u32 vertexCount = 2*(xSpan + ySpan);

    // Stores lines back to back, <p0, p1>, <p1, p2>, etc.
    v3* grid  = allocate_array(vertexCount, v3);
    v3* start = grid; // NOT redundant with the grid pointer. See lines_by_step().

    v3 tl {-xMax,  yMax, 0};
    v3 tr { xMax,  yMax, 0};
    v3 bl {-xMax, -yMax, 0};

    v3 xStep {1,  0, 0};
    v3 yStep {0, -1, 0};

    lines_by_step(xSpan, tl, bl, xStep, &start);
    lines_by_step(ySpan, tl, tr, yStep, &start);

    Grid result = { grid, vertexCount };
    return result;
}

struct AABB
{
    v3 min;
    v3 max;
};

inline AABB
compute_bounding_box(const Static_Mesh& mesh)
{
    AABB result;

    f32 minX = -FLT_MAX;
    f32 minY = -FLT_MAX;
    f32 minZ = -FLT_MAX;

    f32 maxX = 0;
    f32 maxY = 0;
    f32 maxZ = 0;

    for (u32 i = 0; i < mesh.vertexCount; i++) {
        v3 v = ((v3*)mesh.vertices)[i];
        if (v.x > maxX) maxX = v.x;
        else if (v.x < minX) minX = v.x;

        if (v.y > maxY) maxY = v.y;
        else if (v.y < minY) minY = v.y;

        if (v.z > maxZ) maxZ = v.z;
        else if (v.z < minZ) minZ = v.z;
    }

    result.min.x = minX;
    result.min.y = minY;
    result.min.z = minZ;

    result.max.x = maxX;
    result.max.y = maxY;
    result.max.z = maxZ;

    return result;
}

struct Debug_Cube
{
    f32* vertices; // 3D
    u32 vertexCount;
};

inline Static_Mesh
make_cube(f32 halfWidth)
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

    Static_Mesh mesh;

    mesh.vertices = allocate_array_copy(ArraySize(vertices), f32, vertices);
    mesh.indices  = allocate_array_copy(ArraySize(indices),  u8,  indices);

    mesh.vertexCount = ArraySize(vertices) / 3;
    mesh.indexCount  = ArraySize(indices);

    mesh.indexSize = IndexSize_u8;
    mesh.primitive = Primitive_Triangles;

    return mesh;
}

inline Static_Mesh
make_solid_cube(f32 halfWidth, v3 color)
{
    Static_Mesh cube = make_cube(halfWidth);

    auto material = allocate_type(Material);

    auto group = allocate_new(Colored_Index_Group);
    material->coloredIndexGroups = group;
    material->coloredGroupCount  = 1;

    group->color = color;
    group->start = 0;
    group->count = cube.indexCount;

    cube.material = material;

    return cube;
}

inline Static_Mesh
make_box(v3 origin, v3 span, v3 color)
{
    // Scale and translate a 1x1x1 cube to fit.
    Static_Mesh cube = make_solid_cube(.5f, color);
    for (u32 i = 0; i < cube.vertexCount; i++) {
        v3& v = *(((v3*)cube.vertices) + i);
        v *= span;
        v += origin;
    }

    return cube;
}


static inline MTL_Material*
find_material(const MTL_File& mtl, string32 name)
{
    for (u32 i = 0; i < mtl.materialCount; i++) {
        if (mtl.materials[i].name == name)
            return mtl.materials + i;
    }

    return nullptr;
}

static inline Texture
load_texture(string32 path)
{
    arena_scope(gMem->file);

    Texture result;

    buffer32 fileData = read_file_buffer(path);

    int x = 0;
    int y = 0;
    int channels = 0;

    result.data = stbi_load_from_memory(fileData.data, fileData.size, &x, &y, &channels, 0);
    result.format = (Texture_Format)(channels-1);
    result.x = x;
    result.y = y;

    if (!result.data)
        log_debug("STB Image Error: %s\n", stbi_failure_reason());

    return result;
}

inline Static_Mesh
load_static_mesh(const OBJ_File& obj, const MTL_File& mtl,
                 const char* texturePath, const char* tag)
{
    Static_Mesh result;

    result.tag = dup(tag);

    result.vertices = (f32*)obj.vertices;
    result.normals  = (f32*)obj.normals;
    result.tangents = (f32*)obj.tangents;
    result.uvs      = (f32*)obj.uvs;
    result.indices  = obj.indices;

    result.vertexCount = obj.vertexCount;
    result.indexCount  = obj.indexCount;
    result.indexSize   = (Index_Size)obj.indexSize;

    if (!obj.groupCount || !mtl.materialCount)
        return result;

    Material* material = allocate_new(Material);

    material->coloredGroupCount  = obj.groupCount;
    material->coloredIndexGroups = allocate_array(obj.groupCount, Colored_Index_Group);

    for (u32 i = 0; i < obj.groupCount; i++) {
        OBJ_Material_Group& group = obj.groups[i];

        MTL_Material* mtlMat = find_material(mtl, group.material);

        auto& cg = material->coloredIndexGroups[i];
        cg.start = group.startingIndex;

        // There might be a diffuse map, or there might just be a solid color. Handle both cases.
        // NOTE(blake): If there is no diffuse map, we assume there are no other maps.

        if (!mtlMat->diffuseMap) {
            cg.color       = mtlMat->diffuseColor;
            cg.specularExp = mtlMat->specularExponent;
            // NOTE(blake): need to fill out the `count` fields in a separate pass.
            continue;
        }

        cg.diffuseMap  = load_texture(cat(texturePath, mtlMat->diffuseMap));
        cg.specularExp = mtlMat->specularExponent;

        if (mtlMat->normalMap)   cg.normalMap   = load_texture(cat(texturePath, mtlMat->normalMap));
        if (mtlMat->emissiveMap) cg.emissiveMap = load_texture(cat(texturePath, mtlMat->emissiveMap));
        if (mtlMat->specularMap) cg.specularMap = load_texture(cat(texturePath, mtlMat->specularMap));
    }

    // NOTE(blake): we have to calculate the number of indices in each group b/c the only information
    // in each OBJ group is the starting index.

    u32 totalIndexCount = 0;

    for (u32 i = 0; i < material->coloredGroupCount-1; i++) {
        Colored_Index_Group& groupCur  = material->coloredIndexGroups[i];
        Colored_Index_Group& groupNext = material->coloredIndexGroups[i+1];

        groupCur.count = groupNext.start - groupCur.start;
        totalIndexCount += groupCur.count;
    }

    Colored_Index_Group& lastGroup = material->coloredIndexGroups[material->coloredGroupCount-1];
    lastGroup.count = obj.indexCount - lastGroup.start;

    totalIndexCount += lastGroup.count;
    assert(totalIndexCount == obj.indexCount);

    result.material = material;
    return result;
}

struct Debug_Normals
{
    v3* t;
    v3* b;
    v3* n;

    u32 vertexCount;
};

inline Debug_Normals
make_debug_normals(const Static_Mesh& mesh)
{
    Debug_Normals result = {};
    if (!mesh.has_normals()) return result;

    v3* meshVertices = (v3*)mesh.vertices;
    v3* meshNormals  = (v3*)mesh.normals;
    v3* meshTangents = (v3*)mesh.tangents;

    if (mesh.has_indices()) {
        v3* normals    = allocate_array(mesh.indexCount * 2, v3);
        v3* tangents   = nullptr;
        v3* bitangents = nullptr;

        if (mesh.has_tangents()) {
            tangents   = allocate_array(mesh.indexCount * 2, v3);
            bitangents = allocate_array(mesh.indexCount * 2, v3);
        }

        u32 lineBase = 0;
        for (u32 i = 0; i < mesh.indexCount; i++, lineBase += 2) {
            v3 v = meshVertices[i];

            normals[lineBase + 0] = v;
            normals[lineBase + 1] = v + .2f * meshNormals[i];

            if (mesh.has_tangents()) {
                tangents[lineBase + 0] = v;
                tangents[lineBase + 1] = v + .2f * meshTangents[i];

                bitangents[lineBase + 0] = v;
                bitangents[lineBase + 1] = v + .2f * glm::cross(meshTangents[i], meshNormals[i]);
            }
        }

        result.t = tangents;
        result.b = bitangents;
        result.n = normals;

        result.vertexCount = mesh.indexCount * 2;
    }
    else {
        v3* normals    = allocate_array(mesh.vertexCount * 2, v3);
        v3* tangents   = nullptr;
        v3* bitangents = nullptr;

        if (mesh.has_tangents()) {
            tangents   = allocate_array(mesh.vertexCount * 2, v3);
            bitangents = allocate_array(mesh.vertexCount * 2, v3);
        }

        for (u32 i = 0; i < mesh.vertexCount * 2; i += 2) {
            normals[i + 0] = meshVertices[i];
            normals[i + 1] = meshVertices[i] + meshNormals[i];

            if (mesh.has_tangents()) {
                tangents[i + 0] = meshVertices[i];
                tangents[i + 1] = meshVertices[i] + meshTangents[i];

                bitangents[i + 0] = meshVertices[i];
                bitangents[i + 1] = meshVertices[i] + glm::cross(meshNormals[i], meshTangents[i]);
            }
        }

        result.t = tangents;
        result.b = bitangents;
        result.n = normals;

        result.vertexCount = mesh.vertexCount * 2;
    }

    return result;
}


// Render Commands

inline void
set_render_target(Push_Buffer& buffer)
{
    gGame->targetRenderCommandBuffer = &buffer;
}

#define push_render_command(command) ((command*)push_render_command_(RenderCommand_##command, sizeof(command), alignof(command)))

// TODO: overload the arena functions with push buffers b/c buffer.arena gets annoying.
inline void*
push_render_command_(Render_Command_Type type, u32 size, u32 alignment)
{
    Render_Command_Header* header = push_type(gGame->targetRenderCommandBuffer->arena, Render_Command_Header);

    u8* before = (u8*)gGame->targetRenderCommandBuffer->arena.at;
    u8* cmd    = (u8*)push(gGame->targetRenderCommandBuffer->arena, size, alignment);

    header->type = type;
    header->size = down_cast<u32>(size + (cmd - before)); // size includes the alignment offset, if any.

    gGame->targetRenderCommandBuffer->count++;

    *(void**)cmd = nullptr; // _staged = nullptr

    return cmd;
}

// Frame begin commands:

inline void
cmd_set_clear_color(f32 r, f32 g, f32 b, f32 a)
{
    Set_Clear_Color* clearColor = push_render_command(Set_Clear_Color);
    clearColor->r = srgb_to_linear(r);
    clearColor->g = srgb_to_linear(g);
    clearColor->b = srgb_to_linear(b);
    clearColor->a = a;
}

inline void
cmd_set_viewport(Game_Resolution res)
{
    Set_Viewport* viewport = push_render_command(Set_Viewport);
    viewport->x = 0;
    viewport->y = 0;
    viewport->w = res.w;
    viewport->h = res.h;
}

inline void
cmd_set_viewport(u32 x, u32 y, u32 w, u32 h)
{
    Set_Viewport* viewport = push_render_command(Set_Viewport);
    viewport->x = x;
    viewport->y = y;
    viewport->w = w;
    viewport->h = h;
}

inline void
cmd_set_view_matrix(const mat4& matrix)
{
    Set_View_Matrix* viewMatrix = push_render_command(Set_View_Matrix);
    memcpy(viewMatrix->mat4, &matrix, sizeof(matrix));
}

inline void
cmd_set_projection_matrix(const mat4& matrix)
{
    Set_Projection_Matrix* projMatrix = push_render_command(Set_Projection_Matrix);
    memcpy(projMatrix->mat4, &matrix, sizeof(matrix));
}

inline void
cmd_set_aa_technique(AA_Technique t)
{
    Set_AA_Technique* setAA = push_render_command(Set_AA_Technique);
    setAA->technique = t;
}

inline void
cmd_resize_buffers(u32 w, u32 h)
{
    Resize_Buffers* resize = push_render_command(Resize_Buffers);
    resize->w = w;
    resize->h = h;
}

inline void
cmd_resize_buffers(Game_Resolution res)
{
    cmd_resize_buffers(res.w, res.h);
}

// Exec commands:

inline void
cmd_render_debug_lines(v3* vertices, u32 vertexCount, v3 color)
{
    Render_Debug_Lines* debugLines = push_render_command(Render_Debug_Lines);
    debugLines->vertices    = (f32*)vertices;
    debugLines->vertexCount = vertexCount;

    debugLines->r = srgb_to_linear(color.r);
    debugLines->g = srgb_to_linear(color.g);
    debugLines->b = srgb_to_linear(color.b);
}

inline void
cmd_render_debug_cubes(view32<v3> centers, f32 halfWidth, v3 color)
{
    v3* centersCopy = allocate_array_copy(centers.size, v3, centers.data);

    Render_Debug_Cubes* debugCubes = push_render_command(Render_Debug_Cubes);
    debugCubes->centers   = (f32*)centersCopy;
    debugCubes->count     = centers.size;
    debugCubes->halfWidth = halfWidth;

    debugCubes->r = srgb_to_linear(color.r);
    debugCubes->g = srgb_to_linear(color.g);
    debugCubes->b = srgb_to_linear(color.b);
}

inline void
cmd_render_debug_cube(v3 position, f32 halfWidth, v3 color)
{
    cmd_render_debug_cubes(view_of(&position, 1), halfWidth, color);
}

inline void
cmd_render_static_mesh(const Static_Mesh& mesh, const mat4& modelMatrix)
{
    Render_Static_Mesh* staticMesh = push_render_command(Render_Static_Mesh);

    staticMesh->mesh = mesh;
    *(mat4*)&staticMesh->modelMatrix = modelMatrix;
}

inline void
cmd_render_point_light(v3 position, v3 color)
{
    Render_Point_Light* pointLight = push_render_command(Render_Point_Light);
    pointLight->x = position.x;
    pointLight->y = position.y;
    pointLight->z = position.z;
    pointLight->r = srgb_to_linear(color.r);
    pointLight->g = srgb_to_linear(color.g);
    pointLight->b = srgb_to_linear(color.b);
}

inline void
render_commands(const Push_Buffer& buffer)
{
    renderer_exec(&gGame->rendererWorkspace, buffer.arena.start, buffer.count);
}
