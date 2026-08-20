#pragma once
#include "ports.h"
#include <stdint.h>

struct PmosOpenFile {
    pmos_right_t io_right;
    pmos_right_t op_right;
    uint64_t flags;
};

struct PmosFsData {
    uint64_t total_size;
    uint64_t array_size;
    struct PmosOpenFile open_files[];
};

#define FLAG_ISATTY 0x01
#define FLAG_ISPIPE 0x02