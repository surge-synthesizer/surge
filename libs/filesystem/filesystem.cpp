//
//  filesystem.cpp
//  surge-vst2
//
//  Created by Keith Zantow on 10/2/18.
//

#if defined(__APPLE__) || TARGET_RACK
#ifdef __APPLE__
#include "TargetConditionals.h"
#endif
#if defined(TARGET_OS_MAC) || TARGET_RACK

#include <sys/stat.h>
#include "filesystem.h"

namespace std { namespace experimental { namespace filesystem {
    // path class:
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
    
    std::string path::generic_string() const {
        return p;
    }
    
    path path::filename() {
        auto idx = this->p.find_last_of("/");
        path p(this->p.substr(idx+1));
        return p;
    }
    
    path path::extension() {
        auto idx = this->p.find_last_of(".");
        if( idx == std::string::npos )
            return path( "" );

        path res(p.substr(idx));
        return res;
    }
    // emd path class

    // file class:
    file::file(std::string filePath):
    p(filePath)
    {}
    
    file::operator class path() {
        return p;
    }
    
    path file::path() const {
        return p;
    }
    // end file class
    
    // directory_entry class:
    directory_entry::directory_entry(class path p):
    p(p)
    {}
    
    path directory_entry::path() const {
        return p;
    }
    // end directory_entry
    
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
        mode_t nMode = 0755; // UNIX style permissions
        int nError = 0;
#if ! TARGET_RACK
	// FIX THAT
#if defined(_WIN32) 
        nError = _mkdir(p.c_str()); // can be used on Windows
#else
        // create_directories is recursive so this solution
        // nError = mkdir(p.c_str(), nMode); // can be used on non-Windows
        // doesn't work if you are creating two or 3 layers.
        // From: https://stackoverflow.com/questions/2336242/recursive-mkdir-system-call-on-unix
        char* pos;
        char file_path[PATH_MAX];
        sprintf( file_path, "%s", p.c_str() ); // I need a non-const char* so have to copy
        for (pos=strchr(file_path+1, '/'); pos; pos=strchr(pos+1, '/')) {
            *pos='\0';
            if (mkdir(file_path, nMode)==-1) {
                if (errno!=EEXIST) { *pos='/'; return; }
            }
            *pos='/';
        }
        // and clean up the end
        nError = mkdir(file_path, nMode );
#endif
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
        if((dp  = opendir(p.c_str())) == NULL) {
//            std::cout << "Error(" << errno << ") opening " << p.generic_string() << std::endl;
          return files;
        }
        
        // this needs to return the full path not just the relative path
#if WINDOWS
        // This code is only used in WINDOWS on Rack; and on Rack mingw is using a posix impl
        // without readdir_r. So do the old fashioned way
        struct dirent *dirp;
        while( (dirp = readdir(dp)) != NULL ) {
          string fname(dirp->d_name);
#else
        struct dirent dirp, *entry;
        while ( (readdir_r(dp, &dirp, &entry) == 0) && entry != NULL) {
          string fname(dirp.d_name);
#endif
          // Skip . and .. : https://github.com/kurasu/surge/issues/77
          if (fname.compare(".") == 0 || fname.compare("..") == 0) {
              continue;
          }
            
          file res(p.generic_string() + '/' + fname);

          files.push_back(res);
        }

        closedir(dp);
        
        return files;
    }
    
    std::vector<directory_entry> recursive_directory_iterator(const path& src) {
        std::vector<directory_entry> entries;
        for(const auto& entry : directory_iterator(src)) {
            const auto& p = entry.path();
            directory_entry e(p);
            entries.emplace_back(e);
            if (is_directory(p)) {
                std::vector<directory_entry> subdir = recursive_directory_iterator(p);
                for(const auto& subdirEntry : subdir) {
                    entries.emplace_back(subdirEntry);
                }
            }
        }
        return entries;
    }
    
    path relative(const path& p, const path& root) {
        return path(p.generic_string().substr(root.generic_string().length()));
    }
    
    void copy(const path& src, const path& dst, const copy_options options) {
        std::ifstream in(src.generic_string());
        std::ofstream out(dst.generic_string());
        out << in.rdbuf();
    }
    
    void copy_recursive(const path& src, const path& target, const std::function<bool(path)>& predicate) noexcept
    {
        try
        {
            for (const auto& dirEntry : recursive_directory_iterator(src))
            {
                const auto& p = dirEntry.path();
                if (predicate(p))
                {
                    // Create path in target, if not existing.
                    const auto relativeSrc = relative(p, src);
                    auto targetStr = target.generic_string() + '/' + relativeSrc.generic_string();
                    path targetPath(targetStr);
                    if (is_directory(p)) {
                        create_directories(targetPath);
                    } else {
                        // Copy to the targetParentPath which we just created.
                        copy(p, targetPath, copy_options::overwrite_existing);
                    }
                }
            }
        }
        catch (std::exception& e)
        {
            //        std::cout << e.what();
        }
    }
    
    void copy_recursive(const path& src, const path& target) noexcept
    {
        copy_recursive(src, target, [](path p) { return true; });
    }
}}}

#endif
#endif
