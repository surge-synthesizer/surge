// SPDX-License-Identifier: GPL-3.0-or-later
// Included inside the JUCE gui_basics translation unit.
#include <map>
namespace juce
{
class FileChooser::Native final : public FileChooser::Pimpl,
                                 public std::enable_shared_from_this<FileChooser::Native>
{
  public:
    Native(FileChooser &chooser, int flags) : owner(chooser), flags(flags), token(++nextToken) {}
    ~Native() override { pending.erase(token); }
    void launch() override
    {
        pending[token] = shared_from_this();
        const bool save = (flags & FileBrowserComponent::saveMode) != 0;
        const bool directory = (flags & FileBrowserComponent::canSelectDirectories) != 0 &&
                               (flags & FileBrowserComponent::canSelectFiles) == 0;
        const bool multiple = (flags & FileBrowserComponent::canSelectMultipleItems) != 0;
        // clang-format off
        EM_ASM({ SurgeBrowser.pickFiles($0, UTF8ToString($1), UTF8ToString($2), UTF8ToString($3), !!$4, !!$5, !!$6); },
               token, owner.title.toRawUTF8(), owner.filters.toRawUTF8(),
               owner.startingFile.getFullPathName().toRawUTF8(), save, directory, multiple);
        // clang-format on
    }
    void runModally() override { jassertfalse; }
    static int complete(int token, const char *json)
    {
        const auto found = pending.find(token);
        if (found == pending.end()) return 0;
        auto selection = found->second.lock();
        pending.erase(found);
        if (!selection) return 0;
        Array<URL> urls;
        auto parsed = JSON::parse(String::fromUTF8(json));
        if (auto *paths = parsed.getArray())
            for (const auto &path : *paths) urls.add(URL(File(path.toString())));
        selection->owner.finished(urls);
        return 1;
    }
  private:
    FileChooser &owner;
    int flags, token;
    inline static int nextToken = 0;
    inline static std::map<int, std::weak_ptr<Native>> pending;
};
bool FileChooser::isPlatformDialogAvailable() { return true; }
std::shared_ptr<FileChooser::Pimpl> FileChooser::showPlatformDialog(FileChooser &owner, int flags,
                                                                  FilePreviewComponent *)
{
    return std::make_shared<Native>(owner, flags);
}
extern "C" EMSCRIPTEN_KEEPALIVE int surge_file_dialog_complete(int token, const char *json)
{
    return FileChooser::Native::complete(token, json);
}
}
