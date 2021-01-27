/*
** FIXME: This file should contain no symbols on any platform
**
** There are a few symbols which on Linux currently get linked
** for a very difficult to determine reason. We should fix this,
** but for the meantime lets add this file to patch the link
** errors
*/
#if LINUX || MAC
namespace VSTGUI
{
void doAssert(const char *, const char *, const char *) {}
} // namespace VSTGUI
#endif
