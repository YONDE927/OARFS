#pragma once

#define FUSE_USE_VERSION 31
#include "filesystem.h"
#include <fuse3/fuse.h>

int fusemain(FileSystem& fs_ref, int argc, char *argv[]);
