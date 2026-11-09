// C bridge to the C++ engine, for the Swift input method (see akkhara_c.h).
#include "akkhara_c.h"

#include <onnxruntime_c_api.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <string>

#include "akkhara_engine.h"

struct AkkEngine {
    std::unique_ptr<akkhara::MiniModel> model;
    std::unique_ptr<akkhara::LiveEngine> live;
    std::atomic<int64_t> generation{0};
};

namespace {
void copyOut(const std::string& s, char* out, size_t len) {
    if (!out || len == 0) return;
    size_t n = s.size() < len - 1 ? s.size() : len - 1;
    std::memcpy(out, s.data(), n);
    out[n] = 0;
}
}  // namespace

extern "C" AkkEngine* akk_open(const char* dir, char* err, size_t errLen) {
    try {
        const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
        auto e = std::make_unique<AkkEngine>();
        e->model = std::make_unique<akkhara::MiniModel>(api, std::filesystem::u8path(dir), 2);
        e->live = std::make_unique<akkhara::LiveEngine>(*e->model);
        return e.release();
    } catch (const std::exception& ex) {
        copyOut(ex.what(), err, errLen);
        return nullptr;
    }
}

extern "C" void akk_close(AkkEngine* e) { delete e; }

extern "C" void akk_set_generation(AkkEngine* e, int64_t gen) { e->generation = gen; }

extern "C" int akk_quick(AkkEngine* e, int64_t gen, const char* latin, char* out, size_t len) {
    try {
        auto cancelled = [e, gen] { return e->generation.load() != gen; };
        auto r = e->live->quick(akkhara::fromUtf8(latin), cancelled);
        if (cancelled()) return 0;
        copyOut(akkhara::toUtf8(r), out, len);
        return 1;
    } catch (...) {
        return 0;
    }
}

extern "C" int akk_candidates(AkkEngine* e, int64_t gen, const char* latin, char* out, size_t len) {
    try {
        auto cancelled = [e, gen] { return e->generation.load() != gen; };
        auto items = e->live->candidates(akkhara::fromUtf8(latin), cancelled);
        if (cancelled() || items.empty()) return 0;
        std::string s;
        for (size_t i = 0; i < items.size(); ++i) s += (i ? "\n" : "") + akkhara::toUtf8(items[i]);
        copyOut(s, out, len);
        return int(items.size());
    } catch (...) {
        return 0;
    }
}
