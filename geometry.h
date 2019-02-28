
struct AABB
{
    v3 min;
    v3 max;
};

struct Grid
{
    v3* vertices;
    u32 vertexCount;
};

struct Cube
{
    f32* vertices;
    u8*  indices;

    u32 vertexCount;
    u32 indexCount;
};

static void points_by_step(u32 n, v3 from, v3 step, v3** out);
static void points_by_step(u32 n, v3 from, v3 step, v3* out);
static void lines_by_step(u32 n, v3 p1, v3 p2, v3 step, v3** out);
static void lines_by_step(u32 n, v3 p1, v3 p2, v3 step, v3* out);
static void lines_by_step(u32 n, v3 p1, v3 p2, v3 step1, v3 step2, v3** out);

static AABB compute_bounding_box(const v3* vertices, u32 count);
static Grid make_xy_grid(int xMax, int yMax);
static Cube make_cube(f32 halfWidth);


