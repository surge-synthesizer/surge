// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Browser archives stage on the same IDBFS mount as their destinations. Rename
// commits preserve original file objects and allow rollback without allocating
// another copy when a later write fails. No browser persistence callback can run
// in the middle of this synchronous message-thread transaction.
class BrowserArchiveImport
{
    fs::path root;
    std::map<fs::path, fs::path> roots;
    struct Change { fs::path source, target, backup; bool saved{}, installed{}; };
    std::vector<Change> changes;
    std::vector<fs::path> directories;
    bool preserveRecovery{};

    void makeParents(const fs::path &path)
    {
        if (fs::is_directory(path)) return;
        if (fs::exists(path)) throw std::runtime_error("Archive destination is not a directory");
        makeParents(path.parent_path());
        fs::create_directory(path);
        directories.push_back(path);
    }
public:
    explicit BrowserArchiveImport(const fs::path &user)
        : root(user / (".surge-archive-" + juce::Uuid().toString().toStdString()))
    {
        fs::create_directories(root / "files");
        fs::create_directory(root / "backups");
    }
    ~BrowserArchiveImport()
    {
        if (!preserveRecovery) { std::error_code error; fs::remove_all(root, error); }
    }
    fs::path stage(const fs::path &destination)
    {
        auto found = roots.find(destination);
        if (found != roots.end()) return found->second;
        auto path = root / "files" / std::to_string(roots.size());
        fs::create_directory(path);
        roots.emplace(destination, path);
        return path;
    }
    void commit()
    {
        // Inspect all entries before changing any destination files.
        for (const auto &[destination, staged] : roots)
            for (const auto &entry : fs::recursive_directory_iterator(staged))
            {
                if (entry.is_symlink()) throw std::runtime_error("Archive links are not supported");
                if (entry.is_directory()) continue;
                if (!entry.is_regular_file()) throw std::runtime_error("Unsupported archive entry");
                auto target = destination / entry.path().lexically_relative(staged);
                if (fs::is_symlink(target) || (fs::exists(target) && !fs::is_regular_file(target)))
                    throw std::runtime_error("Archive destination is not a regular file");
                changes.push_back({entry.path(), target, root / "backups" / std::to_string(changes.size())});
            }
        juce::Array<juce::var> recovery;
        for (const auto &change : changes)
        {
            juce::DynamicObject::Ptr file = new juce::DynamicObject();
            file->setProperty("target", juce::String(path_to_string(change.target)));
            file->setProperty("backup", juce::String(path_to_string(change.backup)));
            recovery.add(juce::var(file.get()));
        }
        if (!juce::File(path_to_string(root / "recovery.json"))
                 .replaceWithText(juce::JSON::toString(juce::var(recovery))))
            throw std::runtime_error("Unable to write archive recovery manifest");
        try
        {
            for (auto &change : changes)
            {
                makeParents(change.target.parent_path());
                if (fs::exists(change.target))
                {
                    fs::rename(change.target, change.backup);
                    change.saved = true;
                }
                fs::rename(change.source, change.target);
                change.installed = true;
            }
        }
        catch (...)
        {
            bool failed = false;
            for (auto it = changes.rbegin(); it != changes.rend(); ++it)
            {
                std::error_code error;
                if (it->installed) { fs::remove(it->target, error); failed |= bool(error); }
                if (it->saved) { error.clear(); fs::rename(it->backup, it->target, error); failed |= bool(error); }
            }
            for (auto it = directories.rbegin(); it != directories.rend(); ++it)
            { std::error_code error; fs::remove(*it, error); }
            if (failed)
            {
                preserveRecovery = true;
                throw std::runtime_error("Archive rollback could not finish. Original files are retained in " + root.u8string());
            }
            throw;
        }
    }
};
