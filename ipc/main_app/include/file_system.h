#pragma

#include <string>
#include <vector>
// #include "json.hpp"

#define SIZE_KB 1024
#define SIZE_MB (1024 * SIZE_KB)
#define SIZE_GB (1024 * SIZE_MB)

#define SIZE_128MB (128 * SIZE_MB)

typedef struct {
    std::string filename;
    long long size;
    int seconds;
} AviFileInfo;

namespace FileSystem
{
    bool MakeDirs(const std::string& path);

    long long size(const char *filename);
    uint64_t AviTotalSize(const char *directory);
    double VideoDuration(const std::string& filename);

    std::vector<AviFileInfo> AviList(const char *directory);
};
