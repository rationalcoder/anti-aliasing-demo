#pragma once

#include "primitives.h"
#include "tanks.h"
#include "renderer.h"

// Utility

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

struct Debug_Cube
{
    f32* vertices; // 3D
    u32 vertexCount;
};

inline Static_Mesh
push_debug_cube(f32 halfWidth)
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

    mesh.vertexCount = ArraySize(vertices);
    mesh.indexCount  = ArraySize(indices);

    mesh.indexSize = IndexSize_u8;
    mesh.primitive = Primitive_Triangles;

    return mesh;
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

inline void
cmd_set_clear_color(f32 r, f32 g, f32 b, f32 a)
{
    Set_Clear_Color* clearColor = push_render_command(Set_Clear_Color);
    clearColor->r = r;
    clearColor->g = g;
    clearColor->b = b;
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
cmd_begin_render_pass()
{
    push_render_command(Begin_Render_Pass);
}

inline void
cmd_render_debug_lines(v3* vertices, u32 vertexCount, v3 color)
{
    Render_Debug_Lines* debugLines = push_render_command(Render_Debug_Lines);
    debugLines->vertices    = (f32*)vertices;
    debugLines->vertexCount = vertexCount;

    debugLines->r = color.r;
    debugLines->g = color.g;
    debugLines->b = color.b;
}

inline void
cmd_render_debug_cubes(view32<v3> centers, f32 halfWidth, v3 color)
{
    v3* centersCopy = allocate_array_copy(centers.size, v3, centers.data);

    Render_Debug_Cubes* debugCubes = push_render_command(Render_Debug_Cubes);
    debugCubes->centers   = (f32*)centersCopy;
    debugCubes->count     = centers.size;
    debugCubes->halfWidth = halfWidth;

    debugCubes->r = color.r;
    debugCubes->g = color.g;
    debugCubes->b = color.b;
}

inline void
cmd_render_debug_cube(v3 position, f32 halfWidth, v3 color)
{
    cmd_render_debug_cubes(view_of(&position, 1), halfWidth, color);
}

inline void
render_commands(const Push_Buffer& buffer)
{
    renderer_exec(&gGame->rendererWorkspace, buffer.arena.start, buffer.count);
}
