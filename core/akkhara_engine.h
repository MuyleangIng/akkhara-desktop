// Akkhara engine in C++: akkhara-mini (ONNX int8) → Khmer suggestions.
//
// The same "brain" as the phone keyboard (MiniModel.kt + LiveEngine.kt) and the Python test app
// (akkhara_desktop/engine.py), so all of them give the same answers:
//   1. tokenizer: every character → an id (tokenizer.json), then </s>
//   2. encoder: ids → hidden states                         (once per chunk)
//   3. decoder: hidden states + Khmer letters so far → scores of the next letter
//        greedy = best letter each step (fast, #1 only);  beam = keep the 5 best (top-5)
//   4. live typing: long text is cut into 6-word chunks; finished chunks come from a cache.
//
// Plain C++17 + the ONNX Runtime C API. Used by the Windows keyboard (TSF) and the Mac keyboard.
#pragma once

#include <filesystem>
#include <functional>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct OrtApi;
struct OrtEnv;
struct OrtSession;
struct OrtSessionOptions;
struct OrtMemoryInfo;

namespace akkhara {

using Cancelled = std::function<bool()>;  // true = a newer key press made this work useless

struct Candidate {
    std::u32string text;
    float score;
};

class MiniModel {
public:
    // api: from OrtGetApiBase()->GetApi(ORT_API_VERSION). dir: folder with encoder.int8.onnx,
    // decoder.int8.onnx and tokenizer.json. Throws std::runtime_error on failure.
    MiniModel(const OrtApi* api, const std::filesystem::path& dir, int threads = 2);
    ~MiniModel();
    MiniModel(const MiniModel&) = delete;
    MiniModel& operator=(const MiniModel&) = delete;

    // abbreviation (lowercase) → full Khmer name, from abbreviations.tsv next to the model (may be empty)
    const std::unordered_map<std::u32string, std::u32string>& abbreviations() const { return abbrevs_; }

    std::u32string greedy(const std::u32string& text, const Cancelled& cancelled = nullptr);
    std::vector<Candidate> topK(const std::u32string& text, int k = 5, int beams = 5,
                                const Cancelled& cancelled = nullptr);

private:
    struct Encoded {
        std::vector<float> hidden;  // [srcLen, dim]
        std::vector<int64_t> mask;  // [srcLen]
        int64_t srcLen = 0, dim = 0;
    };
    std::vector<int64_t> tokenize(const std::u32string& text) const;
    std::u32string detokenize(const std::vector<int64_t>& ids) const;
    Encoded encode(const std::u32string& text);
    std::vector<std::vector<float>> nextLogProbs(const Encoded& enc,
                                                 const std::vector<std::vector<int64_t>>& prefixes);
    void check(void* status) const;

    const OrtApi* api_;
    OrtEnv* env_ = nullptr;
    OrtSessionOptions* opts_ = nullptr;
    OrtSession* encoder_ = nullptr;
    OrtSession* decoder_ = nullptr;
    OrtMemoryInfo* mem_ = nullptr;
    std::unordered_map<char32_t, int64_t> vocab_;
    std::unordered_map<int64_t, char32_t> idToChar_;
    std::unordered_map<std::u32string, std::u32string> abbrevs_;
};

// Top-k for text with known abbreviations, which are NOT sent to the model (alone, the model turned
// "mptc" into a wrong ministry): #1 other words from the model + abbreviation as typed, #2 the same with
// the full Khmer name, then other candidates of the last plain piece. Returns false (out untouched)
// when there is no known abbreviation. Same logic as akkhara/abbrev.py and the phone's LiveEngine.kt.
bool withAbbreviations(const std::u32string& text,
                       const std::unordered_map<std::u32string, std::u32string>& table, size_t k,
                       const std::function<std::vector<std::u32string>(const std::u32string&)>& topFn,
                       std::vector<std::u32string>& out);

// No space between Khmer pieces; one space next to a Latin word (training target rule).
std::u32string join(const std::u32string& left, const std::u32string& right);

// Live typing: 6-word chunks + an LRU cache of finished chunks (thread-safe).
class LiveEngine {
public:
    explicit LiveEngine(MiniModel& model, size_t chunkWords = 6, size_t cacheSize = 500);
    std::u32string quick(const std::u32string& text, const Cancelled& cancelled = nullptr);
    std::vector<std::u32string> candidates(const std::u32string& text, const Cancelled& cancelled = nullptr);
    std::vector<std::u32string> chunks(const std::u32string& text) const;

private:
    std::vector<Candidate> top(const std::u32string& chunk, const Cancelled& cancelled = nullptr);
    std::u32string quickPlain(const std::u32string& text, const Cancelled& cancelled);
    std::vector<std::u32string> candidatesPlain(const std::u32string& text, const Cancelled& cancelled);
    std::u32string prefix(const std::vector<std::u32string>& cs);
    bool cached(const std::u32string& chunk, std::vector<Candidate>& out);

    MiniModel& model_;
    size_t chunkWords_, cacheSize_;
    std::mutex lock_;
    std::list<std::pair<std::u32string, std::vector<Candidate>>> lru_;  // front = newest
    std::unordered_map<std::u32string, decltype(lru_)::iterator> index_;
};

// UTF helpers (Windows uses UTF-16, files and the Mac use UTF-8).
std::u32string fromUtf8(const std::string& s);
std::string toUtf8(const std::u32string& s);
std::u32string fromUtf16(const std::u16string& s);
std::u16string toUtf16(const std::u32string& s);

}  // namespace akkhara
