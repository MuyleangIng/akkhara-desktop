#include "candidate_window.h"

#include <algorithm>

namespace akk {
namespace {
constexpr wchar_t kClass[] = L"AkkharaCandidateWindow";
constexpr COLORREF kBg = RGB(0xFF, 0xFF, 0xFF), kBorder = RGB(0xD0, 0xD7, 0xDE), kInk = RGB(0x1B, 0x1F, 0x24),
                   kMuted = RGB(0x6B, 0x72, 0x80), kSel = RGB(0xE3, 0xF6, 0xEE), kAccent = RGB(0x1B, 0xAF, 0x7A);

int Px(int v, int dpi) { return MulDiv(v, dpi, 96); }

// Khmer fonts that ship with Windows 10/11 (first one found is used)
const wchar_t* const kKhmerFonts[] = {L"Khmer UI", L"Leelawadee UI", L"Noto Sans Khmer", L"DaunPenh"};

int CALLBACK FontExists(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM found) {
    *reinterpret_cast<bool*>(found) = true;
    return 0;
}
}  // namespace

bool CandidateWindow::Create(std::function<void(int)> onClick, std::function<void(UINT, LPARAM)> onMessage) {
    onClick_ = std::move(onClick);
    onMessage_ = std::move(onMessage);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW | CS_IME;
        wc.lpfnWndProc = Proc;
        wc.hInstance = g_module;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.lpszClassName = kClass;
        registered = RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kClass, L"", WS_POPUP, 0, 0, 10,
                            10, nullptr, nullptr, g_module, this);
    if (!hwnd_) return false;
    HDC dc = GetDC(hwnd_);
    scale_ = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(hwnd_, dc);
    MakeFonts();
    return true;
}

void CandidateWindow::MakeFonts() {
    const wchar_t* face = L"Khmer UI";
    HDC dc = GetDC(nullptr);
    for (const wchar_t* f : kKhmerFonts) {
        LOGFONTW lf{};
        lf.lfCharSet = DEFAULT_CHARSET;
        wcsncpy_s(lf.lfFaceName, f, _TRUNCATE);
        bool found = false;
        EnumFontFamiliesExW(dc, &lf, FontExists, reinterpret_cast<LPARAM>(&found), 0);
        if (found) { face = f; break; }
    }
    ReleaseDC(nullptr, dc);
    khmer_ = CreateFontW(-Px(20, scale_), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    small_ = CreateFontW(-Px(12, scale_), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

void CandidateWindow::Destroy() {
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    if (khmer_) DeleteObject(khmer_);
    if (small_) DeleteObject(small_);
    khmer_ = small_ = nullptr;
    visible_ = false;
}

void CandidateWindow::SetItems(const std::vector<std::wstring>& items, int selected, const std::wstring& status) {
    items_ = items;
    selected_ = selected;
    status_ = status;
    Layout();
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void CandidateWindow::SetAnchor(const RECT& r) {
    anchor_ = r;
    Layout();
}

void CandidateWindow::Show(bool show) {
    if (!hwnd_) return;
    show = show && (!items_.empty() || !status_.empty());
    if (show == visible_) {
        if (show) InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    visible_ = show;
    ShowWindow(hwnd_, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void CandidateWindow::Layout() {
    if (!hwnd_) return;
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, khmer_);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    rowH_ = tm.tmHeight + Px(10, scale_);
    int w = Px(240, scale_);
    for (size_t i = 0; i < items_.size(); ++i) {
        std::wstring row = std::to_wstring(i + 1) + L"   " + items_[i];
        SIZE sz{};
        GetTextExtentPoint32W(dc, row.c_str(), int(row.size()), &sz);
        w = std::max(w, int(sz.cx) + Px(32, scale_));
    }
    SelectObject(dc, small_);
    GetTextMetricsW(dc, &tm);
    footerH_ = tm.tmHeight + Px(10, scale_);
    SIZE sz{};
    GetTextExtentPoint32W(dc, status_.c_str(), int(status_.size()), &sz);
    w = std::max(w, int(sz.cx) + Px(24, scale_));
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);

    width_ = std::min(w, Px(900, scale_));
    int h = int(items_.size()) * rowH_ + footerH_ + Px(8, scale_);

    // below the text; above it when there is no room; always inside the screen
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromRect(&anchor_, MONITOR_DEFAULTTONEAREST), &mi);
    const RECT& work = mi.rcWork;
    int x = std::clamp(int(anchor_.left), int(work.left), std::max(int(work.left), int(work.right) - width_));
    int y = anchor_.bottom + Px(4, scale_);
    if (y + h > work.bottom) y = anchor_.top - h - Px(4, scale_);
    y = std::max(y, int(work.top));
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width_, h, SWP_NOACTIVATE);
}

int CandidateWindow::RowAt(int y) const {
    int i = (y - Px(4, scale_)) / std::max(1, rowH_);
    return (i >= 0 && i < int(items_.size())) ? i : -1;
}

void CandidateWindow::Paint(HDC target) {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    // draw into a bitmap first (no flicker)
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bmp = CreateCompatibleBitmap(target, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    HBRUSH bg = CreateSolidBrush(kBg);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    int y = Px(4, scale_);
    HGDIOBJ oldFont = SelectObject(dc, khmer_);
    for (size_t i = 0; i < items_.size(); ++i) {
        RECT row{0, y, rc.right, y + rowH_};
        if (int(i) == selected_) {
            HBRUSH sel = CreateSolidBrush(kSel);
            FillRect(dc, &row, sel);
            DeleteObject(sel);
            RECT bar{0, y, Px(3, scale_), y + rowH_};
            HBRUSH acc = CreateSolidBrush(kAccent);
            FillRect(dc, &bar, acc);
            DeleteObject(acc);
        }
        RECT num{Px(10, scale_), y, Px(30, scale_), y + rowH_};
        SetTextColor(dc, kMuted);
        std::wstring n = std::to_wstring(i + 1);
        DrawTextW(dc, n.c_str(), -1, &num, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        RECT txt{Px(30, scale_), y, rc.right - Px(8, scale_), y + rowH_};
        SetTextColor(dc, kInk);
        DrawTextW(dc, items_[i].c_str(), -1, &txt, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
        y += rowH_;
    }
    SelectObject(dc, small_);
    SetTextColor(dc, kMuted);
    RECT foot{Px(10, scale_), y, rc.right - Px(8, scale_), y + footerH_};
    DrawTextW(dc, status_.c_str(), -1, &foot, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, oldFont);

    HBRUSH border = CreateSolidBrush(kBorder);
    FrameRect(dc, &rc, border);
    DeleteObject(border);

    BitBlt(target, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

LRESULT CALLBACK CandidateWindow::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* self = reinterpret_cast<CandidateWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;  // never steal the focus from the app
        case WM_LBUTTONDOWN:
            if (self) {
                int i = self->RowAt(int(short(HIWORD(lp))));
                if (i >= 0 && self->onClick_) self->onClick_(i);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (self) self->Paint(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        default:
            if (msg >= WM_APP && self && self->onMessage_) {
                self->onMessage_(msg, lp);
                return 0;
            }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace akk
