// The Akkhara keyboard itself (TSF text input processor).
//
// While Akkhara is the active keyboard:
//   - letters start a "composition": the Latin you type stays in the app, underlined
//   - the suggestion box shows the top-5 Khmer under it (computed in the background)
//   - Enter = insert the highlighted Khmer, ↑/↓ (or Tab) = choose, click = insert that row,
//     Shift+Enter or Esc = keep exactly the Latin you typed, Backspace = edit
//   - Ctrl/Alt shortcuts and every other key work as usual
#pragma once

#include "candidate_window.h"
#include "globals.h"

#include <functional>
#include <string>
#include <vector>

namespace akk {

class TextService : public ITfTextInputProcessorEx,
                    public ITfThreadMgrEventSink,
                    public ITfKeyEventSink,
                    public ITfCompositionSink,
                    public ITfDisplayAttributeProvider {
public:
    TextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor(Ex)
    STDMETHODIMP Activate(ITfThreadMgr* tm, TfClientId id) override { return ActivateEx(tm, id, 0); }
    STDMETHODIMP ActivateEx(ITfThreadMgr* tm, TfClientId id, DWORD flags) override;
    STDMETHODIMP Deactivate() override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* focus, ITfDocumentMgr* prev) override;
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL foreground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* eaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM lp, BOOL* eaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override { *eaten = FALSE; return S_OK; }
    STDMETHODIMP OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override { *eaten = FALSE; return S_OK; }
    STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override { *eaten = FALSE; return S_OK; }

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ec, ITfComposition* comp) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** out) override;

private:
    ~TextService();
    enum class Action { None, Ignore, Append, Backspace, Commit, KeepLatin, Up, Down };
    Action Classify(ITfContext* pic, WPARAM vk, wchar_t* ch) const;
    bool Composing() const { return composition_ != nullptr; }
    bool ContextDisabled(ITfContext* pic) const;

    HRESULT Edit(ITfContext* pic, std::function<void(TfEditCookie)> fn, bool sync);
    void SetCompositionText(TfEditCookie ec, ITfContext* pic, const std::wstring& text);
    void Commit(TfEditCookie ec, const std::wstring& text);
    void UpdateAnchor(TfEditCookie ec, ITfContext* pic);
    void Reset();

    void RequestSuggestions();
    void OnEngineMessage(UINT msg, LPARAM lp);
    void OnClick(int index);
    void Refresh();
    std::wstring Choice();  // the Khmer to insert for the current Latin

    LONG refs_ = 1;
    ITfThreadMgr* threadMgr_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
    DWORD threadMgrCookie_ = TF_INVALID_COOKIE;
    TfGuidAtom attrAtom_ = TF_INVALID_GUIDATOM;

    ITfComposition* composition_ = nullptr;
    ITfContext* context_ = nullptr;  // the document of the current composition
    std::wstring latin_;             // what the user typed
    std::vector<std::wstring> items_;
    std::wstring itemsFor_;          // the Latin that items_ belong to
    unsigned long long requestId_ = 0;
    int selected_ = 0;
    CandidateWindow window_;
};

}  // namespace akk
