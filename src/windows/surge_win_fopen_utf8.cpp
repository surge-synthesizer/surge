#include <cstdio>
#include <vector>

#include <windows.h>

namespace
{

std::vector<wchar_t> utf8ToWide(const char* s)
{
   std::vector<wchar_t> wide;
   if (auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0))
   {
      wide.resize(size);
      if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &wide[0], wide.size()))
         wide[0] = L'\0'; // let _wfopen() fail with invalid parameter
   }
   return wide;
}

} // anonymous namespace

FILE* surge_win_fopen_utf8(const char* pathname, const char* mode)
{
   const auto pathNameWide = utf8ToWide(pathname);
   const auto modeWide = utf8ToWide(mode);
   return _wfopen(pathNameWide.data(), modeWide.data());
}
