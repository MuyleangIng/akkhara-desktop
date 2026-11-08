// Runs the Akkhara engine on a background thread, so typing never waits for the model.
//
// One engine per app (process). Loaded the first time Akkhara is switched on in that app:
//   <install>\x64\akkhara-ort.dll      ONNX Runtime (renamed so it never clashes with Windows' own copy)
//   <install>\model\encoder.int8.onnx, decoder.int8.onnx, tokenizer.json
// Results go back to the candidate window as a window message.
#pragma once

#include "globals.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace akk {

inline constexpr UINT WM_AKK_RESULTS = WM_APP + 0x41;  // lParam = Results* (receiver deletes it)

struct Results {
    unsigned long long id = 0;
    std::wstring text;               // the Latin these suggestions are for
    std::vector<std::wstring> items; // Khmer suggestions, best first
    bool final = false;              // false = fast #1 only, true = full top-5
};

class EngineHost {
public:
    static EngineHost& Get();

    void Start();          // start loading in the background (once per process)
    bool Ready() const;    // model loaded
    bool Failed() const;   // could not load (missing files …)

    // Ask for suggestions; an older request still running is cancelled.
    // Results are posted to `notify` as WM_AKK_RESULTS (one quick, then one final).
    unsigned long long Request(const std::wstring& latin, HWND notify);
    void CancelAll();

    // #1 right now, on this thread (Enter pressed before the suggestions arrived).
    std::wstring QuickSync(const std::wstring& latin);

private:
    EngineHost() = default;
    void Run();
    struct Impl;
    std::shared_ptr<Impl> impl_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::wstring pending_;
    HWND notify_ = nullptr;
    std::atomic<unsigned long long> latest_{0};  // newest request (read by the worker to cancel)
    unsigned long long taken_ = 0;
    bool started_ = false;
    std::atomic<int> state_{0};  // 0 loading, 1 ready, 2 failed
};

}  // namespace akk
