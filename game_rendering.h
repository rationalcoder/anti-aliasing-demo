#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "tanks.h"
#include "renderer.h"

// Utility

inline void
points_by_step(u32 n, glm::vec3 from, glm::vec3 step, glm::vec3** out)
{
    glm::vec3* cur = *out;
    glm::vec3* onePastLast = cur + n;
    for (; cur != onePastLast; cur++, from += step)
        *cur = from;

    *out = onePastLast;
}

inline void
lines_by_step(u32 n, glm::vec3 p1, glm::vec3 p2,
              glm::vec3 step, glm::vec3** out)
{
    glm::vec3* iter = *out;
    glm::vec3* onePastLast = iter + 2*n;
    for (; iter != onePastLast; iter += 2, p1 += step, p2 += step) {
        iter[0] = p1;
        iter[1] = p2;
    }

    *out = onePastLast;
}

inline void
lines_by_step(u32 n, glm::vec3 p1, glm::vec3 p2,
              glm::vec3 step1, glm::vec3 step2,
              glm::vec3** out)
{
    glm::vec3* iter = *out;
    glm::vec3* onePastLast = iter + 2*n;
    for (; iter != onePastLast; iter += 2, p1 += step1, p2 += step2) {
        iter[0] = p1;
        iter[1] = p2;
    }

    *out = onePastLast;
}

struct Grid
{
    glm::vec3* vertices;
    u32 vertexCount;
};

inline Grid
make_xy_grid(int xMax, int yMax, Memory_Arena* arena)
{
    u32 xSpan = 2*xMax + 1;
    u32 ySpan = 2*yMax + 1;
    u32 vertexCount = 2*(xSpan + ySpan);

    // Stores lines back to back, <p0, p1>, <p1, p2>, etc.
    glm::vec3* grid  = arena_push_array(*arena, vertexCount, glm::vec3);
    glm::vec3* start = grid; // NOT redundant with the grid pointer. See lines_by_step().

    glm::vec3 tl {-xMax,  yMax, 0};
    glm::vec3 tr { xMax,  yMax, 0};
    glm::vec3 bl {-xMax, -yMax, 0};

    glm::vec3 xStep {1,  0, 0};
    glm::vec3 yStep {0, -1, 0};

    lines_by_step(xSpan, tl, bl, xStep, &start);
    lines_by_step(ySpan, tl, tr, yStep, &start);

    Grid result = { grid, vertexCount };
    return result;
}

// Render Commands

#define push_render_command(command) ((command*)push_render_command_(RenderCommand_##command, sizeof(command), alignof(command)))

// TODO: overload the arena functions with push buffers b/c buffer.arena gets annoying.
inline void*
push_render_command_(Render_Command_Type type, u32 size, u32 alignment)
{
    Render_Command_Header* header = arena_push_type(gGame->renderCommandBuffer.arena, Render_Command_Header);

    u8* before = (u8*)gGame->renderCommandBuffer.arena.at;
    u8* cmd    = (u8*)arena_push(gGame->renderCommandBuffer.arena, size, alignment);

    header->type = type;
    header->size = down_cast<u32>(size + (cmd - before)); // size includes the alignment offset, if any.

    gGame->renderCommandBuffer.count++;

    return cmd;
}

inline void
set_clear_color(f32 r, f32 g, f32 b, f32 a)
{
    Set_Clear_Color* clearColor = push_render_command(Set_Clear_Color);
    clearColor->r = r;
    clearColor->g = g;
    clearColor->b = b;
    clearColor->a = a;
}

inline void
set_view_matrix(const glm::mat4& matrix)
{
    Set_View_Matrix* viewMatrix = push_render_command(Set_View_Matrix);
    copy_memory(viewMatrix->mat4, &matrix, sizeof(matrix));
}

inline void
set_projection_matrix(const glm::mat4& matrix)
{
    Set_Projection_Matrix* projMatrix = push_render_command(Set_Projection_Matrix);
    copy_memory(projMatrix->mat4, &matrix, sizeof(matrix));
}

inline void
begin_render_pass()
{
    push_render_command(Begin_Render_Pass);
}

inline void
render_debug_lines(glm::vec3* vertices, u32 vertexCount, glm::vec4 color)
{
    Render_Debug_Lines* debugLines = push_render_command(Render_Debug_Lines);
    debugLines->vertices = (f32*)vertices;
    debugLines->vertexCount = vertexCount;
    debugLines->r = color.r;
    debugLines->g = color.g;
    debugLines->b = color.b;
    debugLines->a = color.a;
}
