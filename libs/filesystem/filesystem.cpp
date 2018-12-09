//
//  filesystem.cpp
//  surge-vst2
//
//  Created by Keith Zantow on 10/2/18.
//

#ifdef __APPLE__
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC

#include "filesystem.h"

namespace std::experimental::filesystem {
        path::path():
        path("")
        {}
        
        path::path(std::string filePath):
        p(filePath)
        {}
        
        path::operator std::string() {
            return p;
        }
        
        void path::append(std::string s) {
            p.append("/");
            p.append(s);
        }
        
        const char* path::c_str() {
            return p.c_str();
        }
        
        std::string path::generic_string() {
            return p;
        }
        
        path path::filename() {
            auto idx = this->p.find_last_of("/");
            path p(this->p.substr(idx+1));
            return p;
        }
        
        std::string path::extension() {
            auto idx = this->p.find_last_of(".");
            return p.substr(idx);
        }
    
        file::file(std::string filePath):
        p(filePath)
        {}
        
        file::operator class path() {
            return p;
        }
        
        path file::path() {
            return p;
        }
    
    bool exists(path p) {
        FILE *file;
        if ((file = fopen(p.p.c_str(), "r")))
        {
            fclose(file);
            return true;
        }
        return false;
    }
    
    void create_directories(path p) {
        mode_t nMode = 0733; // UNIX style permissions
        int nError = 0;
#if defined(_WIN32)
        nError = _mkdir(p.c_str()); // can be used on Windows
#else
        nError = mkdir(p.c_str(), nMode); // can be used on non-Windows
#endif
        if (nError != 0) {
            // handle your error here
        }
    }
    
    bool is_directory(path p) {
        DIR *dp;
        bool isDir = false;
        if((dp  = opendir(p.c_str())) != NULL && readdir(dp) != NULL) {
            isDir = true;
        }
        if( dp != NULL )
        {
            closedir(dp);
        }
        return isDir;
    }
    
    std::vector<file> directory_iterator(path p) {
        std::vector<file> files;
        DIR *dp;
        struct dirent *dirp;
        if((dp  = opendir(p.c_str())) == NULL) {
//            std::cout << "Error(" << errno << ") opening " << p.generic_string() << std::endl;
        }
        
        while ((dirp = readdir(dp)) != NULL) {
            files.push_back(file(string(dirp->d_name)));
        }
        closedir(dp);
        
        return files;
    }
}

#endif
#endif
