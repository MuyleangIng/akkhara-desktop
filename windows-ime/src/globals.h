// Akkhara Windows keyboard (a TSF "text input processor"): shared ids and helpers.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <msctf.h>
#include <olectl.h>

#include <atomic>
#include <string>

namespace akk {

// {57257CD7-B2DD-47D1-94E1-2CCC9766838A}  the COM class of the keyboard
inline constexpr CLSID CLSID_Akkhara = {0x57257cd7, 0xb2dd, 0x47d1, {0x94, 0xe1, 0x2c, 0xcc, 0x97, 0x66, 0x83, 0x8a}};
// {0F2A00C4-EC31-49B5-AA40-89D73AE7F83B}  the language profile "Akkhara (Sing Khmer)" under Khmer
inline constexpr GUID GUID_Profile = {0x0f2a00c4, 0xec31, 0x49b5, {0xaa, 0x40, 0x89, 0xd7, 0x3a, 0xe7, 0xf8, 0x3b}};
// {92D7890E-8B74-4B3D-BA01-78131560E8C6}  the underline shown under the Latin while typing
inline constexpr GUID GUID_DisplayAttributeInput = {0x92d7890e, 0x8b74, 0x4b3d, {0xba, 0x01, 0x78, 0x13, 0x15, 0x60, 0xe8, 0xc6}};

inline constexpr LANGID LANGID_KHMER = MAKELANGID(LANG_KHMER, SUBLANG_KHMER_CAMBODIA);  // 0x0453
inline constexpr wchar_t DESCRIPTION[] = L"Akkhara (Sing Khmer)";
inline constexpr int IDI_AKKHARA = 101;  // icon resource id (akkhara.rc)

extern HINSTANCE g_module;           // this DLL
extern std::atomic<long> g_refs;     // live objects + server locks (DllCanUnloadNow)

inline void DllAddRef() { ++g_refs; }
inline void DllRelease() { --g_refs; }

// Folder of this DLL, e.g. C:\Program Files\Akkhara\x64\ (with the trailing backslash).
std::wstring ModuleDir();

}  // namespace akk
