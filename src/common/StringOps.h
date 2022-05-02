#pragma once

#include <string>

/*
 * There are a number of places in Surge where raw C strings are copied around.
 * Ideally these would be replaced with something safer, but for the time being, a replacement
 * for dangerous strcpy operations at least was needed.
 *
 * strncpy is no good - the destination buffer can end up being non-NULL-terminated, and it also
 * writes zeros the length of the whole buffer for no reason.
 * bsd strlcpy is also no good, it performs a length check on the source string for no reason.
 *
 * So, here's strxcpy:
 * a function that copies a string to dst from src
 * while ensuring that dst does not exceed n bytes
 *
 * that's it. it returns nothing.
 * that suits Surge's purposes fine, as we don't care if a string gets truncated, only that we
 * don't crash.
 *
 * the current implementation just uses snprintf.
 */

static inline void strxcpy(char *const dst, const char *const src, const size_t n)
{
    snprintf(dst, n, "%s", src);
}

/*
 * There are also a whole mess of 'txt' buffers scattered through the code - sometimes different
 * lengths. For now, standardize on 256 bytes to get rid of the magic constants.
 *
 * If something breaks - check the TXT_SIZE constants versus what they were in earlier revisions!
 *
 * In some cases, buffers that were not called txt but served a similar purpose and were also 256
 * bytes now also use this constant.
 */
#define TXT_SIZE 256

namespace Surge
{
namespace GUI
{
/*
 * Used to convert letter case for menu entries
 * Input string should be macOS style (Menu Entry is Like This)
 * Output is string in Windows style (Menu entry is like this), or unmodified if we're on Mac
 */
inline std::string toOSCase(const std::string &iMenuName)
{
#if WINDOWS
    auto menuName = iMenuName;

    for (auto i = 1; i < menuName.length() - 1; ++i)
    {
        if (!(isupper(menuName[i]) && (isupper(menuName[i + 1]) || !isalpha(menuName[i + 1]))))
        {
            menuName[i] = std::tolower(menuName[i]);
        }
    }

    return menuName;
#else
    return iMenuName;
#endif
}
} // namespace GUI
} // namespace Surge