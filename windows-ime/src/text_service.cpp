#include "text_service.h"

#include "display_attribute.h"
#include "engine_host.h"

#include <algorithm>
#include <memory>
#include <new>

namespace akk {
namespace {

// ---- a TSF edit session that runs a lambda ----------------------------------------------------
class EditSession : public ITfEditSession {
public:
    explicit EditSession(std::function<void(TfEditCookie)> fn) : fn_(std::move(fn)) {}
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ITfEditSession) {
            *ppv = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG r = InterlockedDecrement(&refs_);
        if (!r) delete this;
        return r;
    }
    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        fn_(ec);
        return S_OK;
    }

private:
    LONG refs_ = 1;
    std::function<void(TfEditCookie)> fn_;
};

template <class T>
void SafeRelease(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}

DWORD GetCompartment(IUnknown* owner, REFGUID guid) {
    DWORD value = 0;
    ITfCompartmentMgr* mgr = nullptr;
    if (owner && SUCCEEDED(owner->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&mgr)))) {
        ITfCompartment* c = nullptr;
        if (SUCCEEDED(mgr->GetCompartment(guid, &c))) {
            VARIANT v;
            VariantInit(&v);
            if (c->GetValue(&v) == S_OK && v.vt == VT_I4) value = DWORD(v.lVal);
            c->Release();
        }
        mgr->Release();
    }
    return value;
}

void SetCompartment(IUnknown* owner, TfClientId id, REFGUID guid, DWORD value) {
    ITfCompartmentMgr* mgr = nullptr;
    if (owner && SUCCEEDED(owner->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&mgr)))) {
        ITfCompartment* c = nullptr;
        if (SUCCEEDED(mgr->GetCompartment(guid, &c))) {
            VARIANT v;
            v.vt = VT_I4;
            v.lVal = LONG(value);
            c->SetValue(id, &v);
            c->Release();
        }
        mgr->Release();
    }
}

bool Down(int vk) { return (GetKeyState(vk) & 0x8000) != 0; }

// The character a key gives on a US keyboard. Akkhara reads Latin, and the Khmer system layout
// underneath gives Khmer letters, so the keys are mapped here, not by Windows.
wchar_t UsChar(WPARAM vk) {
    const bool shift = Down(VK_SHIFT);
    const bool caps = (GetKeyState(VK_CAPITAL) & 1) != 0;
    if (vk >= 'A' && vk <= 'Z') return wchar_t((shift != caps) ? vk : vk - 'A' + 'a');
    if (vk >= '0' && vk <= '9') return shift ? L")!@#$%^&*("[vk - '0'] : wchar_t(vk);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return wchar_t(L'0' + (vk - VK_NUMPAD0));
    switch (vk) {
        case VK_SPACE: return L' ';
        case VK_OEM_1: return shift ? L':' : L';';
        case VK_OEM_PLUS: return shift ? L'+' : L'=';
        case VK_OEM_COMMA: return shift ? L'<' : L',';
        case VK_OEM_MINUS: return shift ? L'_' : L'-';
        case VK_OEM_PERIOD: return shift ? L'>' : L'.';
        case VK_OEM_2: return shift ? L'?' : L'/';
        case VK_OEM_3: return shift ? L'~' : L'`';
        case VK_OEM_4: return shift ? L'{' : L'[';
        case VK_OEM_5: return shift ? L'|' : L'\\';
        case VK_OEM_6: return shift ? L'}' : L']';
        case VK_OEM_7: return shift ? L'"' : L'\'';
        case VK_DECIMAL: return L'.';
        case VK_ADD: return L'+';
        case VK_SUBTRACT: return L'-';
        case VK_MULTIPLY: return L'*';
        case VK_DIVIDE: return L'/';
    }
    return 0;
}

const wchar_t kHelp[] = L"Enter = insert   \x2191\x2193 choose   Shift+Enter / Esc = keep Latin";
}  // namespace

// ---- life cycle -------------------------------------------------------------------------------
TextService::TextService() { DllAddRef(); }
TextService::~TextService() { DllRelease(); }

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor || riid == IID_ITfTextInputProcessorEx)
        *ppv = static_cast<ITfTextInputProcessorEx*>(this);
    else if (riid == IID_ITfThreadMgrEventSink)
        *ppv = static_cast<ITfThreadMgrEventSink*>(this);
    else if (riid == IID_ITfKeyEventSink)
        *ppv = static_cast<ITfKeyEventSink*>(this);
    else if (riid == IID_ITfCompositionSink)
        *ppv = static_cast<ITfCompositionSink*>(this);
    else if (riid == IID_ITfDisplayAttributeProvider)
        *ppv = static_cast<ITfDisplayAttributeProvider*>(this);
    if (!*ppv) return E_NOINTERFACE;
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) TextService::AddRef() { return InterlockedIncrement(&refs_); }
STDMETHODIMP_(ULONG) TextService::Release() {
    LONG r = InterlockedDecrement(&refs_);
    if (!r) delete this;
    return r;
}

STDMETHODIMP TextService::ActivateEx(ITfThreadMgr* tm, TfClientId id, DWORD) {
    threadMgr_ = tm;
    threadMgr_->AddRef();
    clientId_ = id;

    ITfSource* source = nullptr;
    if (SUCCEEDED(tm->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
        source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &threadMgrCookie_);
        source->Release();
    }
    ITfKeystrokeMgr* keys = nullptr;
    if (SUCCEEDED(tm->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keys)))) {
        keys->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
        keys->Release();
    }
    ITfCategoryMgr* cat = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                   reinterpret_cast<void**>(&cat)))) {
        cat->RegisterGUID(GUID_DisplayAttributeInput, &attrAtom_);
        cat->Release();
    }
    SetCompartment(tm, clientId_, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, 1);  // keyboard "on"

    window_.Create([this](int i) { OnClick(i); }, [this](UINT m, LPARAM l) { OnEngineMessage(m, l); });
    EngineHost::Get().Start();  // load the model in the background now
    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
    EngineHost::Get().CancelAll();
    if (Composing() && context_) {
        ITfContext* pic = context_;
        pic->AddRef();
        Edit(pic, [this](TfEditCookie ec) { Commit(ec, latin_); }, true);  // keep what was typed
        pic->Release();
    }
    Reset();
    window_.Destroy();
    if (threadMgr_) {
        ITfKeystrokeMgr* keys = nullptr;
        if (SUCCEEDED(threadMgr_->QueryInterface(IID_ITfKeystrokeMgr, reinterpret_cast<void**>(&keys)))) {
            keys->UnadviseKeyEventSink(clientId_);
            keys->Release();
        }
        ITfSource* source = nullptr;
        if (threadMgrCookie_ != TF_INVALID_COOKIE &&
            SUCCEEDED(threadMgr_->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source)))) {
            source->UnadviseSink(threadMgrCookie_);
            source->Release();
        }
    }
    threadMgrCookie_ = TF_INVALID_COOKIE;
    SafeRelease(threadMgr_);
    clientId_ = TF_CLIENTID_NULL;
    return S_OK;
}

// ---- focus --------------------------------------------------------------------------------------
STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) {
    window_.Show(false);  // another document got the focus
    return S_OK;
}

STDMETHODIMP TextService::OnSetFocus(BOOL foreground) {
    if (!foreground) window_.Show(false);
    else if (Composing()) Refresh();
    return S_OK;
}

// ---- keys ---------------------------------------------------------------------------------------
bool TextService::ContextDisabled(ITfContext* pic) const {
    // password boxes and read-only fields: Windows marks the context as disabled / empty
    return GetCompartment(pic, GUID_COMPARTMENT_KEYBOARD_DISABLED) != 0 ||
           GetCompartment(pic, GUID_COMPARTMENT_EMPTYCONTEXT) != 0;
}

TextService::Action TextService::Classify(ITfContext* pic, WPARAM vk, wchar_t* ch) const {
    *ch = 0;
    if (Down(VK_CONTROL) || Down(VK_MENU) || Down(VK_LWIN) || Down(VK_RWIN)) return Action::None;  // shortcuts
    if (!Composing()) {
        if (ContextDisabled(pic)) return Action::None;
        if (vk >= 'A' && vk <= 'Z') {  // only a letter starts Sing Khmer
            *ch = UsChar(vk);
            return Action::Append;
        }
        return Action::None;
    }
    switch (vk) {
        case VK_BACK: return Action::Backspace;
        case VK_RETURN: return Down(VK_SHIFT) ? Action::KeepLatin : Action::Commit;
        case VK_ESCAPE: return Action::KeepLatin;
        case VK_UP: return Action::Up;
        case VK_DOWN: return Action::Down;
        case VK_TAB: return Down(VK_SHIFT) ? Action::Up : Action::Down;
        case VK_LEFT: case VK_RIGHT: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT: case VK_DELETE:
            return Action::Ignore;  // keep the cursor at the end of the Latin
    }
    *ch = UsChar(vk);
    return *ch ? Action::Append : Action::None;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext* pic, WPARAM wp, LPARAM, BOOL* eaten) {
    wchar_t ch;
    *eaten = Classify(pic, wp, &ch) != Action::None;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* pic, WPARAM wp, LPARAM, BOOL* eaten) {
    wchar_t ch;
    Action a = Classify(pic, wp, &ch);
    *eaten = a != Action::None;
    switch (a) {
        case Action::None:
        case Action::Ignore:
            break;
        case Action::Append:
            latin_ += ch;
            Edit(pic, [this, pic](TfEditCookie ec) { SetCompositionText(ec, pic, latin_); UpdateAnchor(ec, pic); }, true);
            if (!Composing()) {  // this field does not allow a composition: just type the letter
                latin_.clear();
                std::wstring one(1, ch);
                Edit(pic, [pic, one](TfEditCookie ec) {
                    ITfInsertAtSelection* ins = nullptr;
                    if (SUCCEEDED(pic->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&ins)))) {
                        ITfRange* r = nullptr;
                        if (SUCCEEDED(ins->InsertTextAtSelection(ec, 0, one.c_str(), LONG(one.size()), &r)) && r) r->Release();
                        ins->Release();
                    }
                }, true);
                break;
            }
            RequestSuggestions();
            break;
        case Action::Backspace:
            if (!latin_.empty()) latin_.pop_back();
            if (latin_.empty()) {
                Edit(pic, [this](TfEditCookie ec) { Commit(ec, L""); }, true);
            } else {
                Edit(pic, [this, pic](TfEditCookie ec) { SetCompositionText(ec, pic, latin_); UpdateAnchor(ec, pic); }, true);
                RequestSuggestions();
            }
            break;
        case Action::Commit: {
            std::wstring text = Choice();
            Edit(pic, [this, text](TfEditCookie ec) { Commit(ec, text); }, true);
            break;
        }
        case Action::KeepLatin: {
            std::wstring text = latin_;
            Edit(pic, [this, text](TfEditCookie ec) { Commit(ec, text); }, true);
            break;
        }
        case Action::Up:
        case Action::Down:
            if (!items_.empty()) {
                int n = int(items_.size());
                selected_ = (selected_ + (a == Action::Down ? 1 : n - 1)) % n;
                Refresh();
            }
            break;
    }
    return S_OK;
}

// ---- edit sessions --------------------------------------------------------------------------------
HRESULT TextService::Edit(ITfContext* pic, std::function<void(TfEditCookie)> fn, bool sync) {
    auto* session = new (std::nothrow) EditSession(std::move(fn));
    if (!session) return E_OUTOFMEMORY;
    HRESULT hrSession = S_OK;
    HRESULT hr = pic->RequestEditSession(clientId_, session,
                                         (sync ? TF_ES_SYNC : TF_ES_ASYNCDONTCARE) | TF_ES_READWRITE, &hrSession);
    session->Release();
    return FAILED(hr) ? hr : hrSession;
}

void TextService::SetCompositionText(TfEditCookie ec, ITfContext* pic, const std::wstring& text) {
    if (!composition_) {  // start a composition at the cursor
        ITfInsertAtSelection* ins = nullptr;
        ITfContextComposition* cc = nullptr;
        ITfRange* range = nullptr;
        if (SUCCEEDED(pic->QueryInterface(IID_ITfInsertAtSelection, reinterpret_cast<void**>(&ins))) &&
            SUCCEEDED(ins->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &range)) &&
            SUCCEEDED(pic->QueryInterface(IID_ITfContextComposition, reinterpret_cast<void**>(&cc)))) {
            ITfComposition* comp = nullptr;
            if (SUCCEEDED(cc->StartComposition(ec, range, static_cast<ITfCompositionSink*>(this), &comp)) && comp) {
                composition_ = comp;
                context_ = pic;
                context_->AddRef();
            }
        }
        SafeRelease(range);
        SafeRelease(cc);
        SafeRelease(ins);
        if (!composition_) return;
    }
    ITfRange* range = nullptr;
    if (FAILED(composition_->GetRange(&range))) return;
    range->SetText(ec, 0, text.c_str(), LONG(text.size()));
    // underline the Latin (our display attribute)
    ITfProperty* prop = nullptr;
    if (attrAtom_ != TF_INVALID_GUIDATOM && SUCCEEDED(pic->GetProperty(GUID_PROP_ATTRIBUTE, &prop))) {
        VARIANT v;
        v.vt = VT_I4;
        v.lVal = LONG(attrAtom_);
        prop->SetValue(ec, range, &v);
        prop->Release();
    }
    // cursor at the end of the Latin
    ITfRange* caret = nullptr;
    if (SUCCEEDED(range->Clone(&caret))) {
        caret->Collapse(ec, TF_ANCHOR_END);
        TF_SELECTION sel{};
        sel.range = caret;
        sel.style.ase = TF_AE_NONE;
        sel.style.fInterimChar = FALSE;
        pic->SetSelection(ec, 1, &sel);
        caret->Release();
    }
    range->Release();
}

void TextService::Commit(TfEditCookie ec, const std::wstring& text) {
    if (composition_ && context_) {
        ITfRange* range = nullptr;
        if (SUCCEEDED(composition_->GetRange(&range))) {
            ITfProperty* prop = nullptr;
            if (SUCCEEDED(context_->GetProperty(GUID_PROP_ATTRIBUTE, &prop))) {
                prop->Clear(ec, range);
                prop->Release();
            }
            range->SetText(ec, 0, text.c_str(), LONG(text.size()));
            ITfRange* caret = nullptr;
            if (SUCCEEDED(range->Clone(&caret))) {
                caret->Collapse(ec, TF_ANCHOR_END);
                TF_SELECTION sel{};
                sel.range = caret;
                sel.style.ase = TF_AE_NONE;
                context_->SetSelection(ec, 1, &sel);
                caret->Release();
            }
            range->Release();
        }
        composition_->EndComposition(ec);
    }
    Reset();
}

void TextService::UpdateAnchor(TfEditCookie ec, ITfContext* pic) {
    RECT rc{};
    bool ok = false;
    ITfRange* range = nullptr;
    ITfContextView* view = nullptr;
    if (composition_ && SUCCEEDED(composition_->GetRange(&range)) && SUCCEEDED(pic->GetActiveView(&view))) {
        BOOL clipped = FALSE;
        ok = view->GetTextExt(ec, range, &rc, &clipped) == S_OK && (rc.right > rc.left || rc.bottom > rc.top);
    }
    SafeRelease(view);
    SafeRelease(range);
    if (!ok) {  // the app did not tell where the text is: use the caret
        GUITHREADINFO gti{sizeof(gti)};
        if (GetGUIThreadInfo(0, &gti) && gti.hwndCaret) {
            rc = gti.rcCaret;
            MapWindowPoints(gti.hwndCaret, nullptr, reinterpret_cast<POINT*>(&rc), 2);
            ok = true;
        }
    }
    if (ok) window_.SetAnchor(rc);
}

void TextService::Reset() {
    EngineHost::Get().CancelAll();
    SafeRelease(composition_);
    SafeRelease(context_);
    latin_.clear();
    items_.clear();
    itemsFor_.clear();
    selected_ = 0;
    window_.SetItems({}, 0, L"");
    window_.Show(false);
}

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie ec, ITfComposition*) {
    // the app ended it (clicked elsewhere …): the Latin stays as typed
    if (composition_ && context_) {
        ITfRange* range = nullptr;
        ITfProperty* prop = nullptr;
        if (SUCCEEDED(composition_->GetRange(&range)) &&
            SUCCEEDED(context_->GetProperty(GUID_PROP_ATTRIBUTE, &prop)))
            prop->Clear(ec, range);
        SafeRelease(prop);
        SafeRelease(range);
    }
    Reset();
    return S_OK;
}

// ---- suggestions ------------------------------------------------------------------------------------
void TextService::RequestSuggestions() {
    requestId_ = EngineHost::Get().Request(latin_, window_.Hwnd());
    Refresh();
}

void TextService::OnEngineMessage(UINT msg, LPARAM lp) {
    if (msg != WM_AKK_RESULTS) return;
    std::unique_ptr<Results> r(reinterpret_cast<Results*>(lp));
    if (!Composing() || r->id != requestId_ || r->text != latin_) return;  // outdated
    if (r->final) {
        items_ = r->items;
        selected_ = 0;
    } else if (items_.empty() || itemsFor_ != latin_ || items_[0] != r->items[0]) {
        items_ = r->items;  // fast #1 while the top-5 is still coming
        selected_ = 0;
    }
    itemsFor_ = r->text;
    Refresh();
}

void TextService::Refresh() {
    if (!Composing()) {
        window_.Show(false);
        return;
    }
    std::wstring status = kHelp;
    if (EngineHost::Get().Failed()) status = L"Akkhara: model not found, reinstall. Enter keeps the Latin.";
    else if (!EngineHost::Get().Ready()) status = L"Akkhara is loading\x2026";
    window_.SetItems(items_, selected_, status);
    window_.Show(true);
}

std::wstring TextService::Choice() {
    if (!items_.empty() && itemsFor_ == latin_) return items_[std::min<size_t>(selected_, items_.size() - 1)];
    if (EngineHost::Get().Ready()) return EngineHost::Get().QuickSync(latin_);  // not ready yet: #1 now
    return latin_;
}

void TextService::OnClick(int index) {
    if (!Composing() || !context_ || index < 0 || index >= int(items_.size()) || itemsFor_ != latin_) return;
    std::wstring text = items_[index];
    ITfContext* pic = context_;
    pic->AddRef();
    Edit(pic, [this, text](TfEditCookie ec) { Commit(ec, text); }, false);
    pic->Release();
}

// ---- display attribute (the underline) ----------------------------------------------------------------
STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** out) {
    if (!out) return E_INVALIDARG;
    *out = new (std::nothrow) EnumDisplayAttributes();
    return *out ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** out) {
    if (!out) return E_INVALIDARG;
    *out = nullptr;
    if (guid != GUID_DisplayAttributeInput) return E_INVALIDARG;
    *out = new (std::nothrow) InputDisplayAttribute();
    return *out ? S_OK : E_OUTOFMEMORY;
}

}  // namespace akk
