// How the Latin looks while you type it: underlined (like the phone keyboard).
#pragma once

#include "globals.h"

namespace akk {

class InputDisplayAttribute : public ITfDisplayAttributeInfo {
public:
    InputDisplayAttribute() { DllAddRef(); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ITfDisplayAttributeInfo) {
            *ppv = static_cast<ITfDisplayAttributeInfo*>(this);
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
    STDMETHODIMP GetGUID(GUID* guid) override {
        if (!guid) return E_INVALIDARG;
        *guid = GUID_DisplayAttributeInput;
        return S_OK;
    }
    STDMETHODIMP GetDescription(BSTR* desc) override {
        if (!desc) return E_INVALIDARG;
        *desc = SysAllocString(L"Akkhara input");
        return *desc ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* da) override {
        if (!da) return E_INVALIDARG;
        *da = Default();
        return S_OK;
    }
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE*) override { return E_NOTIMPL; }
    STDMETHODIMP Reset() override { return S_OK; }

    static TF_DISPLAYATTRIBUTE Default() {
        TF_DISPLAYATTRIBUTE da{};
        da.crText.type = TF_CT_NONE;
        da.crBk.type = TF_CT_NONE;
        da.lsStyle = TF_LS_SOLID;  // solid underline
        da.fBoldLine = FALSE;
        da.crLine.type = TF_CT_NONE;
        da.bAttr = TF_ATTR_INPUT;
        return da;
    }

private:
    ~InputDisplayAttribute() { DllRelease(); }
    LONG refs_ = 1;
};

class EnumDisplayAttributes : public IEnumTfDisplayAttributeInfo {
public:
    EnumDisplayAttributes() { DllAddRef(); }
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IEnumTfDisplayAttributeInfo) {
            *ppv = static_cast<IEnumTfDisplayAttributeInfo*>(this);
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
    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** out) override {
        if (!out) return E_INVALIDARG;
        auto* e = new (std::nothrow) EnumDisplayAttributes();
        if (!e) return E_OUTOFMEMORY;
        e->pos_ = pos_;
        *out = e;
        return S_OK;
    }
    STDMETHODIMP Next(ULONG count, ITfDisplayAttributeInfo** out, ULONG* fetched) override {
        ULONG n = 0;
        if (!out) return E_INVALIDARG;
        while (n < count && pos_ < 1) {
            out[n] = new (std::nothrow) InputDisplayAttribute();
            if (!out[n]) break;
            ++n;
            ++pos_;
        }
        if (fetched) *fetched = n;
        return n == count ? S_OK : S_FALSE;
    }
    STDMETHODIMP Reset() override { pos_ = 0; return S_OK; }
    STDMETHODIMP Skip(ULONG count) override {
        pos_ += count;
        return pos_ <= 1 ? S_OK : S_FALSE;
    }

private:
    ~EnumDisplayAttributes() { DllRelease(); }
    LONG refs_ = 1;
    ULONG pos_ = 0;
};

}  // namespace akk
