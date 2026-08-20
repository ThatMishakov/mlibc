#pragma once
#include "ports.h"
#include <stdint.h>

struct OpenFile {
    pmos_right_t io_right;
    pmos_right_t op_right;
    uint64_t flags;
};

struct FsData {
    uint64_t total_size;
    uint64_t array_size;
    struct OpenFile open_files[];
};
