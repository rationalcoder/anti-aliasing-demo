
static void
points_by_step(u32 n, v3 from, v3 step, v3** out)
{
    v3* cur = *out;
    v3* onePastLast = cur + n;
    for (; cur != onePastLast; cur++, from += step)
        *cur = from;

    *out = onePastLast;
}

static void
points_by_step(u32 n, v3 from, v3 step, v3* out)
{
    points_by_step(n, from, step, &out);
}

static void
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

static void
lines_by_step(u32 n, v3 p1, v3 p2, v3 step, v3* out)
{
    lines_by_step(n, p1, p2, step, &out);
}

static void
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

static Grid
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

static Cube
make_cube(f32 halfWidth)
{
    Cube result = {};

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

    result.vertices = allocate_array_copy(array_size(vertices), f32, vertices);
    result.indices  = allocate_array_copy(array_size(indices),  u8,  indices);

    result.vertexCount = array_size(vertices) / 3;
    result.indexCount  = array_size(indices);

    return result;
}

static AABB
compute_bounding_box(const v3* vertices, u32 count)
{
    AABB result;

    f32 minX = -FLT_MAX;
    f32 minY = -FLT_MAX;
    f32 minZ = -FLT_MAX;

    f32 maxX = 0;
    f32 maxY = 0;
    f32 maxZ = 0;

    for (u32 i = 0; i < count; i++) {
        const v3& v = vertices[i];
        if (v.x > maxX) maxX = v.x; else if (v.x < minX) minX = v.x;
        if (v.y > maxY) maxY = v.y; else if (v.y < minY) minY = v.y;
        if (v.z > maxZ) maxZ = v.z; else if (v.z < minZ) minZ = v.z;
    }

    result.min.x = minX;
    result.min.y = minY;
    result.min.z = minZ;

    result.max.x = maxX;
    result.max.y = maxY;
    result.max.z = maxZ;

    return result;
}


