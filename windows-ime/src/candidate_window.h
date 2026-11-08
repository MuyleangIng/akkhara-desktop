// The small suggestion box under the text you are typing (top-5 Khmer).
// It never takes the focus from the app; click a row to insert it.
#pragma once

#include "globals.h"

#include <functional>
#include <string>
#include <vector>

namespace akk {

class CandidateWindow {
public:
    // onClick(index) when a row is clicked; onMessage(msg, lParam) for engine results.
    bool Create(std::function<void(int)> onClick, std::function<void(UINT, LPARAM)> onMessage);
    void Destroy();
    HWND Hwnd() const { return hwnd_; }

    void SetItems(const std::vector<std::wstring>& items, int selected, const std::wstring& status);
    void SetAnchor(const RECT& screenRect);  // the text being typed, in screen coordinates
    void Show(bool show);
    bool Visible() const { return visible_; }

private:
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
    void Layout();
    void Paint(HDC hdc);
    int RowAt(int y) const;
    void MakeFonts();

    HWND hwnd_ = nullptr;
    std::function<void(int)> onClick_;
    std::function<void(UINT, LPARAM)> onMessage_;
    std::vector<std::wstring> items_;
    std::wstring status_;
    int selected_ = 0;
    RECT anchor_{};
    bool visible_ = false;
    HFONT khmer_ = nullptr, small_ = nullptr;
    int scale_ = 96;  // DPI
    int rowH_ = 0, footerH_ = 0, width_ = 0;
};

}  // namespace akk
