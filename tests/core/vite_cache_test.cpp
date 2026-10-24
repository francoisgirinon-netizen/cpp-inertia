#include <catch2/catch_test_macros.hpp>

#include <inertia/ports/file_reader.hpp>
#include <inertia/vite.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace inertia;

namespace {

constexpr std::string_view kManifest = R"({
  "src/app.tsx": {"file": "assets/app-1.js", "css": ["assets/app-1.css"], "imports": ["_vendor.js"]},
  "_vendor.js": {"file": "assets/vendor-2.js", "css": ["assets/v.css"]}
})";

constexpr std::string_view kOtherManifest = R"({"src/app.tsx": {"file": "assets/app-9.js"}})";

constexpr std::string_view kOtherTags =
    R"V(<script type="module" src="/build/assets/app-9.js"></script>)V";

// A reader over one manifest that counts its reads. With `stamped` it reports a stamp that changes
// with every call of set(), like a file's modification time.
class CountingReader final : public FileReader {
  public:
    explicit CountingReader(bool stamped) : stamped_(stamped) {}

    void set(std::string content) {
        const std::scoped_lock lock(mutex_);
        content_ = std::move(content);
        ++generation_;
    }
    void remove() {
        const std::scoped_lock lock(mutex_);
        content_.reset();
        ++generation_;
    }
    [[nodiscard]] int reads() const { return reads_; }

    [[nodiscard]] std::optional<std::string>
    read(const std::filesystem::path &path) const override {
        if (path.filename() != "manifest.json") {
            return std::nullopt; // the hot file
        }
        const std::scoped_lock lock(mutex_);
        ++reads_;
        return content_;
    }
    [[nodiscard]] std::optional<FileStamp> stamp(const std::filesystem::path &path) const override {
        const std::scoped_lock lock(mutex_);
        if (!stamped_ || path.filename() != "manifest.json" || !content_) {
            return std::nullopt;
        }
        return FileStamp{.size = content_->size(), .modified = generation_};
    }

  private:
    bool stamped_;
    mutable std::mutex mutex_;
    std::optional<std::string> content_;
    std::int64_t generation_ = 0;
    mutable std::atomic<int> reads_ = 0;
};

} // namespace

TEST_CASE("a stamped manifest is read once while its stamp does not change", "[vite][cache]") {
    auto files = std::make_shared<CountingReader>(true);
    files->set(std::string(kManifest));
    const Vite vite(ViteConfig{}, files);

    const auto hash = vite.manifestHash();
    const auto tags = vite.tags();
    REQUIRE(files->reads() == 1);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(vite.manifestHash() == hash);
        REQUIRE(vite.tags() == tags);
    }
    REQUIRE(files->reads() == 1);
    REQUIRE(hash.size() == 16);
    REQUIRE_FALSE(tags.empty());
}

TEST_CASE("a changed stamp reloads the manifest hash and tags", "[vite][cache]") {
    auto files = std::make_shared<CountingReader>(true);
    files->set(std::string(kManifest));
    const Vite vite(ViteConfig{}, files);
    const auto hash = vite.manifestHash();
    const auto tags = vite.tags();

    files->set(std::string(kOtherManifest));
    REQUIRE(vite.manifestHash() != hash);
    REQUIRE(vite.manifestHash() == Vite(ViteConfig{}, files).manifestHash());
    REQUIRE(vite.tags() == kOtherTags);
    REQUIRE(vite.tags() != tags);
}

TEST_CASE("a reader without stamps is read on every call but its content is parsed once",
          "[vite][cache]") {
    auto files = std::make_shared<CountingReader>(false);
    files->set(std::string(kManifest));
    const Vite vite(ViteConfig{}, files);

    const auto hash = vite.manifestHash();
    REQUIRE(vite.manifestHash() == hash);
    REQUIRE(files->reads() == 2); // no stamp to trust: the content is compared instead

    files->set(std::string(kOtherManifest));
    REQUIRE(vite.manifestHash() != hash);
    REQUIRE(vite.tags() == kOtherTags);
}

TEST_CASE("a manifest that disappears or turns malformed is not served from the cache",
          "[vite][cache]") {
    for (const bool stamped : {true, false}) {
        auto files = std::make_shared<CountingReader>(stamped);
        files->set(std::string(kManifest));
        const Vite vite(ViteConfig{}, files);
        REQUIRE_FALSE(vite.manifestHash().empty());

        files->remove();
        REQUIRE(vite.manifestHash().empty());
        REQUIRE(vite.tags().empty());

        files->set(R"({"src/app.tsx": )");
        REQUIRE(vite.manifestHash().empty());
        REQUIRE(vite.tags().empty());

        files->set(std::string(kManifest));
        REQUIRE_FALSE(vite.manifestHash().empty());
        REQUIRE_FALSE(vite.tags().empty());
    }
}

TEST_CASE("the manifest cache is safe to use from several threads while the file changes",
          "[vite][cache]") {
    auto files = std::make_shared<CountingReader>(true);
    files->set(std::string(kManifest));
    const Vite vite(ViteConfig{}, files);
    const auto hashA = vite.manifestHash();
    const auto tagsA = vite.tags();
    files->set(std::string(kOtherManifest));
    const auto hashB = vite.manifestHash();
    const auto tagsB = vite.tags();
    REQUIRE(hashA != hashB);

    std::atomic<bool> stop = false;
    std::atomic<int> bad = 0;
    std::vector<std::thread> readers;
    readers.reserve(4);
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop) {
                // Hash and tags are read separately, so each must be one of the two states.
                const auto hash = vite.manifestHash();
                const auto tags = vite.tags();
                if ((hash != hashA && hash != hashB) || (tags != tagsA && tags != tagsB)) {
                    ++bad;
                }
            }
        });
    }
    for (int i = 0; i < 200; ++i) {
        files->set(std::string(i % 2 == 0 ? kManifest : kOtherManifest));
    }
    stop = true;
    for (auto &reader : readers) {
        reader.join();
    }
    REQUIRE(bad == 0);
}
