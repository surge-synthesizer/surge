#include "guihelpers.h"

#include <cctype>

std::string Surge::UI::toOSCaseForMenu(std::string menuName)
{
#if WINDOWS
   for (auto i = 1; i < menuName.length() - 1; ++i)
      if (!(isupper(menuName[i]) && (isupper(menuName[i + 1]) || !isalpha(menuName[i + 1]))))
      {
         menuName[i] = std::tolower(menuName[i]);
      }
#endif
   return menuName;
}
