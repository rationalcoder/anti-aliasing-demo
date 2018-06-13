
// NOTE: small b/c malloc/free is being used for a bit for a demo.
constexpr u64 contiguous_size() { return Gigabytes(8); }
constexpr u64 perm_arena_size() { return Megabytes(4); }
constexpr u64 temp_arena_size() { return Megabytes(1); }

constexpr u32 us_per_update() { return 5000; }
constexpr b32 should_ste()    { return false; }

constexpr int target_opengl_version_major() { return 4; }
constexpr int target_opengl_version_minor() { return 3; }

