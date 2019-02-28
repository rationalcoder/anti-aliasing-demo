
static inline void*
stbi_realloc_sized(void* p, umm oldSize, umm newSize);

#define STBI_FREE(p) ((void)(p))
#define STBI_MALLOC(size) allocate(size, 8)
#define STBI_REALLOC_SIZED(p, oldSize, newSize) stbi_realloc_sized(p, oldSize, newSize)

#ifdef STB_IMPLEMENTATION

static inline void*
stbi_realloc_sized(void* p, umm oldSize, umm newSize)
{
    void* space = allocate(newSize, 8);
    memcpy(space, p, oldSize);

    return space;
}
#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_NO_PIC
#define STB_NO_PNM
#define STB_NO_HDR
#define STB_NO_PSD
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_SPRINTF_IMPLEMENTATION

#endif

#include "stb_rectpack.h"
#include "stb_image.h"
#include "stb_truetype.h"
#include "stb_sprintf.h"

