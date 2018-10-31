#include "tanks.h"

static inline void*
stbi_realloc_sized(void* p, umm oldSize, umm newSize)
{
    void* space = allocate(newSize, 8);
    memcpy(space, p, oldSize);

    return space;
}

#define STBI_FREE(p) ((void)(p))
#define STBI_MALLOC(size) allocate(size, 8)
#define STBI_REALLOC_SIZED(p, oldSize, newSize) stbi_realloc_sized(p, oldSize, newSize)

#define STB_IMAGE_IMPLEMENTATION
#define STB_NO_PIC
#define STB_NO_PNM
#define STB_NO_HDR
#define STB_NO_PSD
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"
