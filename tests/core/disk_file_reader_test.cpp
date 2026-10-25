#include <catch2/catch_test_macros.hpp>

#include <inertia/disk_file_reader.hpp>
#include <inertia/ports/file_reader.hpp>
#include <inertia/vite.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

using namespace inertia;

namespace {

// A directory that is removed with the object.
class TempDir {
  public:
    TempDir() {
        static std::atomic<int> counter{0};
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("inertia-disk-reader-" + std::to_string(unique) + "-" + std::to_string(counter++));
        std::filesystem::create_directories(path_);
    }
    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;
    TempDir(TempDir &&) = delete;
    TempDir &operator=(TempDir &&) = delete;
    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

void write(const std::filesystem::path &file, const std::string &content) {
    std::ofstream out{file, std::ios::binary | std::ios::trunc};
    out << content;
}

} // namespace

TEST_CASE("DiskFileReader reads a file and reports a missing one as nullopt", "[disk]") {
    const TempDir dir;
    const DiskFileReader reader;
    write(dir.path() / "a.txt", "hello\n");

    CHECK(reader.read(dir.path() / "a.txt") == "hello\n");
    CHECK_FALSE(reader.read(dir.path() / "missing.txt").has_value());
    CHECK_FALSE(reader.read(dir.path()).has_value()); // a directory is not a file
}

TEST_CASE("DiskFileReader reads an empty file as an empty string", "[disk]") {
    const TempDir dir;
    write(dir.path() / "empty", "");
    CHECK(DiskFileReader{}.read(dir.path() / "empty") == "");
}

TEST_CASE("DiskFileReader stamps are stable until the file changes size", "[disk]") {
    const TempDir dir;
    const DiskFileReader reader;
    const auto file = dir.path() / "manifest.json";
    write(file, "{}");

    REQUIRE(reader.stamp(file).has_value());
    const FileStamp first = reader.stamp(file).value_or(FileStamp{});
    CHECK(first.size == 2);
    CHECK(reader.stamp(file) == first);

    write(file, R"({"a": 1})");
    REQUIRE(reader.stamp(file).has_value());
    const FileStamp second = reader.stamp(file).value_or(FileStamp{});
    CHECK(second != first);
    CHECK(second.size == 8);
}

TEST_CASE("DiskFileReader has no stamp for a missing file or a directory", "[disk]") {
    const TempDir dir;
    const DiskFileReader reader;
    CHECK_FALSE(reader.stamp(dir.path() / "missing").has_value());
    CHECK_FALSE(reader.stamp(dir.path()).has_value());
}

TEST_CASE("Vite over a DiskFileReader follows the manifest on disk", "[disk][vite][cache]") {
    const TempDir dir;
    ViteConfig cfg;
    cfg.publicDir = dir.path();
    std::filesystem::create_directories(dir.path() / "build" / ".vite");
    const auto manifest = dir.path() / "build" / ".vite" / "manifest.json";
    const Vite vite(cfg, std::make_shared<DiskFileReader>());

    CHECK(vite.manifestHash().empty());
    CHECK(vite.tags().empty());

    write(manifest, R"({"src/app.tsx": {"file": "assets/app-1.js"}})");
    const auto first = vite.manifestHash();
    CHECK(first.size() == 16);
    CHECK(vite.tags() == R"V(<script type="module" src="/build/assets/app-1.js"></script>)V");
    CHECK(vite.manifestHash() == first);

    write(manifest, R"({"src/app.tsx": {"file": "assets/app-22.js"}})");
    CHECK(vite.manifestHash() != first);
    CHECK(vite.tags() == R"V(<script type="module" src="/build/assets/app-22.js"></script>)V");
}
