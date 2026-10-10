#include <filesystem>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include "mainapp_log.h"
#include "file_system.h"

bool FileSystem::MakeDirs(const std::string& path)
{
    if (path.empty()) return false;

    std::string tmp;
    for (size_t i = 0; i < path.size(); i++) {
        tmp += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            if (tmp == "/" || tmp.empty()) continue;

            if (mkdir(tmp.c_str(), 0755) != 0) {
                if (errno != EEXIST) {
                    LOG_E << "mkdir failed: " << tmp << ", errno: " << errno;
                    return false;
                }
            }
        }
    }
    return true;
}

long long FileSystem::size(const char *filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file) 
        return -1;
    fseek(file, 0, SEEK_END);
    long long size = ftell(file);
    fclose(file);
    return size;
}
