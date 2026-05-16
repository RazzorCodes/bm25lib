#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "bm25.hpp"
#include "store/sqlite.hpp"

// Performance smoke test for Milestone 1.
//
// Goal: query cost scales with postings hit set, not total corpus size.
// Pass condition: median query latency on a 10x larger corpus is at most 2x the
// median latency on the small corpus, when the queried terms appear in roughly
// the same number of documents in both corpora.

namespace
{
struct TempDb
{
    std::string path;
    explicit TempDb(const std::string &name)
        : path((std::filesystem::temp_directory_path() / name).string())
    {
    }
    ~TempDb() { (void)std::filesystem::remove(path); }
    TempDb(const TempDb &) = delete;
    TempDb &operator=(const TempDb &) = delete;
};

// Build a synthetic corpus of `totalDocs` documents.
// - `anchorDocs` of them contain `anchorTerm` (evenly spaced by index).
// - The remaining docs contain only filler words.
void BuildCorpus(bm25::Bm25 &service, const std::size_t totalDocs,
                 const std::size_t anchorDocs, const std::string &anchorTerm)
{
    std::size_t anchorsPlaced = 0;
    for (std::size_t i = 0; i < totalDocs; ++i)
    {
        const bool placeAnchor =
            anchorDocs > 0 &&
            (anchorsPlaced * totalDocs) / anchorDocs <= i &&
            anchorsPlaced < anchorDocs;

        const std::string text =
            placeAnchor
                ? anchorTerm + " filler" + std::to_string(i)
                : "word" + std::to_string(i % 20) + " filler" + std::to_string(i);

        (void)service.IngestChunk("doc" + std::to_string(i), text);

        if (placeAnchor)
        {
            ++anchorsPlaced;
        }
    }
}

using Nanos = std::chrono::nanoseconds;

std::vector<Nanos> MeasureQueryLatency(bm25::Bm25 &service, const std::string &query,
                                       const int runs)
{
    std::vector<Nanos> durations;
    durations.reserve(static_cast<std::size_t>(runs));
    for (int i = 0; i < runs; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();
        const auto result = service.Query(query);
        const auto t1 = std::chrono::steady_clock::now();
        (void)result;
        durations.push_back(t1 - t0);
    }
    return durations;
}

Nanos Median(std::vector<Nanos> &v)
{
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}
} // namespace

TEST(PerfSmokeTest, QueryLatencyDoesNotGrowLinearlyWithCorpusSize)
{
    constexpr std::size_t kSmallTotal = 100;
    constexpr std::size_t kLargeTotal = 1000; // 10x
    constexpr std::size_t kAnchorDocs = 10;   // same hit set in both corpora
    constexpr int kRuns = 30;
    constexpr double kMaxRatio = 2.0;
    const std::string kAnchorTerm = "uniqueanchor";

    TempDb smallDb("bm25_perf_small.db");
    TempDb largeDb("bm25_perf_large.db");

    {
        bm25::Store::Sqlite smallStore(smallDb.path);
        bm25::Bm25 smallService(smallStore);
        BuildCorpus(smallService, kSmallTotal, kAnchorDocs, kAnchorTerm);

        // Warm-up.
        (void)smallService.Query(kAnchorTerm);

        auto smallDurations = MeasureQueryLatency(smallService, kAnchorTerm, kRuns);
        const auto medianSmall = Median(smallDurations);

        // Verify the small corpus query actually hits documents.
        const auto check = smallService.Query(kAnchorTerm);
        ASSERT_TRUE(check.has_value());
        ASSERT_FALSE(check->empty()) << "anchor term must appear in small corpus";

        bm25::Store::Sqlite largeStore(largeDb.path);
        bm25::Bm25 largeService(largeStore);
        BuildCorpus(largeService, kLargeTotal, kAnchorDocs, kAnchorTerm);

        // Warm-up.
        (void)largeService.Query(kAnchorTerm);

        auto largeDurations = MeasureQueryLatency(largeService, kAnchorTerm, kRuns);
        const auto medianLarge = Median(largeDurations);

        const auto check2 = largeService.Query(kAnchorTerm);
        ASSERT_TRUE(check2.has_value());
        ASSERT_FALSE(check2->empty()) << "anchor term must appear in large corpus";
        EXPECT_EQ(check2->size(), check->size()) << "postings hit set should be the same size";

        if (medianSmall.count() > 0)
        {
            const double ratio =
                static_cast<double>(medianLarge.count()) /
                static_cast<double>(medianSmall.count());
            EXPECT_LE(ratio, kMaxRatio)
                << "median query latency on 10x corpus (" << medianLarge.count()
                << " ns) exceeds " << kMaxRatio << "x small-corpus median ("
                << medianSmall.count() << " ns); ratio=" << ratio;
        }
        // If medianSmall is 0 ns (sub-nanosecond resolution), the ratio check is vacuously
        // satisfied — the implementation is fast enough that the timer can't distinguish.
    }
}
