#pragma once
#include "common.h"
#include "memory.h"

enum Render_Command_Type : u32
{
    RenderCommand_Set_Clear_Color,
    RenderCommand_Set_Viewport,
    RenderCommand_Set_View_Matrix,
    RenderCommand_Set_Projection_Matrix,

    RenderCommand_Begin_Render_Pass,

    RenderCommand_Render_Debug_Lines,
    RenderCommand_Render_Static_Mesh,
    RenderCommand_Render_Textured_Quad,
    RenderCommand_Render_Point_Light,
    RenderCommand_Count_
};

struct Set_Clear_Color
{
    void* _staged;
    f32 r;
    f32 g;
    f32 b;
    f32 a;
};

struct Set_Viewport
{
    void* _staged;
    s32 x;
    s32 y;
    s32 width;
    s32 height;
};

struct Set_View_Matrix
{
    void* _staged;
    f32 mat4[16];
};

struct Set_Projection_Matrix
{
    void* _staged;
    f32 mat4[16];
};

struct Begin_Render_Pass
{
    void* _staged;
};

struct Render_Textured_Quad
{
    void* _staged;
    f32 topLeftX;
    f32 topLeftY;
    f32 topLeftZ;

    f32 bottomRightX;
    f32 bottomRightY;
    f32 bottomRightZ;
};

struct Render_Static_Mesh
{
    void* _staged;
};

struct Render_Point_Light
{
    void* _staged;
};

struct Render_Debug_Lines
{
    void* _staged;
    f32* vertices; // 3D
    u32 vertexCount;
    f32 r, g, b, a;
};

struct Render_Command_Header
{
    Render_Command_Type type;
    u32 size;
};

template <typename T_> inline T_*
render_command_after(Render_Command_Header* header)
{
    return (T_*)payload_after(header);
}

// workspace is intended to be sub-allocated from storage and returned.
extern b32
renderer_init(Memory_Arena* storage, Memory_Arena* workspace);

extern b32
renderer_exec(Memory_Arena* workspace, void* commands, u32 count);
