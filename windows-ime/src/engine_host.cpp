#include "engine_host.h"

#include <onnxruntime_c_api.h>

#include <stdexcept>

#include "../../core/akkhara_engine.h"

namespace akk {

struct EngineHost::Impl {
    std::unique_ptr<akkhara::MiniModel> model;
    std::unique_ptr<akkhara::LiveEngine> engine;
};

namespace {
std::u32string ToU32(const std::wstring& w) {
    return akkhara::fromUtf16(std::u16string(w.begin(), w.end()));
}
std::wstring ToW(const std::u32string& s) {
    auto u = akkhara::toUtf16(s);
    return std::wstring(u.begin(), u.end());
}
}  // namespace

EngineHost& EngineHost::Get() {
    static EngineHost* host = new EngineHost();  // never destroyed: the worker lives as long as the app
    return *host;
}

bool EngineHost::Ready() const { return state_.load() == 1; }
bool EngineHost::Failed() const { return state_.load() == 2; }

void EngineHost::Start() {
    std::lock_guard<std::mutex> g(mu_);
    if (started_) return;
    started_ = true;
    DllAddRef();  // keep this DLL loaded while the worker thread exists
    std::thread([this] { Run(); }).detach();
}

void EngineHost::Run() {
    // 1. load ONNX Runtime from our own folder (full path: never Windows' System32 copy)
    auto impl = std::make_shared<Impl>();
    try {
        std::wstring dir = ModuleDir();
        HMODULE ort = LoadLibraryExW((dir + L"akkhara-ort.dll").c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!ort) throw std::runtime_error("akkhara-ort.dll not found");
        using GetApiBase = const OrtApiBase*(ORT_API_CALL*)();
        auto getBase = reinterpret_cast<GetApiBase>(GetProcAddress(ort, "OrtGetApiBase"));
        if (!getBase) throw std::runtime_error("OrtGetApiBase missing");
        const OrtApi* api = getBase()->GetApi(ORT_API_VERSION);
        if (!api) throw std::runtime_error("ONNX Runtime too old");
        // 2. load the model (…\x64\ → …\model\)
        std::filesystem::path model = std::filesystem::path(dir).parent_path().parent_path() / L"model";
        impl->model = std::make_unique<akkhara::MiniModel>(api, model, 2);
        impl->engine = std::make_unique<akkhara::LiveEngine>(*impl->model);
        impl->engine->quick(U"nh jg tv sala rean");  // warm-up: the first run is slower
        {
            std::lock_guard<std::mutex> g(mu_);
            impl_ = impl;
        }
        state_ = 1;
    } catch (...) {
        state_ = 2;
        return;
    }
    // 3. serve requests: always the newest one; older ones are cancelled
    for (;;) {
        std::wstring text;
        HWND notify;
        unsigned long long id;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this] { return taken_ != latest_.load(); });
            text = pending_;
            notify = notify_;
            id = taken_ = latest_.load();
        }
        auto stale = [this, id] { return id != latest_.load(); };
        auto post = [&](Results* r) {
            if (!notify || !PostMessageW(notify, WM_AKK_RESULTS, 0, reinterpret_cast<LPARAM>(r))) delete r;
        };
        try {
            auto u = ToU32(text);
            auto quick = impl->engine->quick(u, stale);
            if (stale()) continue;
            post(new Results{id, text, {ToW(quick)}, false});
            auto top = impl->engine->candidates(u, stale);
            if (stale() || top.empty()) continue;
            auto* r = new Results{id, text, {}, true};
            for (auto& t : top) r->items.push_back(ToW(t));
            post(r);
        } catch (...) {
            // a model error must never crash the app the user is typing in
        }
    }
}

unsigned long long EngineHost::Request(const std::wstring& latin, HWND notify) {
    Start();
    std::lock_guard<std::mutex> g(mu_);
    pending_ = latin;
    notify_ = notify;
    ++latest_;
    cv_.notify_one();
    return latest_.load();
}

void EngineHost::CancelAll() {
    std::lock_guard<std::mutex> g(mu_);
    taken_ = ++latest_;  // nothing pending
}

std::wstring EngineHost::QuickSync(const std::wstring& latin) {
    std::shared_ptr<Impl> impl;
    {
        std::lock_guard<std::mutex> g(mu_);
        impl = impl_;
    }
    if (!impl) return latin;
    try {
        return ToW(impl->engine->quick(ToU32(latin)));
    } catch (...) {
        return latin;
    }
}

}  // namespace akk
