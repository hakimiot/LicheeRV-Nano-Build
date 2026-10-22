#include <filesystem>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include "mainapp_log.h"
#include "file_system.h"

// 判断是否是目录
static bool IsDir(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

// 判断是否是普通文件
static bool IsRegularFile(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

// 判断文件名是否以 .json 结尾
static bool HasJsonSuffix(const char* name)
{
    size_t len = strlen(name);
    return len >= 6 && strcmp(name + len - 5, ".json") == 0;
}

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

bool FileSystem::ScanMediaJson(nlohmann::json& out, std::string& error, const std::string& date)
{
    const std::string root = "/media";

    out = nlohmann::json::object();
    out["size"]  = 0;
    out["files"] = nlohmann::json::array();

    error.clear();

    // 收集要扫描的日期目录
    std::vector<std::string> dateDirs;

    if (!date.empty()) {
        // 只处理指定日期
        std::string subDir = root + "/" + date;
        if (IsDir(subDir)) {
            dateDirs.push_back(date);
        } else {
            LOG_W << "Date dir not found: " << subDir;
            return true;   // 空结构，不算错误
        }
    } else {
        // 扫描 /media 下所有一级子目录
        DIR* dp = opendir(root.c_str());
        if (!dp) {
            error = "Cannot open dir: " + root;
            LOG_E << error;
            return false;
        }

        struct dirent* entry;
        while ((entry = readdir(dp)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            if (entry->d_name[0] == '.')
                continue;   // 跳过隐藏目录

            std::string full = root + "/" + entry->d_name;
            if (IsDir(full)) {
                dateDirs.push_back(entry->d_name);
            }
        }
        closedir(dp);

        std::sort(dateDirs.begin(), dateDirs.end());
    }

    // 逐个日期目录读取 .json
    uint64_t totalSize = 0;

    for (const auto& d : dateDirs) {
        std::string subDirPath = root + "/" + d;

        nlohmann::json dateNode;
        dateNode["date"]  = d;
        dateNode["size"]  = 0;
        dateNode["files"] = nlohmann::json::array();

        uint64_t dateSize = 0;

        DIR* subDp = opendir(subDirPath.c_str());
        if (!subDp) {
            LOG_W << "Cannot open dir: " << subDirPath;
            out["files"].push_back(std::move(dateNode));
            continue;
        }

        struct dirent* fileEntry;
        while ((fileEntry = readdir(subDp)) != nullptr) {
            if (strcmp(fileEntry->d_name, ".") == 0 ||
                strcmp(fileEntry->d_name, "..") == 0)
                continue;

            if (!HasJsonSuffix(fileEntry->d_name))
                continue;

            std::string path = subDirPath + "/" + fileEntry->d_name;
            if (!IsRegularFile(path))
                continue;

            try {
                std::ifstream ifs(path);
                if (!ifs.is_open()) {
                    LOG_W << "Cannot open: " << path;
                    continue;
                }

                nlohmann::json item;
                ifs >> item;
                dateNode["files"].push_back(std::move(item));
                dateSize++;
            } catch (const std::exception& e) {
                LOG_E << "Parse JSON failed: " << path << " : " << e.what();
            }
        }
        closedir(subDp);

        dateNode["size"] = dateSize;
        totalSize += dateSize;

        out["files"].push_back(std::move(dateNode));
    }

    out["size"] = totalSize;

    LOG_I << "ScanMediaJson: date=" << (date.empty() ? "ALL" : date)
          << " dirs=" << out["files"].size()
          << " files=" << totalSize;

    return true;
}
