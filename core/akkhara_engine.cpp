// See akkhara_engine.h. Line-by-line port of akkhara_desktop/engine.py (and the phone's MiniModel.kt).
#include "akkhara_engine.h"

#include <onnxruntime_c_api.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace akkhara {

namespace {
constexpr int64_t PAD = 0, EOS = 1, UNK = 2;  // T5: the decoder starts from <pad>
constexpr size_t MAX_IN = 160, MAX_OUT = 160;

bool isCancelled(const Cancelled& c) { return c && c(); }

void logSoftmax(std::vector<float>& x) {
    float m = *std::max_element(x.begin(), x.end());
    double s = 0;
    for (float v : x) s += std::exp(double(v) - m);
    float log = m + float(std::log(s));
    for (float& v : x) v -= log;
}

// ---- tiny reader for the "vocab" object of tokenizer.json ({"<pad>": 0, " ": 3, ...}) ----------
// Only single characters matter (the special tokens <pad>, </s>, <unk> have fixed ids 0, 1, 2).
void appendUtf8(std::string& out, char32_t c) {
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xC0 | (c >> 6)); out += char(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { out += char(0xE0 | (c >> 12)); out += char(0x80 | ((c >> 6) & 0x3F)); out += char(0x80 | (c & 0x3F)); }
    else { out += char(0xF0 | (c >> 18)); out += char(0x80 | ((c >> 12) & 0x3F)); out += char(0x80 | ((c >> 6) & 0x3F)); out += char(0x80 | (c & 0x3F)); }
}

std::string readJsonString(const std::string& s, size_t& i) {  // s[i] == '"'
    std::string out;
    ++i;
    while (i < s.size() && s[i] != '"') {
        char c = s[i++];
        if (c != '\\') { out += c; continue; }
        char e = s[i++];
        switch (e) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                char32_t cp = char32_t(std::stoul(s.substr(i, 4), nullptr, 16));
                i += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && s.compare(i, 2, "\\u") == 0) {  // surrogate pair
                    char32_t lo = char32_t(std::stoul(s.substr(i + 2, 4), nullptr, 16));
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
                appendUtf8(out, cp);
                break;
            }
            default: out += e;  // \" \\ \/
        }
    }
    ++i;  // closing quote
    return out;
}

std::unordered_map<std::string, int64_t> readVocab(const std::filesystem::path& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + file.string());
    std::stringstream ss;
    ss << f.rdbuf();
    std::string s = ss.str();
    size_t i = s.find("\"vocab\"");
    if (i == std::string::npos) throw std::runtime_error("no vocab in tokenizer.json");
    i = s.find('{', i) + 1;
    std::unordered_map<std::string, int64_t> vocab;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t' || s[i] == ',')) ++i;
        if (s[i] == '}') break;
        std::string key = readJsonString(s, i);
        i = s.find(':', i) + 1;
        size_t end = s.find_first_of(",}", i);
        vocab[key] = std::stoll(s.substr(i, end - i));
        i = end;
    }
    return vocab;
}
}  // namespace

// ---- UTF conversions ------------------------------------------------------------------------
std::u32string fromUtf8(const std::string& s) {
    std::u32string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        char32_t cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
        else { cp = c & 0x07; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        out += cp;
        i += n;
    }
    return out;
}

std::string toUtf8(const std::u32string& s) {
    std::string out;
    for (char32_t c : s) appendUtf8(out, c);
    return out;
}

std::u32string fromUtf16(const std::u16string& s) {
    std::u32string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char32_t c = s[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size()) {
            c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            ++i;
        }
        out += c;
    }
    return out;
}

std::u16string toUtf16(const std::u32string& s) {
    std::u16string out;
    for (char32_t c : s) {
        if (c < 0x10000) out += char16_t(c);
        else { c -= 0x10000; out += char16_t(0xD800 + (c >> 10)); out += char16_t(0xDC00 + (c & 0x3FF)); }
    }
    return out;
}

// ---- MiniModel ------------------------------------------------------------------------------
void MiniModel::check(void* status) const {
    if (!status) return;
    auto* st = static_cast<OrtStatus*>(status);
    std::string msg = api_->GetErrorMessage(st);
    api_->ReleaseStatus(st);
    throw std::runtime_error("onnxruntime: " + msg);
}

MiniModel::MiniModel(const OrtApi* api, const std::filesystem::path& dir, int threads) : api_(api) {
    check(api_->CreateEnv(ORT_LOGGING_LEVEL_ERROR, "akkhara", &env_));
    check(api_->CreateSessionOptions(&opts_));
    check(api_->SetIntraOpNumThreads(opts_, threads));
    check(api_->SetSessionGraphOptimizationLevel(opts_, ORT_ENABLE_ALL));
    auto enc = (dir / "encoder.int8.onnx").native();
    auto dec = (dir / "decoder.int8.onnx").native();
    check(api_->CreateSession(env_, enc.c_str(), opts_, &encoder_));
    check(api_->CreateSession(env_, dec.c_str(), opts_, &decoder_));
    check(api_->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mem_));
    for (auto& [key, id] : readVocab(dir / "tokenizer.json")) {
        std::u32string k = fromUtf8(key);
        if (k.size() == 1) {  // one character per token (special tokens are longer)
            vocab_[k[0]] = id;
            idToChar_[id] = k[0];
        }
    }
}

MiniModel::~MiniModel() {
    if (mem_) api_->ReleaseMemoryInfo(mem_);
    if (decoder_) api_->ReleaseSession(decoder_);
    if (encoder_) api_->ReleaseSession(encoder_);
    if (opts_) api_->ReleaseSessionOptions(opts_);
    if (env_) api_->ReleaseEnv(env_);
}

std::vector<int64_t> MiniModel::tokenize(const std::u32string& text) const {
    std::vector<int64_t> ids;
    for (size_t i = 0; i < text.size() && ids.size() < MAX_IN - 1; ++i) {
        auto it = vocab_.find(text[i]);
        ids.push_back(it == vocab_.end() ? UNK : it->second);
    }
    ids.push_back(EOS);
    return ids;
}

std::u32string MiniModel::detokenize(const std::vector<int64_t>& ids) const {
    std::u32string out;
    for (int64_t id : ids) {
        if (id <= UNK) continue;
        auto it = idToChar_.find(id);
        if (it != idToChar_.end()) out += it->second;
    }
    return out;
}

MiniModel::Encoded MiniModel::encode(const std::u32string& text) {
    Encoded e;
    std::vector<int64_t> ids = tokenize(text);
    e.mask.assign(ids.size(), 1);
    int64_t shape[2] = {1, int64_t(ids.size())};
    OrtValue *idT = nullptr, *maskT = nullptr, *out = nullptr;
    check(api_->CreateTensorWithDataAsOrtValue(mem_, ids.data(), ids.size() * sizeof(int64_t), shape, 2,
                                               ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &idT));
    check(api_->CreateTensorWithDataAsOrtValue(mem_, e.mask.data(), e.mask.size() * sizeof(int64_t), shape, 2,
                                               ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &maskT));
    const char* inNames[] = {"input_ids", "attention_mask"};
    const char* outNames[] = {"encoder_hidden_states"};
    const OrtValue* ins[] = {idT, maskT};
    OrtStatus* st = api_->Run(encoder_, nullptr, inNames, ins, 2, outNames, 1, &out);
    api_->ReleaseValue(idT);
    api_->ReleaseValue(maskT);
    check(st);
    OrtTensorTypeAndShapeInfo* info = nullptr;
    check(api_->GetTensorTypeAndShape(out, &info));
    int64_t dims[3] = {0, 0, 0};
    check(api_->GetDimensions(info, dims, 3));
    api_->ReleaseTensorTypeAndShapeInfo(info);
    e.srcLen = dims[1];
    e.dim = dims[2];
    float* data = nullptr;
    check(api_->GetTensorMutableData(out, reinterpret_cast<void**>(&data)));
    e.hidden.assign(data, data + e.srcLen * e.dim);
    api_->ReleaseValue(out);
    return e;
}

std::vector<std::vector<float>> MiniModel::nextLogProbs(const Encoded& enc,
                                                        const std::vector<std::vector<int64_t>>& prefixes) {
    const int64_t b = int64_t(prefixes.size()), t = int64_t(prefixes[0].size());
    std::vector<int64_t> dec(b * t);
    for (int64_t i = 0; i < b; ++i) std::copy(prefixes[i].begin(), prefixes[i].end(), dec.begin() + i * t);
    std::vector<float> hidden(b * enc.hidden.size());
    std::vector<int64_t> mask(b * enc.srcLen);
    for (int64_t i = 0; i < b; ++i) {
        std::copy(enc.hidden.begin(), enc.hidden.end(), hidden.begin() + i * enc.hidden.size());
        std::copy(enc.mask.begin(), enc.mask.end(), mask.begin() + i * enc.srcLen);
    }
    int64_t decShape[2] = {b, t}, hShape[3] = {b, enc.srcLen, enc.dim}, mShape[2] = {b, enc.srcLen};
    OrtValue *decT = nullptr, *hT = nullptr, *mT = nullptr, *out = nullptr;
    check(api_->CreateTensorWithDataAsOrtValue(mem_, dec.data(), dec.size() * sizeof(int64_t), decShape, 2,
                                               ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &decT));
    check(api_->CreateTensorWithDataAsOrtValue(mem_, hidden.data(), hidden.size() * sizeof(float), hShape, 3,
                                               ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &hT));
    check(api_->CreateTensorWithDataAsOrtValue(mem_, mask.data(), mask.size() * sizeof(int64_t), mShape, 2,
                                               ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &mT));
    const char* inNames[] = {"decoder_input_ids", "encoder_hidden_states", "attention_mask"};
    const char* outNames[] = {"logits"};
    const OrtValue* ins[] = {decT, hT, mT};
    OrtStatus* st = api_->Run(decoder_, nullptr, inNames, ins, 3, outNames, 1, &out);
    api_->ReleaseValue(decT);
    api_->ReleaseValue(hT);
    api_->ReleaseValue(mT);
    check(st);
    OrtTensorTypeAndShapeInfo* info = nullptr;
    check(api_->GetTensorTypeAndShape(out, &info));
    int64_t dims[3] = {0, 0, 0};
    check(api_->GetDimensions(info, dims, 3));
    api_->ReleaseTensorTypeAndShapeInfo(info);
    const int64_t v = dims[2];
    float* logits = nullptr;
    check(api_->GetTensorMutableData(out, reinterpret_cast<void**>(&logits)));
    std::vector<std::vector<float>> rows(b);
    for (int64_t i = 0; i < b; ++i) {
        const float* last = logits + (i * t + t - 1) * v;  // only the LAST position matters
        rows[i].assign(last, last + v);
        logSoftmax(rows[i]);
    }
    api_->ReleaseValue(out);
    return rows;
}

std::u32string MiniModel::greedy(const std::u32string& text, const Cancelled& cancelled) {
    if (text.find_first_not_of(U" \t\r\n") == std::u32string::npos) return U"";
    Encoded enc = encode(text);
    std::vector<int64_t> seq{PAD};
    for (size_t step = 0; step < MAX_OUT; ++step) {
        if (isCancelled(cancelled)) break;
        auto lp = nextLogProbs(enc, {seq})[0];
        int64_t best = int64_t(std::max_element(lp.begin(), lp.end()) - lp.begin());
        if (best == EOS) break;
        seq.push_back(best);
    }
    return detokenize(seq);
}

std::vector<Candidate> MiniModel::topK(const std::u32string& text, int k, int beams, const Cancelled& cancelled) {
    if (text.find_first_not_of(U" \t\r\n") == std::u32string::npos) return {};
    beams = std::max(beams, k);
    Encoded enc = encode(text);
    std::vector<std::vector<int64_t>> seqs{{PAD}};
    std::vector<float> scores{0.f};
    std::vector<std::pair<float, std::vector<int64_t>>> finished;
    for (size_t step = 0; step < MAX_OUT; ++step) {
        if (isCancelled(cancelled)) return {};  // a newer key press arrived: stop
        auto lp = nextLogProbs(enc, seqs);
        const size_t v = lp[0].size();
        std::vector<std::pair<float, size_t>> total;  // (score, beam * v + letter)
        total.reserve(seqs.size() * v);
        for (size_t b = 0; b < seqs.size(); ++b)
            for (size_t tok = 0; tok < v; ++tok) total.emplace_back(scores[b] + lp[b][tok], b * v + tok);
        const size_t keep = std::min(total.size(), size_t(2 * beams));
        std::partial_sort(total.begin(), total.begin() + keep, total.end(),
                          [](auto& a, auto& b) { return a.first != b.first ? a.first > b.first : a.second > b.second; });
        std::vector<std::vector<int64_t>> newSeqs;
        std::vector<float> newScores;
        for (size_t i = 0; i < keep; ++i) {
            size_t b = total[i].second / v, tok = total[i].second % v;
            auto seq = seqs[b];
            seq.push_back(int64_t(tok));
            if (int64_t(tok) == EOS) finished.emplace_back(total[i].first / float(seq.size() - 1), seq);
            else { newSeqs.push_back(seq); newScores.push_back(total[i].first); }
            if (int(newSeqs.size()) == beams) break;
        }
        // stop when k are finished and no open beam can beat the k-th best of them
        if (int(finished.size()) >= k && !newScores.empty()) {
            std::vector<float> fs;
            for (auto& f : finished) fs.push_back(f.first);
            std::sort(fs.begin(), fs.end());
            float best = *std::max_element(newScores.begin(), newScores.end()) / float(step + 1);
            if (best < fs[fs.size() - k]) break;
        }
        if (newSeqs.empty()) break;
        seqs = std::move(newSeqs);
        scores = std::move(newScores);
    }
    if (int(finished.size()) < k)  // max length reached: open beams count too
        for (size_t i = 0; i < seqs.size(); ++i)
            finished.emplace_back(scores[i] / float(std::max<size_t>(1, seqs[i].size() - 1)), seqs[i]);
    std::stable_sort(finished.begin(), finished.end(), [](auto& a, auto& b) { return a.first > b.first; });
    std::vector<Candidate> out;
    for (auto& [s, q] : finished) {
        std::u32string t = detokenize(q);
        if (std::none_of(out.begin(), out.end(), [&](auto& c) { return c.text == t; })) out.push_back({t, s});
        if (int(out.size()) == k) break;
    }
    return out;
}

// ---- live typing ----------------------------------------------------------------------------
namespace {
bool isLatin(char32_t c) { return c < 128 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); }
bool isSpace(char32_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0xA0 || c == 0x3000; }
}  // namespace

std::u32string join(const std::u32string& left, const std::u32string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (isLatin(left.back()) || isLatin(right.front())) return left + U" " + right;
    return left + right;
}

LiveEngine::LiveEngine(MiniModel& model, size_t chunkWords, size_t cacheSize)
    : model_(model), chunkWords_(chunkWords), cacheSize_(cacheSize) {}

std::vector<std::u32string> LiveEngine::chunks(const std::u32string& text) const {
    std::vector<std::u32string> words, out;
    std::u32string w;
    for (char32_t c : text) {
        if (isSpace(c)) { if (!w.empty()) words.push_back(w), w.clear(); }
        else w += c;
    }
    if (!w.empty()) words.push_back(w);
    for (size_t i = 0; i < words.size(); i += chunkWords_) {
        std::u32string c;
        for (size_t j = i; j < std::min(words.size(), i + chunkWords_); ++j) c += (j > i ? U" " : U"") + words[j];
        out.push_back(c);
    }
    return out;
}

bool LiveEngine::cached(const std::u32string& chunk, std::vector<Candidate>& out) {
    std::lock_guard<std::mutex> g(lock_);
    auto it = index_.find(chunk);
    if (it == index_.end()) return false;
    lru_.splice(lru_.begin(), lru_, it->second);  // mark as newest
    out = it->second->second;
    return true;
}

std::vector<Candidate> LiveEngine::top(const std::u32string& chunk, const Cancelled& cancelled) {
    std::vector<Candidate> r;
    if (cached(chunk, r)) return r;
    r = model_.topK(chunk, 5, 5, cancelled);
    if (!r.empty()) {  // a cancelled (empty) result is NOT cached
        std::lock_guard<std::mutex> g(lock_);
        if (!index_.count(chunk)) {
            lru_.emplace_front(chunk, r);
            index_[chunk] = lru_.begin();
            while (lru_.size() > cacheSize_) { index_.erase(lru_.back().first); lru_.pop_back(); }
        }
    }
    return r;
}

std::u32string LiveEngine::prefix(const std::vector<std::u32string>& cs) {
    std::u32string out;
    for (size_t i = 0; i + 1 < cs.size(); ++i) {
        auto t = top(cs[i]);
        out = join(out, t.empty() ? U"" : t[0].text);
    }
    return out;
}

std::u32string LiveEngine::quick(const std::u32string& text, const Cancelled& cancelled) {
    auto cs = chunks(text);
    if (cs.empty()) return U"";
    std::vector<Candidate> known;
    std::u32string last = cached(cs.back(), known) && !known.empty() ? known[0].text
                                                                      : model_.greedy(cs.back(), cancelled);
    return join(prefix(cs), last);
}

std::vector<std::u32string> LiveEngine::candidates(const std::u32string& text, const Cancelled& cancelled) {
    auto cs = chunks(text);
    if (cs.empty()) return {};
    std::u32string p = prefix(cs);
    std::vector<std::u32string> out;
    for (auto& c : top(cs.back(), cancelled)) out.push_back(join(p, c.text));
    return out;
}

}  // namespace akkhara
