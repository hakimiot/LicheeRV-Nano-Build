#include <filesystem>
#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <algorithm>
#include <opencv2/opencv.hpp>
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
