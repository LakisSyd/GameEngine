#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <optional>

namespace fs = std::filesystem;

// ============================================================================
// HELPER: Конвертация пути в std::string (C++17 vs C++20 совместимость)
// ============================================================================
inline std::string ToString(const fs::path& path) {
    return path.string();
}

// ============================================================================
// STRUCT: FileEntry
// ============================================================================
struct FileEntry {
    std::string path;
    std::string name;
    std::string extension;
    uintmax_t size;
    bool isDirectory;

    FileEntry() : size(0), isDirectory(false) {}

    bool MatchesName(const std::string& query) const {
        auto toLower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
            };
        return toLower(name).find(toLower(query)) != std::string::npos;
    }
};

// ============================================================================
// CLASS: FileSystem
// ============================================================================
class FileSystem {
public:
    explicit FileSystem(const fs::path& rootPath) {
        if (rootPath.empty()) {
            std::cerr << "FileSystem: root path must not be empty\n";
            return;
        }

        std::error_code ec;
        m_rootPath = fs::absolute(rootPath, ec).lexically_normal();
        if (ec) {
            std::cerr << "FileSystem: invalid root path: " << ec.message() << "\n";
            m_rootPath.clear();
            return;
        }

        if (!fs::exists(m_rootPath, ec) || ec) {
            std::cout << "Creating root directory: "
                << ToString(m_rootPath) << "\n";
            ec.clear();
            fs::create_directories(m_rootPath, ec);
        }

        if (ec || !fs::is_directory(m_rootPath, ec)) {
            std::cerr << "FileSystem: root directory is unavailable: "
                << ToString(m_rootPath) << "\n";
            m_rootPath.clear();
            return;
        }

        fs::path canonicalRoot = fs::weakly_canonical(m_rootPath, ec);
        if (!ec)
            m_rootPath = std::move(canonicalRoot);
    }

    void ScanFileSystem() {
        m_files.clear();
        std::error_code ec;

        if (!fs::is_directory(m_rootPath, ec) || ec) {
            std::cerr << "Scan error: " << ToString(m_rootPath) << "\n";
            return;
        }

        auto options = fs::directory_options::skip_permission_denied;

        fs::recursive_directory_iterator it(m_rootPath, options, ec);
        const fs::recursive_directory_iterator end;
        while (it != end) {
            if (ec) {
                ec.clear();
                it.increment(ec);
                continue;
            }

            const auto& entry = *it;
            FileEntry fe;

            // ИСПОЛЬЗУЕМ НАШУ ФУНКЦИЮ ToString ВМЕСТО .u8string()
            fe.path = ToString(entry.path());
            fe.name = ToString(entry.path().filename());

            fe.extension = ToString(entry.path().extension());
            std::transform(fe.extension.begin(), fe.extension.end(),
                fe.extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            fe.isDirectory = entry.is_directory(ec);
            if (ec) {
                ec.clear();
                it.increment(ec);
                continue;
            }

            if (fe.isDirectory) {
                fe.size = 0;
            }
            else {
                fe.size = entry.file_size(ec);
                if (ec) {
                    fe.size = 0;
                    ec.clear();
                }
            }

            m_files.push_back(std::move(fe));
            it.increment(ec);
        }

        std::cout << "FileSystem: Scanned " << m_files.size() << " entries\n";
    }

    bool DeleteFile(const fs::path& relativePath) {
        const auto resolvedPath = ResolvePath(relativePath, false);
        if (!resolvedPath) return false;
        const fs::path& fullPath = *resolvedPath;
        std::error_code ec;

        if (!fs::is_regular_file(fullPath, ec) || ec) {
            std::cerr << "Delete warning: " << ToString(relativePath) << "\n";
            RemoveFromCache(ToString(fullPath));
            return false;
        }

        bool removed = fs::remove(fullPath, ec);

        if (ec) {
            std::cerr << "Delete error [" << ToString(relativePath) // Исправлено
                << "]: " << ec.message() << "\n";
            return false;
        }

        if (!removed) {
            std::cerr << "Delete failed: " << ToString(relativePath) << "\n";
            return false;
        }

        RemoveFromCache(ToString(fullPath));
        std::cout << "Deleted: " << ToString(relativePath) << "\n";
        return true;
    }

    bool DeleteDirectory(const fs::path& relativePath) {
        const auto resolvedPath = ResolvePath(relativePath, false);
        if (!resolvedPath) return false;
        const fs::path& fullPath = *resolvedPath;
        std::error_code ec;

        if (!fs::is_directory(fullPath, ec) || ec) {
            std::cerr << "DeleteDir error: " << ToString(relativePath) << "\n";
            RemoveFromCache(ToString(fullPath));
            return false;
        }

        uintmax_t count = fs::remove_all(fullPath, ec);

        if (ec) {
            std::cerr << "DeleteDir error [" << ToString(relativePath) << "]: "
                << ec.message() << "\n";
            return false;
        }

        std::string pathStr = ToString(fullPath);
        m_files.erase(
            std::remove_if(m_files.begin(), m_files.end(),
                [&pathStr](const FileEntry& entry) {
                    return entry.path == pathStr ||
                        entry.path.find(pathStr + "/") != std::string::npos ||
                        entry.path.find(pathStr + "\\") != std::string::npos;
                }),
            m_files.end()
        );

        std::cout << "Deleted directory (" << count << " items)\n";
        return true;
    }

    const FileEntry* FindByPath(const std::string& fullPath) const {
        for (const auto& entry : m_files) {
            if (entry.path == fullPath) {
                return &entry;
            }
        }
        return nullptr;
    }

    std::vector<const FileEntry*> FindByName(const std::string& query) const {
        std::vector<const FileEntry*> results;
        for (const auto& entry : m_files) {
            if (entry.MatchesName(query)) {
                results.push_back(&entry);
            }
        }
        return results;
    }

    std::vector<const FileEntry*> GetFilesByExtension(const std::string& ext) const {
        std::vector<const FileEntry*> results;
        std::string normalizedExt = ext;
        if (!normalizedExt.empty() && normalizedExt[0] != '.') {
            normalizedExt = "." + normalizedExt;
        }
        std::transform(normalizedExt.begin(), normalizedExt.end(),
            normalizedExt.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        for (const auto& entry : m_files) {
            if (!entry.isDirectory && entry.extension == normalizedExt) {
                results.push_back(&entry);
            }
        }
        return results;
    }

    std::vector<const FileEntry*> GetEntriesInDirectory(const fs::path& relativeDir) const {
        std::vector<const FileEntry*> results;

        const auto resolvedPath = ResolvePath(relativeDir, true);
        if (!resolvedPath) return results;
        const fs::path& targetPath = *resolvedPath;
        std::string targetStr = ToString(targetPath);

        // ✅ FIX: Нормализуем слэши в targetStr
        std::replace(targetStr.begin(), targetStr.end(), '\\', '/');
        if (!targetStr.empty() && targetStr.back() == '/') {
            targetStr.pop_back();
        }

        // ✅ DEBUG: Выводим targetStr для проверки


        for (const auto& entry : m_files) {
            std::string entryPath = entry.path;
            std::replace(entryPath.begin(), entryPath.end(), '\\', '/');

            size_t lastSlash = entryPath.find_last_of('/');
            if (lastSlash == std::string::npos) continue;

            std::string parentPath = entryPath.substr(0, lastSlash);

            // ✅ DEBUG: Сравниваем пути


            if (parentPath == targetStr) {
                results.push_back(&entry);
            }
        }

        return results;
    }

    const std::vector<FileEntry>& GetAllFiles() const { return m_files; }
    const fs::path& GetRootPath() const { return m_rootPath; }
    void Refresh() { ScanFileSystem(); }

    // Filtered entries: combines directory listing + name query + extension filter
    std::vector<const FileEntry*> GetFilteredEntries(
        const std::string& relativeDir,
        const std::string& nameQuery,
        const std::string& extensionFilter) const
    {
        auto entries = GetEntriesInDirectory(relativeDir);

        std::vector<const FileEntry*> result;
        result.reserve(entries.size());

        for (const auto* entry : entries)
        {
            // Filter by name query
            if (!nameQuery.empty() && !entry->MatchesName(nameQuery))
                continue;

            // Filter by extension
            if (!extensionFilter.empty() && !entry->isDirectory && entry->extension != extensionFilter)
                continue;

            result.push_back(entry);
        }
        return result;
    }

    bool CreateDirectory(const fs::path& relativePath) {
        const auto resolvedPath = ResolvePath(relativePath, false);
        if (!resolvedPath) return false;
        const fs::path& fullPath = *resolvedPath;
        std::error_code ec;

        bool created = fs::create_directories(fullPath, ec);
        bool exists  = fs::exists(fullPath, ec);

        if (!created && !exists) return false;

        std::string pathStr = ToString(fullPath);

        // ?? FIX: предотвращаем дубль в кэше — если уже есть, не добавляем
        bool alreadyCached = false;
        for (const auto& f : m_files) {
            if (f.path == pathStr) { alreadyCached = true; break; }
        }

        if (!alreadyCached) {
            FileEntry fe;
            fe.path        = pathStr;
            fe.name        = ToString(fullPath.filename());
            fe.extension   = "";   // ?? FIX: расширение у директории всегда пустое (ранее не заполнялось)
            fe.isDirectory = true;
            fe.size        = 0;
            m_files.push_back(fe);
        }

        return true;
    }

private:
    static bool PathComponentEquals(const fs::path& left, const fs::path& right) {
#ifdef _WIN32
        std::wstring leftText = left.wstring();
        std::wstring rightText = right.wstring();
        std::transform(leftText.begin(), leftText.end(), leftText.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        std::transform(rightText.begin(), rightText.end(), rightText.begin(),
            [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        return leftText == rightText;
#else
        return left == right;
#endif
    }

    bool IsWithinRoot(const fs::path& candidate, bool allowRoot) const {
        auto rootPart = m_rootPath.begin();
        auto candidatePart = candidate.begin();

        for (; rootPart != m_rootPath.end(); ++rootPart, ++candidatePart) {
            if (candidatePart == candidate.end()
                || !PathComponentEquals(*rootPart, *candidatePart)) {
                return false;
            }
        }

        return allowRoot || candidatePart != candidate.end();
    }

    std::optional<fs::path> ResolvePath(
        const fs::path& relativePath,
        bool allowRoot) const
    {
        if (m_rootPath.empty() || relativePath.is_absolute()
            || relativePath.has_root_name() || relativePath.has_root_directory()) {
            std::cerr << "FileSystem: rejected path outside project root: "
                << ToString(relativePath) << "\n";
            return std::nullopt;
        }

        for (const auto& part : relativePath) {
            if (part == "..") {
                std::cerr << "FileSystem: rejected parent traversal: "
                    << ToString(relativePath) << "\n";
                return std::nullopt;
            }
        }

        const fs::path candidate = (m_rootPath / relativePath).lexically_normal();
        std::error_code ec;
        const fs::path canonicalCandidate = fs::weakly_canonical(candidate, ec);
        if (ec || !IsWithinRoot(canonicalCandidate, allowRoot)) {
            std::cerr << "FileSystem: rejected path outside project root: "
                << ToString(relativePath) << "\n";
            return std::nullopt;
        }

        return candidate;
    }

    void RemoveFromCache(const std::string& fullPath) {
        std::string normalizedPath = fullPath;
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');

        auto it = std::remove_if(m_files.begin(), m_files.end(),
            [&normalizedPath](const FileEntry& entry) {
                std::string entryPath = entry.path;
                std::replace(entryPath.begin(), entryPath.end(), '\\', '/');
                return entryPath == normalizedPath;
            });

        if (it != m_files.end()) {
            m_files.erase(it, m_files.end());
        }
    }

    std::vector<FileEntry> m_files;
    fs::path m_rootPath;
};

