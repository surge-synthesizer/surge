//
//  Filesystem.h
//  surge-vst2
//
//  Created by Keith Zantow on 10/2/18.
//

#ifndef Filesystem_h
#define Filesystem_h

#include <functional>

#if defined(__APPLE__) || TARGET_RACK
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#if defined(TARGET_OS_MAC) || TARGET_RACK

#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>

namespace std { namespace experimental { namespace filesystem {
    class path {
    public:
        static constexpr char preferred_separator = '/';
        std::string p;
        
        path();

        path(std::string filePath);
        
        operator std::string();
        
        void append(std::string s);
        
        const char* c_str();
        
        std::string generic_string() const;
        
        path filename();
        
        path extension();
    };
    
    class file {
    public:
        path p;
        
        file(std::string filePath);
        
        operator path();
        
        path path() const;
    };
    
    class directory_entry {
    public:
        path p;

        directory_entry(path p);

        path path() const;
    };
    
    bool exists(path p);

    void create_directories(path p);
    
    bool is_directory(path p);
    
    std::vector<file> directory_iterator(path p);
    
    std::vector<directory_entry> recursive_directory_iterator(const path& src);
    
    path relative(const path& p, const path& root);
    
    enum copy_options {
        overwrite_existing = 1
    };
    
    void copy(const path& src, const path& dst, const copy_options options);

    // Exras:
    void copy_recursive(const path& src, const path& target, const std::function<bool(path)>& predicate) noexcept;

    void copy_recursive(const path& src, const path& target) noexcept;
}}}

#endif
#else
#include <filesystem>
#endif

#endif /* Filesystem_h */
