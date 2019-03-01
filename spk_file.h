#pragma once
#include "common.h"
#include "memory.h"

// NOTE(blake): shader pak files not needed atm.

struct SPK_File
{
};

extern SPK_File
spk_load(buffer32 buffer);
