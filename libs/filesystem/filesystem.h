//
//  Filesystem.h
//  surge-vst2
//
//  Created by Keith Zantow on 10/2/18.
//

#ifndef Filesystem_h
#define Filesystem_h

#ifdef __APPLE__
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC

#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

namespace std::experimental::filesystem {
    class path {
    public:
        std::string p;
        
        path();

        path(std::string filePath);
        
        operator std::string();
        
        void append(std::string s);
        
        const char* c_str();
        
        std::string generic_string();
        
        path filename();
        
        std::string extension();
    };
    
    class file {
    public:
        path p;
        
        file(std::string filePath);
        
        operator path();
        
        path path();
    };
    
    bool exists(path p);

    void create_directories(path p);
    
    bool is_directory(path p);
    
    std::vector<file> directory_iterator(path p);
}

#endif
#else
#include <filesystem>
#endif

#endif /* Filesystem_h */
