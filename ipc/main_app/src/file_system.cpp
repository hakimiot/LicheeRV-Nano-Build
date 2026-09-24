#include <filesystem>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include <opencv2/opencv.hpp>
#include "mainapp_log.h"
#include "file_system.h"

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

uint64_t FileSystem::AviTotalSize(const char *directory) {
    uint64_t totalSize = 0;
    
    DIR* dir = opendir(directory);
    if (!dir) {
        LOG_E << "Can not open dir: " << directory;
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string filename = entry->d_name;
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = filename.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".avi") {
                std::string fullPath = directory;
                if (fullPath.back() != '/') {
                    fullPath += '/';
                }
                fullPath += filename;
                
                struct stat file_stat;
                if (stat(fullPath.c_str(), &file_stat) == 0) {
                    totalSize += file_stat.st_size;
                }
            }
        }
    }
    
    closedir(dir);
    return totalSize;
}

double FileSystem::VideoDuration(const std::string& filename)
{
    cv::VideoCapture cap(filename);
    if (!cap.isOpened()) {
        return -1.0;
    }
    
    // 获取总帧数
    double totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
    // 获取帧率
    double fps = cap.get(cv::CAP_PROP_FPS);
    
    cap.release();
    
    if (fps <= 0) {
        return -1.0;
    }
    
    // 总时长（秒）= 总帧数 / 帧率
    return totalFrames / fps;
}

std::vector<AviFileInfo> FileSystem::AviList(const char *directory)
{
    std::vector<AviFileInfo> list;

    DIR* dir = opendir(directory);
    if (!dir) {
        LOG_E << "Can not open dir: " << directory;
        return list;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string filename = entry->d_name;
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = filename.substr(dotPos);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".avi") {
                std::string fullPath = directory;
                if (fullPath.back() != '/') {
                    fullPath += '/';
                }
                fullPath += filename;
                
                struct stat file_stat;
                if (stat(fullPath.c_str(), &file_stat) == 0) {
                    AviFileInfo info = {
                        filename,
                        file_stat.st_size,
                        (int)VideoDuration(std::string(directory) + "/" + filename)
                    };

                    list.push_back(info);
                }
            }
        }
    }
    
    closedir(dir);
    return list;
}

// nlohmann::json_abi_v3_12_0::json FileSystem::AviListJson(const char *directory)
// {
//     nlohmann::json_abi_v3_12_0::json array = nlohmann::json_abi_v3_12_0::json::array();
//     DIR* dir = opendir(directory);
//     if (!dir) {
//         LOG_E << "Can not open dir: " << directory;
//         return array;
//     }

//     struct dirent* entry;
//     while ((entry = readdir(dir)) != nullptr) {
//         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || entry->d_type != DT_DIR) {
//             continue;
//         }

//         nlohmann::json_abi_v3_12_0::json videos = nlohmann::json_abi_v3_12_0::json::array();
//         std::string fullPath = std::string(directory) + "/" + entry->d_name;
//         std::vector<AviFileInfo> infos = AviList(fullPath.c_str());
//         if (infos.size() == 0) {
//             continue;
//         }

//         for (const AviFileInfo& info : infos) {
//             nlohmann::json_abi_v3_12_0::json item;
//             item["file"] = info.filename;
//             item["size"] = info.size;
//             item["sec"] = info.seconds;
//             videos.push_back(item);
//         }

//         nlohmann::json_abi_v3_12_0::json videoItem = {
//             {"date", entry->d_name},
//             {"count", infos.size()},
//             {"videos", videos}
//         };
//         array.push_back(videoItem);
//     }

//     closedir(dir);
//     return array;
// }
