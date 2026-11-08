// DLL entry points: the COM class factory and (un)registration. Windows loads this DLL into every
// app where you type with Akkhara selected.
#include "globals.h"
#include "text_service.h"

#include <new>

namespace akk {
HINSTANCE g_module = nullptr;
std::atomic<long> g_refs{0};

std::wstring ModuleDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(g_module, path, MAX_PATH);
    std::wstring p(path);
    return p.substr(0, p.find_last_of(L"\\/") + 1);
}

HRESULT RegisterServer();    // register.cpp
HRESULT UnregisterServer();  // register.cpp

namespace {
class ClassFactory : public IClassFactory {
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { DllAddRef(); return 2; }   // static object
    STDMETHODIMP_(ULONG) Release() override { DllRelease(); return 1; }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* svc = new (std::nothrow) TextService();
        if (!svc) return E_OUTOFMEMORY;
        HRESULT hr = svc->QueryInterface(riid, ppv);
        svc->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        lock ? DllAddRef() : DllRelease();
        return S_OK;
    }
};
ClassFactory g_factory;
}  // namespace
}  // namespace akk

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        akk::g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (clsid != akk::CLSID_Akkhara) return CLASS_E_CLASSNOTAVAILABLE;
    return akk::g_factory.QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return akk::g_refs.load() <= 0 ? S_OK : S_FALSE; }
STDAPI DllRegisterServer() { return akk::RegisterServer(); }
STDAPI DllUnregisterServer() { return akk::UnregisterServer(); }
