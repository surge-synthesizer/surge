#include <windows.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <iostream>
#include <sstream>

#include "UserInteractions.h"
#include "SurgeGUIEditor.h"

namespace Surge
{

namespace UserInteractions
{

void promptError(const std::string &message, const std::string &title,
                 SurgeGUIEditor *guiEditor)
{
    MessageBox(::GetActiveWindow(),
               message.c_str(),
               title.c_str(),
               MB_OK | MB_ICONERROR );
}

void promptError(const Surge::Error &error, SurgeGUIEditor *guiEditor)
{
    promptError(error.getMessage(), error.getTitle());
}

void promptInfo(const std::string &message, const std::string &title,
                SurgeGUIEditor *guiEditor)
{
    MessageBox(::GetActiveWindow(),
               message.c_str(),
               title.c_str(),
               MB_OK | MB_ICONINFORMATION );
}


MessageResult promptOKCancel(const std::string &message, const std::string &title,
                             SurgeGUIEditor *guiEditor)
{
    if (MessageBox(::GetActiveWindow(),
                   message.c_str(),
                   title.c_str(),
                   MB_YESNO | MB_ICONQUESTION) != IDYES)
        return UserInteractions::CANCEL;

    return UserInteractions::OK;
}

void openURL(const std::string &url)
{
    ShellExecute(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void showHTML( const std::string &html )
{
    TCHAR lpTempPathBuffer[MAX_PATH];

    auto dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer);
    std::ostringstream fns;
    fns << lpTempPathBuffer << "surge-tuning." << rand() << ".html";

    FILE *f = fopen(fns.str().c_str(), "w" );
    if( f )
    {
        fprintf( f, "%s", html.c_str());
        fclose(f);
        std::string url = std::string("file:///") + fns.str();
        openURL(url);
    }
}

void openFolderInFileBrowser(const std::string& folder)
{
   std::string url = "file://" + folder;
   UserInteractions::openURL(url);
}

// thanks https://stackoverflow.com/questions/12034943/win32-select-directory-dialog-from-c-c  
static int CALLBACK BrowseCallbackProc(HWND hwnd,UINT uMsg, LPARAM lParam, LPARAM lpData)
{

    if(uMsg == BFFM_INITIALIZED)
    {
        std::string tmp = (const char *) lpData;
        std::cout << "path: " << tmp << std::endl;
        SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

void BrowseFolder(std::string saved_path, std::function<void(std::string)> cb)
{
    TCHAR path[MAX_PATH];

    const char * path_param = saved_path.c_str();

    BROWSEINFO bi = { 0 };
    bi.lpszTitle  = ("Browse for folder...");
    bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn       = BrowseCallbackProc;
    bi.lParam     = (LPARAM) path_param;

    LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );

    if ( pidl != 0 )
    {
        //get the name of the folder and put it in path
        SHGetPathFromIDList ( pidl, path );

        //free memory used
        IMalloc * imalloc = 0;
        if ( SUCCEEDED( SHGetMalloc ( &imalloc )) )
        {
            imalloc->Free ( pidl );
            imalloc->Release ( );
        }

        cb( path );
    }
}


  
void promptFileOpenDialog(const std::string& initialDirectory,
                          const std::string& filterSuffix,
                          std::function<void(std::string)> callbackOnOpen,
                          bool canSelectDirectories,
                          bool canCreateDirectories,
                          SurgeGUIEditor* guiEditor)
{
  if( canSelectDirectories )
    {
      BrowseFolder( initialDirectory, callbackOnOpen );
      return;
    }
   // With many thanks to
   // https://www.daniweb.com/programming/software-development/code/217307/a-simple-getopenfilename-example
   char szFile[1024];
   OPENFILENAME ofn;
   ZeroMemory(&ofn, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = NULL;
   ofn.lpstrFile = szFile;
   ofn.lpstrFile[0] = '\0';
   ofn.nMaxFile = sizeof(szFile);
   ofn.lpstrFilter = "All\0*.*\0";
   ofn.nFilterIndex = 0;
   ofn.lpstrFileTitle = NULL;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = NULL;
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

   if (GetOpenFileName(&ofn))
   {
      callbackOnOpen(ofn.lpstrFile);
   }
}
};

};

