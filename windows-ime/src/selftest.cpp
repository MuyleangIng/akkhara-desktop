// Self-test for the installed/registered Akkhara keyboard:
// opens a small window with a text box, switches THIS app to Akkhara, types
// "nh jg tv sala rean" + Enter with simulated key presses, and checks that the box then
// contains Khmer. Exit code 0 = it worked.
//
//   akkhara_selftest.exe            (after regsvr32 AkkharaIME.dll)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <msctf.h>

#include <cstdio>
#include <string>

#include "globals.h"

namespace {
HWND g_edit = nullptr;

LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) PostQuitMessage(0);
    if (msg == WM_SETFOCUS && g_edit) SetFocus(g_edit);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Pump(DWORD ms) {
    DWORD end = GetTickCount() + ms;
    while (GetTickCount() < end) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(10);
    }
}

void Key(WORD vk, bool up) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = WORD(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &in, sizeof(in));
}

void Type(const char* text) {
    for (const char* p = text; *p; ++p) {
        WORD vk = (*p == ' ') ? VK_SPACE : WORD(toupper(*p));
        Key(vk, false);
        Key(vk, true);
        Pump(60);
    }
}

std::wstring EditText() {
    wchar_t buf[512] = {};
    GetWindowTextW(g_edit, buf, 512);
    return buf;
}

void PrintUtf8(const char* label, const std::wstring& w) {
    char buf[2048] = {};
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, buf, sizeof(buf), nullptr, nullptr);
    std::printf("%s%s\n", label, buf);
}
}  // namespace

int main() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = Proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"AkkharaSelfTest";
    RegisterClassW(&wc);
    HWND main = CreateWindowW(L"AkkharaSelfTest", L"Akkhara self-test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100,
                              600, 200, nullptr, nullptr, inst, nullptr);
    g_edit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 10, 560, 40, main,
                           nullptr, inst, nullptr);
    SetForegroundWindow(main);
    SetFocus(g_edit);
    Pump(300);

    // switch this app to Akkhara
    ITfInputProcessorProfileMgr* mgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&mgr));
    if (SUCCEEDED(hr)) {
        hr = mgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, akk::LANGID_KHMER, akk::CLSID_Akkhara,
                                  akk::GUID_Profile, nullptr, TF_IPPMF_FORPROCESS | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE);
        mgr->Release();
    }
    std::printf("activate Akkhara: hr=0x%08lx\n", hr);
    if (FAILED(hr)) return 2;
    Pump(3000);  // the model loads in the background

    Type("nh jg tv sala rean");
    Pump(2500);  // suggestions arrive
    PrintUtf8("while typing: ", EditText());
    Key(VK_RETURN, false);
    Key(VK_RETURN, true);
    Pump(800);

    std::wstring result = EditText();
    PrintUtf8("after Enter:  ", result);
    const std::wstring expected = L"\x1781\x17D2\x1789\x17BB\x17C6\x1785\x1784\x17CB\x1791\x17C5\x179F\x17B6\x179B\x17B6\x179A\x17C0\x1793";  // ខ្ញុំចង់ទៅសាលារៀន
    bool ok = result == expected;
    std::printf(ok ? "PASS: Khmer inserted\n" : "FAIL: expected ខ្ញុំចង់ទៅសាលារៀន\n");
    DestroyWindow(main);
    CoUninitialize();
    return ok ? 0 : 1;
}
