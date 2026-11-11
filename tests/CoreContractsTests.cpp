#include "FileSystem.h"
#include "JobSystem.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    class TemporaryDirectory
    {
    public:
        TemporaryDirectory()
        {
            const auto id = std::chrono::steady_clock::now()
                .time_since_epoch().count();
            m_path = fs::temp_directory_path()
                / ("dx_framework_contracts_" + std::to_string(id));
            if (!fs::create_directories(m_path))
                throw std::runtime_error("Could not create temporary test directory");
        }

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(m_path, ignored);
        }

        const fs::path& Path() const { return m_path; }

    private:
        fs::path m_path;
    };

    int failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition)
        {
            std::cout << "[PASS] " << message << '\n';
            return;
        }

        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    }

    void WriteTextFile(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path, std::ios::binary);
        if (!output)
            throw std::runtime_error("Could not create test file");
        output << text;
    }
}

int main()
{
    try
    {
        TemporaryDirectory temporaryDirectory;
        const fs::path sandbox = temporaryDirectory.Path();
        const fs::path root = sandbox / "root";
        const fs::path outside = sandbox / "outside";
        fs::create_directories(outside);

        FileSystem invalidFileSystem(fs::path{});
        Expect(invalidFileSystem.GetRootPath().empty(),
            "an empty root is rejected instead of resolving to the working directory");
        Expect(!invalidFileSystem.CreateDirectory("unexpected"),
            "operations fail when the root is invalid");

        FileSystem fileSystem(root);
        Expect(fs::is_directory(fileSystem.GetRootPath()),
            "constructor creates and canonicalizes the root directory");

        Expect(fileSystem.CreateDirectory("textures/ui"),
            "directory creation works inside the root");
        Expect(fs::is_directory(root / "textures" / "ui"),
            "created directory stays inside the root");

        WriteTextFile(root / "textures" / "ui" / "Button.TXT", "button");
        WriteTextFile(outside / "keep.txt", "outside");
        fileSystem.ScanFileSystem();

        Expect(fileSystem.GetFilesByExtension("txt").size() == 1,
            "extension lookup is case-insensitive");
        Expect(fileSystem.GetEntriesInDirectory("textures/ui").size() == 1,
            "directory listing returns direct children only");

        Expect(!fileSystem.CreateDirectory("../escaped"),
            "parent traversal is rejected for directory creation");
        Expect(!fileSystem.CreateDirectory(outside / "absolute"),
            "absolute paths are rejected for directory creation");
        Expect(!fileSystem.DeleteFile("../outside/keep.txt"),
            "parent traversal is rejected for file deletion");
        Expect(!fileSystem.DeleteFile(outside / "keep.txt"),
            "absolute paths are rejected for file deletion");
        Expect(!fileSystem.DeleteDirectory(outside),
            "absolute paths are rejected for recursive deletion");
        Expect(!fileSystem.DeleteDirectory("."),
            "the project root itself cannot be deleted");
        Expect(fs::exists(outside / "keep.txt"),
            "rejected operations leave outside data untouched");

        std::error_code symlinkError;
        fs::create_directory_symlink(outside, root / "outside_link", symlinkError);
        if (!symlinkError)
        {
            Expect(!fileSystem.DeleteFile("outside_link/keep.txt"),
                "a symlink cannot escape the project root");
            Expect(!fileSystem.DeleteDirectory("outside_link"),
                "an outside directory symlink cannot be recursively deleted");
            Expect(!fileSystem.CreateDirectory("outside_link/escaped"),
                "directory creation cannot escape through a symlink");
            Expect(fs::exists(outside / "keep.txt"),
                "symlink escape rejection preserves the target");
        }
        else
        {
            std::cout << "[SKIP] symlink escape test (OS privilege unavailable)\n";
        }

        Expect(fileSystem.DeleteFile("textures/ui/Button.TXT"),
            "file deletion works inside the root");
        Expect(fileSystem.DeleteDirectory("textures"),
            "recursive directory deletion works inside the root");
        Expect(!fs::exists(root / "textures"),
            "deleted in-root directory is gone");

        JobSystem jobSystem;
        std::atomic<int> completedJobs = 0;
        Expect(!jobSystem.SubmitJob([&completedJobs] { ++completedJobs; }),
            "a job system rejects work before initialization");
        Expect(!jobSystem.Initialize(0),
            "a job system rejects a non-positive worker count");
        Expect(jobSystem.Initialize(2),
            "a job system starts worker threads");

        bool acceptedAllJobs = true;
        for (int jobIndex = 0; jobIndex < 8; ++jobIndex)
        {
            acceptedAllJobs = jobSystem.SubmitJob(
                [&completedJobs] { ++completedJobs; }) && acceptedAllJobs;
        }
        Expect(acceptedAllJobs,
            "a running job system accepts work");
        jobSystem.Wait();
        Expect(completedJobs.load() == 8,
            "worker threads complete all queued work");

        jobSystem.Shutdown();
        Expect(!jobSystem.SubmitJob([&completedJobs] { ++completedJobs; }),
            "a shut-down job system rejects new work");
        Expect(jobSystem.Initialize(2),
            "a job system can be initialized again after shutdown");
        Expect(jobSystem.SubmitJob([&completedJobs] { ++completedJobs; }),
            "a reinitialized job system accepts work");
        jobSystem.Wait();
        Expect(completedJobs.load() == 9,
            "a reinitialized job system executes work");
        jobSystem.Shutdown();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[FATAL] " << exception.what() << '\n';
        return 2;
    }

    if (failures != 0)
    {
        std::cerr << failures << " contract test(s) failed.\n";
        return 1;
    }

    std::cout << "All core contract tests passed.\n";
    return 0;
}
