#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include <functional>
#include <string>
#include <vector>

#include "../markdown/block_tree.h"

namespace Pluma::Outline {

struct Heading {
    int line = 0;  // 1-based source line
    int level = 1; // 1..6
    std::wstring title;
};

// Top-level headings of the document, in order (at most `maxItems`).
std::vector<Heading> CollectHeadings(const Markdown::BlockTree& tree, size_t maxItems = 5000);

// Index of the heading whose section contains `line` (the last heading at or above it), or -1.
int FindSectionIndex(const std::vector<Heading>& headings, int line);

// Left side panel with the document outline: a caption bar and a tree of headings.
class OutlinePanel {
public:
    OutlinePanel() = default;
    ~OutlinePanel();

    OutlinePanel(const OutlinePanel&) = delete;
    OutlinePanel& operator=(const OutlinePanel&) = delete;

    bool Create(HWND parent, HINSTANCE hInstance);
    HWND GetHwnd() const noexcept { return m_hwnd; }

    void SetBounds(int x, int y, int width, int height);
    void SetDarkMode(bool dark);
    void SetDpi(UINT dpi);
    void Focus();

    // Updates the tree. Titles are edited in place when only they changed, so typing
    // inside a heading does not collapse or scroll the tree.
    void SetHeadings(std::vector<Heading> headings);

    // Selects the section that contains the caret line.
    void HighlightLine(int line);

    // line, focusEditor: a heading was chosen with the mouse (focus goes to the editor) or keyboard.
    void SetOnNavigate(std::function<void(int line, bool focusEditor)> callback) {
        m_onNavigate = std::move(callback);
    }
    void SetOnClose(std::function<void()> callback) { m_onClose = std::move(callback); }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleTreeNotify(NMHDR* hdr);

    void Rebuild();
    void ApplyColors();
    void UpdateFonts();
    void Paint(HDC hdc);
    int CaptionHeight() const;
    RECT CloseButtonRect() const;
    int ItemIndex(HTREEITEM item) const;
    void Navigate(int index, bool focusEditor);

    HWND m_hwnd = nullptr;
    HWND m_tree = nullptr;
    HINSTANCE m_hInstance = nullptr;
    UINT m_dpi = 96;
    bool m_dark = false;
    HFONT m_font = nullptr;
    HFONT m_captionFont = nullptr;
    HFONT m_iconFont = nullptr;
    bool m_closeHot = false;
    bool m_closePressed = false;
    bool m_trackingMouse = false;
    bool m_selecting = false; // Programmatic selection in progress

    std::vector<Heading> m_headings;
    std::vector<HTREEITEM> m_items; // Parallel to m_headings
    HTREEITEM m_placeholder = nullptr;
    int m_selectedIndex = -1;
    int m_lastLine = 0;

    std::function<void(int, bool)> m_onNavigate;
    std::function<void()> m_onClose;
};

} // namespace Pluma::Outline
