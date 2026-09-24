#pragma

#include <string>
#include <vector>
// #include "json.hpp"

typedef struct {
    std::string filename;
    long long size;
    int seconds;
} AviFileInfo;

class FileSystem
{
public:
    static long long size(const char *filename);
    static uint64_t AviTotalSize(const char *directory);
    static double VideoDuration(const std::string& filename);

    static std::vector<AviFileInfo> AviList(const char *directory);
    // static nlohmann::json_abi_v3_12_0::json AviListJson(const char *directory);
};