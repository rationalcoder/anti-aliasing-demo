#pragma once
#include "common.h"
#include "memory.h"
#include "mesh.h"

enum Render_Command_Type : u32
{
    // Commands for renderer_begin_frame().
    RenderCommand_Set_Clear_Color,
    RenderCommand_Set_Viewport,
    RenderCommand_Set_View_Matrix,
    RenderCommand_Set_Projection_Matrix,
    RenderCommand_Set_AA_Technique,

    // Not actually used, but useful for sanity checks.
    RenderCommand_Begin_Render_Pass,

    // Commands for renderer_exec().
    RenderCommand_Render_Debug_Lines,
    RenderCommand_Render_Debug_Cubes,
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
    s32 w;
    s32 h;
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

enum AA_Technique : u32
{
    FXAA,
    MSAA_2X,
    MSAA_4X,
    MSAA_8X,
    MSAA_16X,
    MSAA_2X_FXAA,
    MSAA_4X_FXAA,
    MSAA_8X_FXAA,
    MSAA_16X_FXAA,
};

struct Set_AA_Technique
{
    void* _staged;
    AA_Technique technique;
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
    Static_Mesh mesh;
    f32 modelMatrix[16];
};

struct Render_Point_Light
{
    void* _staged;
    f32 x, y, z;
    f32 r, g, b;
};

struct Render_Debug_Lines
{
    void* _staged;
    f32* vertices; // 3D
    u32 vertexCount;
    f32 r, g, b;
};

struct Render_Debug_Cubes
{
    void* _staged;
    f32* centers; // 3D
    u32 count;
    f32 halfWidth;
    f32 r, g, b;
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

// `workspace` is intended to be sub-allocated from storage and returned.
extern b32
renderer_init(Memory_Arena* storage, Memory_Arena* workspace);

// @Hack
extern void*
renderer_upload_imgui_texture_atlas(void* data, u32 w, u32 h, u32 bbp);

extern void
renderer_begin_frame(Memory_Arena* workspace, void* commands, u32 count);

extern b32
renderer_exec(Memory_Arena* workspace, void* commands, u32 count);

extern void
renderer_end_frame(Memory_Arena* workspace, struct ImDrawData* data);
