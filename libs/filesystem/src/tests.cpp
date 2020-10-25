#if 0
#include <filesystem>
namespace fs = std::filesystem;
#else
#include "filesystem/filesystem.h"
namespace fs = Surge::filesystem;
#endif

#include "catch2/catch2.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
struct TmpDir : fs::path
{
   void ensureInitialized()
   {
      if (empty())
      {
         char temp_late[] = "/tmp/surge-tests-filesystem-XXXXXX";
         REQUIRE(mkdtemp(temp_late) != nullptr);
         fs::path::operator=(fs::path{temp_late});
         REQUIRE(!empty());
         REQUIRE(fs::is_directory(*this));
      }
   }

   ~TmpDir()
   {
      if (!empty())
         fs::remove(*this);
   }
} tmpDir;
} // anonymous namespace

// Less noise than with an exception matcher
#define REQUIRE_THROWS_FS_ERROR(expr, errcode) do { \
    try { expr; FAIL("Did not throw: " #expr); } \
    catch (fs::filesystem_error & e) { REQUIRE(e.code() == std::errc::errcode); } \
} while (0)

TEST_CASE("Filesystem", "[filesystem]")
{
   tmpDir.ensureInitialized();

   SECTION("Directory Iterators")
   {
      SECTION("Reports errors at construction")
      {
         std::error_code ec;
         const fs::path p{"/dev/null"};
         SECTION("directory_iterator")
         {
            REQUIRE_THROWS_FS_ERROR(fs::directory_iterator{p}, not_a_directory);
            fs::directory_iterator it{p, ec};
            REQUIRE(ec == std::errc::not_a_directory);
         }
         SECTION("recursive_directory_iterator")
         {
            REQUIRE_THROWS_FS_ERROR(fs::recursive_directory_iterator{p}, not_a_directory);
            fs::recursive_directory_iterator it{p, ec};
            REQUIRE(ec == std::errc::not_a_directory);
         }
      }

      SECTION("Reports errors during recursion")
      {
         const fs::path p{tmpDir / fs::path{"dir"}};
         REQUIRE(fs::create_directory(p));
         const fs::path denied{p / fs::path{"denied"}};
         REQUIRE(fs::create_directory(denied));
         REQUIRE(chmod(denied.c_str(), 0) == 0);
         fs::recursive_directory_iterator it{p};
         REQUIRE_THROWS_FS_ERROR(++it, permission_denied);
         REQUIRE_THROWS_FS_ERROR(fs::remove_all(p), permission_denied);
         REQUIRE(fs::remove(denied));
         REQUIRE(fs::remove(p));
      }

      SECTION("Skips . and ..")
      {
         std::error_code ec;
         const fs::path p{tmpDir / fs::path{"dir"}};
         REQUIRE(fs::create_directory(p));
         SECTION("directory_iterator")
         {
            REQUIRE(fs::directory_iterator{p} == fs::directory_iterator{});
            REQUIRE(fs::directory_iterator{p, ec} == fs::directory_iterator{});
            REQUIRE_FALSE(ec);
         }
         SECTION("directory_iterator")
         {
            REQUIRE(fs::recursive_directory_iterator{p} == fs::recursive_directory_iterator{});
            REQUIRE(fs::recursive_directory_iterator{p, ec} == fs::recursive_directory_iterator{});
            REQUIRE_FALSE(ec);
         }
         REQUIRE(fs::remove(p));
         REQUIRE_FALSE(fs::remove(p));
      }

      SECTION("Visits each directory entry exactly once")
      {
         std::vector<std::string> paths = {
             "dir/1_entry/1a.file",
             "dir/2_entries/2a.dir",
             "dir/2_entries/2b.file",
             "dir/3_entries/3a.dir",
             "dir/3_entries/3b.dir",
             "dir/3_entries/3c.file",
             "file.file",
         };

         std::set<std::string> filenames;
         std::transform(paths.begin(), paths.end(), std::inserter(filenames, filenames.begin()),
                        [](const auto& p) {
                           auto fn(fs::path{p}.filename());
                           REQUIRE_FALSE(fn.empty());
                           return fn.native();
                        });
         REQUIRE(filenames.size() == paths.size());

         const fs::path rootdir{tmpDir / fs::path{"dir"}};
         REQUIRE(fs::create_directory(rootdir));
         for (auto& pp : paths)
         {
            const fs::path p{rootdir / fs::path{pp}};
            if (p.extension().native() == ".file")
            {
               fs::path f{p};
               f.remove_filename();
               fs::create_directories(f);
               const std::ofstream of{p};
               REQUIRE(of.good());
            }
            else
            {
               REQUIRE(fs::create_directories(p) > 0);
               REQUIRE(fs::is_directory(p));
            }
         }

         const auto iterate = [&](auto& it) {
            REQUIRE(it->path().native() != "");
            for (const fs::path& p : it)
            {
               auto fit{filenames.find(p.filename().native())};
               if (fit != end(filenames))
                  filenames.erase(fit);
               if (p.extension().native() == ".file")
               {
                  REQUIRE(fs::is_regular_file(p));
                  REQUIRE_FALSE(fs::is_directory(p));
               }
               else
               {
                  REQUIRE_FALSE(fs::is_regular_file(p));
                  REQUIRE(fs::is_directory(p));
               }
            }
         };

         SECTION("directory_iterator")
         {
            for (auto& pp : paths)
            {
               const fs::path dir{(rootdir / fs::path{pp}).remove_filename()};
               fs::directory_iterator it{dir};
               iterate(it);
            }
         }

         SECTION("recursive_directory_iterator")
         {
            fs::recursive_directory_iterator it{rootdir};
            iterate(it);
         }

         REQUIRE(fs::remove_all(rootdir) == 12);
         REQUIRE(filenames.empty());
      }
   }

   SECTION("Operations")
   {
      SECTION("create_directories")
      {
         REQUIRE_THROWS_FS_ERROR(fs::create_directories(fs::path{"/dev/null"}), file_exists);
         REQUIRE_THROWS_FS_ERROR(fs::create_directories(fs::path{"/dev/null/dir"}), not_a_directory);

         const fs::path basep{tmpDir / fs::path{"dir"}};
         const fs::path p{basep / fs::path{"this/is//a///test"}};
         REQUIRE(fs::create_directories(p));
         REQUIRE(fs::is_directory(p));
         REQUIRE(fs::create_directories(p) == 0);
         REQUIRE(fs::remove_all(basep) == 5);
         REQUIRE_FALSE(fs::exists(basep));
         REQUIRE(fs::remove_all(basep) == 0);
         REQUIRE_FALSE(fs::is_directory(p));
      }

      SECTION("create_directory")
      {
         REQUIRE_FALSE(fs::create_directory(fs::path{"/dev/null"}));
         REQUIRE_THROWS_FS_ERROR(fs::create_directory(fs::path{"/dev/null/dir"}), not_a_directory);

         const fs::path p{tmpDir / fs::path{"dir"}};
         REQUIRE_FALSE(fs::exists(p));
         REQUIRE(fs::create_directory(p));
         REQUIRE_FALSE(fs::create_directory(p));
         REQUIRE(fs::remove(p));
         REQUIRE_FALSE(fs::exists(p));
         REQUIRE_FALSE(fs::remove(p));
         REQUIRE_FALSE(fs::is_directory(p));
      }

      SECTION("exists")
      {
         REQUIRE(fs::exists(fs::path{"."}));
         const fs::path p{tmpDir / fs::path{"dir"}};
         REQUIRE(fs::create_directory(p));
         REQUIRE(chmod(p.c_str(), 0) == 0);
         REQUIRE_THROWS_FS_ERROR(fs::exists(p / fs::path{"file"}), permission_denied);
         REQUIRE(fs::remove(p));
         REQUIRE_FALSE(fs::remove(p));
      }

      SECTION("file_size")
      {
         std::error_code ec;

         REQUIRE_THROWS_FS_ERROR(fs::file_size(fs::path{"."}), is_a_directory);
         REQUIRE(fs::file_size(fs::path{"."}, ec) == std::uintmax_t(-1));
         REQUIRE(ec == std::errc::is_a_directory);

         REQUIRE_THROWS_FS_ERROR(fs::file_size(fs::path{"/dev/null"}), not_supported);
         REQUIRE(fs::file_size(fs::path{"/dev/null"}, ec) == std::uintmax_t(-1));
         REQUIRE(ec == std::errc::not_supported);

         const fs::path p{tmpDir / fs::path{"file"}};
         const char testdata[] = "testdata";
         std::ofstream of{p};
         REQUIRE(of.good());
         REQUIRE(fs::file_size(p) == 0);
         of << testdata;
         of.close();
         REQUIRE(fs::file_size(p) == sizeof(testdata) - 1);
         REQUIRE(fs::file_size(p, ec) == sizeof(testdata) - 1);
         REQUIRE(!ec);
         REQUIRE(fs::remove(p));
         REQUIRE_FALSE(fs::remove(p));
      }

      SECTION("is_directory")
      {
         REQUIRE(fs::is_directory(fs::path{"."}));
         REQUIRE(fs::is_directory(fs::path{"/"}));
         REQUIRE_FALSE(fs::is_directory(fs::path{"/dev/null"}));
         REQUIRE_FALSE(fs::is_directory(fs::path{"/dev/null/dir"}));
      }

      SECTION("is_regular_file")
      {
         const fs::path p{tmpDir / fs::path{"file"}};
         const std::ofstream of{p};
         REQUIRE(of.good());
         REQUIRE(fs::is_regular_file(p));
         REQUIRE(fs::remove(p));
         REQUIRE_FALSE(fs::remove(p));

         REQUIRE_FALSE(fs::is_regular_file(fs::path{"."}));
         REQUIRE_FALSE(fs::is_regular_file(fs::path{"/"}));
         REQUIRE_FALSE(fs::is_regular_file(fs::path{"/dev/null"}));
         REQUIRE_FALSE(fs::is_regular_file(fs::path{"/dev/null/file"}));
      }

      SECTION("remove")
      {
         {
            const fs::path p{tmpDir / fs::path{"file"}};
            const std::ofstream of{p};
            REQUIRE(of.good());
            REQUIRE(fs::remove(p));
            REQUIRE_FALSE(fs::remove(p));
         }
         {
            const fs::path p{tmpDir / fs::path{"dir"}};
            REQUIRE(fs::create_directory(p));
            REQUIRE(fs::remove(p / fs::path{}));
            REQUIRE_FALSE(fs::remove(p / fs::path{}));
         }
         {
            const fs::path p{tmpDir / fs::path{"dir"}};
            REQUIRE(fs::create_directories(p / fs::path{"dir"}));
            REQUIRE_THROWS_FS_ERROR(fs::remove(p), directory_not_empty);
            REQUIRE(fs::remove_all(p) == 2);
         }
      }

      SECTION("remove_all")
      {
         const fs::path p{tmpDir / fs::path{"file"}};
         const std::ofstream of{p};
         REQUIRE(of.good());
         REQUIRE(fs::remove_all(p) == 1);
         REQUIRE(fs::remove_all(p) == 0);
      }
   }

   SECTION("Path")
   {
      SECTION("operator /=, /")
      {
         REQUIRE((fs::path{"foo"} / fs::path{"/bar"}).native() == "/bar");
         REQUIRE((fs::path{"foo"} /= fs::path{"/bar"}).native() == "/bar");
         REQUIRE((fs::path{"foo"} / fs::path{}).native() == "foo/");
         REQUIRE((fs::path{"foo"} /= fs::path{}).native() == "foo/");
         REQUIRE((fs::path{"foo"} / fs::path{"bar"}).native() == "foo/bar");
         REQUIRE((fs::path{"foo"} /= fs::path{"bar"}).native() == "foo/bar");
         REQUIRE((fs::path{"foo/"} / fs::path{"bar"}).native() == "foo/bar");
         REQUIRE((fs::path{"foo/"} /= fs::path{"bar"}).native() == "foo/bar");
      }

      SECTION("operator +=")
      {
         REQUIRE((fs::path{} += fs::path{}).native() == "");
         REQUIRE((fs::path{"foo"} += fs::path{"bar"}).native() == "foobar");
         REQUIRE((fs::path{"foo"} += fs::path{"///bar"}).native() == "foo///bar");
         REQUIRE((fs::path{"file"} += fs::path{".extension"}).native() == "file.extension");
      }

      SECTION("remove_filename")
      {
         // https://en.cppreference.com/w/cpp/filesystem/path/remove_filename
         REQUIRE(fs::path{"foo/bar"}.remove_filename().native() == "foo/");
         REQUIRE(fs::path{"foo/"}.remove_filename().native() == "foo/");
         REQUIRE(fs::path{"/foo"}.remove_filename().native() == "/");
         REQUIRE(fs::path{"/"}.remove_filename().native() == "/");
         REQUIRE(fs::path{}.remove_filename().native() == "");
      }

      SECTION("replace_filename")
      {
         REQUIRE(fs::path{}.replace_filename(fs::path{}).native() == "");
         REQUIRE(fs::path{"/"}.replace_filename(fs::path{}).native() == "/");
         REQUIRE(fs::path{"/"}.replace_filename(fs::path{"file.ext"}).native() == "/file.ext");
         REQUIRE(fs::path{"file.ext"}.replace_filename(fs::path{"other.file"}).native() == "other.file");
         REQUIRE(fs::path{"/foo/bar.txt"}.replace_filename(fs::path{"baz.mid"}).native() == "/foo/baz.mid");
         REQUIRE(fs::path{"/foo/bar.txt"}.replace_filename(fs::path{"baz/qux.mid"}).native() == "/foo/baz/qux.mid");
         REQUIRE(fs::path{"/foo/bar.txt"}.replace_filename(fs::path{"/baz/qux.mid"}).native() == "/baz/qux.mid");
         REQUIRE(fs::path{"/foo/bar.txt"}.replace_filename(fs::path{}).native() == "/foo/");
         REQUIRE(fs::path{"/foo//bar.txt"}.replace_filename(fs::path{}).native() == "/foo//");
      }

      SECTION("replace_extension")
      {
         REQUIRE(fs::path{"file.ext"}.replace_extension().native() == "file");
         REQUIRE(fs::path{"file.ext"}.replace_extension(fs::path{"newext"}).native() == "file.newext");
         REQUIRE(fs::path{"file.ext"}.replace_extension(fs::path{".newext"}).native() == "file.newext");
         REQUIRE(fs::path{"file.ext"}.replace_extension(fs::path{"..newext"}).native() == "file..newext");
         REQUIRE(fs::path{"file.ext"}.replace_extension(fs::path{"/"}).native() == "file./");
         REQUIRE(fs::path{"file.ext"}.replace_extension(fs::path{"./"}).native() == "file./");
         REQUIRE(fs::path{"file"}.replace_extension().native() == "file");
         REQUIRE(fs::path{".file"}.replace_extension().native() == ".file");
         REQUIRE(fs::path{".file."}.replace_extension().native() == ".file");
         REQUIRE(fs::path{".file.ext"}.replace_extension().native() == ".file");
         REQUIRE(fs::path{".file..ext"}.replace_extension().native() == ".file.");
         REQUIRE(fs::path{"."}.replace_extension().native() == ".");
         REQUIRE(fs::path{".."}.replace_extension().native() == "..");
         REQUIRE(fs::path{"/."}.replace_extension().native() == "/.");
         REQUIRE(fs::path{"/.."}.replace_extension().native() == "/..");
      }

      SECTION("filename, has_filename")
      {
         REQUIRE(fs::path{}.filename().native() == "");
         REQUIRE_FALSE(fs::path{}.has_filename());

         // https://en.cppreference.com/w/cpp/filesystem/path/filename
         REQUIRE(fs::path{"/foo/bar.txt"}.filename().native() == "bar.txt");
         REQUIRE(fs::path{"/foo/bar.txt"}.has_filename());

         REQUIRE(fs::path{"/foo/.bar"}.filename().native() == ".bar");
         REQUIRE(fs::path{"/foo/.bar"}.has_filename());

         REQUIRE(fs::path{"/foo/bar/"}.filename().native() == "");
         REQUIRE_FALSE(fs::path{"/foo/bar/"}.has_filename());

         REQUIRE(fs::path{"/foo/."}.filename().native() == ".");
         REQUIRE(fs::path{"/foo/."}.has_filename());

         REQUIRE(fs::path{"/foo/.."}.filename().native() == "..");
         REQUIRE(fs::path{"/foo/.."}.has_filename());

         REQUIRE(fs::path{"."}.filename().native() == ".");
         REQUIRE(fs::path{"."}.has_filename());

         REQUIRE(fs::path{".."}.filename().native() == "..");
         REQUIRE(fs::path{".."}.has_filename());

         REQUIRE(fs::path{"/"}.filename().native() == "");
         REQUIRE_FALSE(fs::path{"/"}.has_filename());

         REQUIRE(fs::path{"//host"}.filename().native() == "host");
         REQUIRE(fs::path{"//host"}.has_filename());
      }

      SECTION("stem, has_stem")
      {
         REQUIRE(fs::path{}.stem().native() == "");
         REQUIRE_FALSE(fs::path{}.has_stem());

         REQUIRE(fs::path{"."}.stem().native() == ".");
         REQUIRE(fs::path{"."}.has_stem());

         REQUIRE(fs::path{".."}.stem().native() == "..");
         REQUIRE(fs::path{".."}.has_stem());

         REQUIRE(fs::path{"..."}.stem().native() == "..");
         REQUIRE(fs::path{"..."}.has_stem());

         // https://en.cppreference.com/w/cpp/filesystem/path/stem
         REQUIRE(fs::path{"/foo/bar.txt"}.stem().native() == "bar");
         REQUIRE(fs::path{"/foo/bar.txt"}.has_stem());

         REQUIRE(fs::path{"/foo/.bar"}.stem().native() == ".bar");
         REQUIRE(fs::path{"/foo/.bar"}.has_stem());

         REQUIRE(fs::path{"foo.bar.baz.tar"}.stem().native() == "foo.bar.baz");
         REQUIRE(fs::path{"foo.bar.baz.tar"}.has_stem());
      }

      SECTION("extension, has_extension")
      {
         REQUIRE(fs::path{}.extension().native() == "");
         REQUIRE_FALSE(fs::path{}.has_extension());

         // https://en.cppreference.com/w/cpp/filesystem/path/extension
         REQUIRE(fs::path{"/foo/bar.txt"}.extension().native() == ".txt");
         REQUIRE(fs::path{"/foo/bar.txt"}.has_extension());

         REQUIRE(fs::path{"/foo/bar."}.extension().native() == ".");
         REQUIRE(fs::path{"/foo/bar."}.has_extension());

         REQUIRE(fs::path{"/foo/bar"}.extension().native() == "");
         REQUIRE_FALSE(fs::path{"/foo/bar"}.has_extension());

         REQUIRE(fs::path{"/foo/bar.txt/bar.cc"}.extension().native() == ".cc");
         REQUIRE(fs::path{"/foo/bar.txt/bar.cc"}.has_extension());

         REQUIRE(fs::path{"/foo/bar.txt/bar."}.extension().native() == ".");
         REQUIRE(fs::path{"/foo/bar.txt/bar."}.has_extension());

         REQUIRE(fs::path{"/foo/bar.txt/bar"}.extension().native() == "");
         REQUIRE_FALSE(fs::path{"/foo/bar.txt/bar"}.has_extension());

         REQUIRE(fs::path{"/foo/."}.extension().native() == "");
         REQUIRE_FALSE(fs::path{"/foo/."}.has_extension());

         REQUIRE(fs::path{"/foo/.."}.extension().native() == "");
         REQUIRE_FALSE(fs::path{"/foo/.."}.has_extension());

         REQUIRE(fs::path{"/foo/.hidden"}.extension().native() == "");
         REQUIRE_FALSE(fs::path{"/foo/.hidden"}.has_extension());

         REQUIRE(fs::path{"/foo/..bar"}.extension().native() == ".bar");
         REQUIRE(fs::path{"/foo/..bar"}.has_extension());
      }

      SECTION("is_absolute, is_relative")
      {
         REQUIRE_FALSE(fs::path{}.is_absolute());
         REQUIRE(fs::path{}.is_relative());
         REQUIRE(fs::path{"/"}.is_absolute());
         REQUIRE_FALSE(fs::path{"/"}.is_relative());
         REQUIRE(fs::path{"/dir/"}.is_absolute());
         REQUIRE_FALSE(fs::path{"/dir/"}.is_relative());
         REQUIRE(fs::path{"/file"}.is_absolute());
         REQUIRE_FALSE(fs::path{"/file"}.is_relative());
         REQUIRE_FALSE(fs::path{"file"}.is_absolute());
         REQUIRE(fs::path{"file"}.is_relative());
      }
   }

   REQUIRE(fs::directory_iterator{tmpDir} == fs::directory_iterator{});
}
