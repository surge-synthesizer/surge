#include "UserInteractions.h"
#include "SurgeError.h"

#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <fstream>
#include <sstream>
#include <system_error>

namespace
{

std::string wideToUtf8(const std::wstring& wide)
{
   if (wide.empty())
      return std::string();

   const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), wide.size(),
                                         nullptr, 0, nullptr, nullptr);
   if (size)
   {
      std::string utf8;
      utf8.resize(size);
      if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), wide.size(), &utf8[0],
                              utf8.size(), nullptr, nullptr))
         return utf8;
   }
   throw std::system_error(GetLastError(), std::system_category());
}

std::wstring utf8ToWide(const std::string& utf8)
{
   if (utf8.empty())
      return std::wstring();

   const auto size =
       MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), utf8.size(), nullptr, 0);
   if (size)
   {
      std::wstring wide;
      wide.resize(size);
      if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), utf8.size(), &wide[0],
                              wide.size()))
         return wide;
   }
   throw std::system_error(GetLastError(), std::system_category());
}

void browseFolder(const std::string& initialDirectory,
                  const std::function<void(std::string)>& callbackOnOpen)
{
   struct BrowseCallback
   {
      static int CALLBACK proc(HWND hwnd, UINT uMsg, LPARAM, LPARAM lpData)
      {
         if (uMsg == BFFM_INITIALIZED)
            SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
         return 0;
      }
   };

   auto path_param = utf8ToWide(initialDirectory);

   BROWSEINFO bi{};
   bi.lpszTitle = L"Browse for folder...";
   bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
   bi.lpfn = BrowseCallback::proc;
   bi.lParam = LPARAM(path_param.c_str());

   if (auto pidl = SHBrowseForFolder(&bi))
   {
      WCHAR path[MAX_PATH];
      SHGetPathFromIDList(&*pidl, path);
      CoTaskMemFree(pidl);
      callbackOnOpen(wideToUtf8(path));
   }
}

} // anonymous namespace

namespace Surge::UserInteractions
{

void promptError(const std::string &message, const std::string &title,
                 SurgeGUIEditor *guiEditor)
{
   MessageBox(::GetActiveWindow(), utf8ToWide(message).c_str(), utf8ToWide(title).c_str(),
              MB_OK | MB_ICONERROR);
}

void promptError(const Surge::Error &error, SurgeGUIEditor *guiEditor)
{
    promptError(error.getMessage(), error.getTitle());
}

void promptInfo(const std::string &message, const std::string &title,
                SurgeGUIEditor *guiEditor)
{
   MessageBox(::GetActiveWindow(), utf8ToWide(message).c_str(), utf8ToWide(title).c_str(),
              MB_OK | MB_ICONINFORMATION);
}

MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
   if (MessageBox(::GetActiveWindow(), utf8ToWide(message).c_str(), utf8ToWide(title).c_str(),
                  MB_YESNO | MB_ICONQUESTION) != IDYES)
      return UserInteractions::CANCEL;

   return UserInteractions::OK;
}

void openURL(const std::string &url)
{
   ShellExecute(nullptr, L"open", utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void showHTML( const std::string &html )
{
   WCHAR pathBuf[MAX_PATH];
   if (!GetTempPath(MAX_PATH, pathBuf))
      return;

   // Prepending file:// opens in the browser instead of the associated app for .html (which might
   // be unset)
   std::wostringstream fns;
   fns << L"file://" << pathBuf << L"surge-data." << rand() << L".html";

   auto fileName(fns.str());
   std::ofstream out(fileName.c_str() + 7);
   if (out << html << std::flush)
      ShellExecute(nullptr, L"open", fileName.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void openFolderInFileBrowser(const std::string& folder)
{
   openURL(folder);
}

void promptFileOpenDialog(const std::string& initialDirectory,
                          const std::string& filterSuffix,
                          const std::string& filterDescription,
                          std::function<void(std::string)> callbackOnOpen,
                          bool canSelectDirectories,
                          bool canCreateDirectories,
                          SurgeGUIEditor* guiEditor)
{
   if (canSelectDirectories)
   {
      browseFolder(initialDirectory, callbackOnOpen);
      return;
   }

   // With many thanks to
   // https://www.daniweb.com/programming/software-development/code/217307/a-simple-getopenfilename-example

   // this also helped!
   // https://stackoverflow.com/questions/34201213/c-lpstr-and-string-trouble-with-zero-terminated-strings
   std::wstring fullFilter;
   fullFilter.append(utf8ToWide(filterDescription));
   fullFilter.push_back(0);
   fullFilter.append(utf8ToWide("*" + filterSuffix));
   fullFilter.push_back(0);

   std::wstring initialDirectoryNative(utf8ToWide(initialDirectory));
   WCHAR szFile[1024];
   OPENFILENAME ofn = {};
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = nullptr;
   ofn.lpstrFile = szFile;
   ofn.lpstrFile[0] = 0;
   ofn.nMaxFile = std::size(szFile);
   ofn.lpstrFilter = fullFilter.c_str();
   ofn.nFilterIndex = 0;
   ofn.lpstrFileTitle = nullptr;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = initialDirectoryNative.c_str();
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

   if (GetOpenFileName(&ofn))
      callbackOnOpen(wideToUtf8(ofn.lpstrFile));
}

} // namespace Surge::UserInteractions
