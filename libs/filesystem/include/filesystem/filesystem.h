//
//  Filesystem.h
//  surge-vst2
//
//  Created by Keith Zantow on 10/2/18.
//

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Surge { namespace filesystem {
    class path {
    public:
        static constexpr char preferred_separator = '/';
        std::string p;
        
        path();

        path(std::string filePath);
        
        operator std::string();
        
        void append(std::string s);
        path& operator /=(const path& path) { append(path.p); return *this; }
        
        const char* c_str();
        
        std::string generic_string() const;
        
        path filename() const;
        path stem() const;
       
        path extension() const;
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
} // namespace filesystem

} // namespace Surge
