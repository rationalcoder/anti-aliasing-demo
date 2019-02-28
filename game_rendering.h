
struct Debug_Normals
{
    v3* t;
    v3* b;
    v3* n;

    u32 vertexCount;
};

static f32 srgb_to_linear(f32 val);
static v3 srgb_to_linear(v3 c);
static v4 srgb_to_linear(v4 c);
static v3 srgb_to_linear(f32 r, f32 g, f32 b);
static v4 srgb_to_linear(f32 r, f32 g, f32 b, f32 a);

static AABB compute_bounding_box(const Static_Mesh& mesh);

static Static_Mesh make_cube_mesh(f32 halfWidth);
static Static_Mesh make_solid_cube(f32 halfWidth, v3 color);
static Static_Mesh make_box(v3 origin, v3 span, v3 color);

static MTL_Material* find_material(const MTL_File& mtl, string32 name);
static Texture load_texture(string32 path);
static Static_Mesh load_static_mesh(const OBJ_File& obj, const MTL_File& mtl,
                                    const char* texturePath, const char* tag);

static Debug_Normals make_debug_normals(const Static_Mesh& mesh);



#define push_render_command(command) ((command*)push_render_command_(RenderCommand_##command, sizeof(command), alignof(command)))

static void* push_render_command_(Render_Command_Type type, u32 size, u32 alignment);

// NOTE(blake): all colors passed are expected to be sRGB.

static void set_render_target(Push_Buffer& buffer);

// Frame begin commands:

static void cmd_set_clear_color(f32 r, f32 g, f32 b);
static void cmd_set_clear_color(v3 c);
static void cmd_set_viewport(Game_Resolution res);
static void cmd_set_viewport(u32 x, u32 y, u32 w, u32 h);
static void cmd_set_view_matrix(const mat4& matrix);
static void cmd_set_projection_matrix(const mat4& matrix);
static void cmd_set_aa_technique(AA_Technique t);
static void cmd_resize_buffers(u32 w, u32 h);
static void cmd_resize_buffers(Game_Resolution res);

// Exec commands:

static void cmd_render_debug_lines(v3* vertices, u32 vertexCount, v3 color);
static void cmd_render_debug_cubes(view32<v3> centers, f32 halfWidth, v3 color);
static void cmd_render_debug_cube(v3 position, f32 halfWidth, v3 color);
static void cmd_render_static_mesh(const Static_Mesh& mesh, const mat4& modelMatrix);
static void cmd_render_point_light(v3 position, v3 color);
static void render_commands(const Push_Buffer& buffer);
