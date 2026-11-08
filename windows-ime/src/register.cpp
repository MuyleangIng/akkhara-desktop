// regsvr32 AkkharaIME.dll → registers the COM class and the TSF language profile:
// Settings → Language → Khmer → "Add a keyboard" then lists "Akkhara (Sing Khmer)".
#include "globals.h"


namespace akk {
namespace {

// TSF categories: a keyboard, works in Store/immersive apps and the tray input indicator,
// and provides its own display attribute (the underline under the Latin while typing).
const GUID* const kCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
    &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
};

std::wstring ClsidString() {
    wchar_t buf[64] = {};
    StringFromGUID2(CLSID_Akkhara, buf, 64);
    return buf;
}

std::wstring ModulePath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(g_module, path, MAX_PATH);
    return path;
}

LONG SetValue(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          DWORD((value.size() + 1) * sizeof(wchar_t)));
}

HRESULT RegisterComClass() {
    std::wstring keyPath = L"CLSID\\" + ClsidString();
    HKEY key = nullptr, inproc = nullptr;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr))
        return E_FAIL;
    SetValue(key, nullptr, DESCRIPTION);
    LONG r = RegCreateKeyExW(key, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &inproc, nullptr);
    if (r == ERROR_SUCCESS) {
        SetValue(inproc, nullptr, ModulePath());
        SetValue(inproc, L"ThreadingModel", L"Apartment");
        RegCloseKey(inproc);
    }
    RegCloseKey(key);
    return r == ERROR_SUCCESS ? S_OK : E_FAIL;
}

void UnregisterComClass() {
    std::wstring keyPath = L"CLSID\\" + ClsidString();
    RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath.c_str());
}

HRESULT RegisterProfile() {
    ITfInputProcessorProfileMgr* mgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&mgr));
    if (FAILED(hr)) return hr;
    std::wstring icon = ModulePath();
    hr = mgr->RegisterProfile(CLSID_Akkhara, LANGID_KHMER, GUID_Profile, DESCRIPTION, ULONG(wcslen(DESCRIPTION)),
                              icon.c_str(), ULONG(icon.size()), UINT(-IDI_AKKHARA), nullptr, 0, TRUE, 0);
    mgr->Release();
    return hr;
}

void UnregisterProfile() {
    ITfInputProcessorProfileMgr* mgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&mgr)))) {
        mgr->UnregisterProfile(CLSID_Akkhara, LANGID_KHMER, GUID_Profile, 0);
        mgr->Release();
    }
}

HRESULT RegisterCategories(bool add) {
    ITfCategoryMgr* cat = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr,
                                  reinterpret_cast<void**>(&cat));
    if (FAILED(hr)) return hr;
    for (const GUID* g : kCategories) {
        HRESULT r = add ? cat->RegisterCategory(CLSID_Akkhara, *g, CLSID_Akkhara)
                        : cat->UnregisterCategory(CLSID_Akkhara, *g, CLSID_Akkhara);
        if (FAILED(r) && add) hr = r;
    }
    cat->Release();
    return hr;
}

struct ComInit {
    HRESULT hr;
    ComInit() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComInit() { if (SUCCEEDED(hr)) CoUninitialize(); }
};
}  // namespace

HRESULT RegisterServer() {
    ComInit com;
    HRESULT hr = RegisterComClass();
    if (SUCCEEDED(hr)) hr = RegisterProfile();
    if (SUCCEEDED(hr)) hr = RegisterCategories(true);
    if (FAILED(hr)) {
        RegisterCategories(false);
        UnregisterProfile();
        UnregisterComClass();
    }
    return hr;
}

HRESULT UnregisterServer() {
    ComInit com;
    RegisterCategories(false);
    UnregisterProfile();
    UnregisterComClass();
    return S_OK;
}

}  // namespace akk
