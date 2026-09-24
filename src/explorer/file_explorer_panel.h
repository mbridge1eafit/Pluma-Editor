#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include <filesystem>
#include <functional>
#include <vector>

namespace Pluma::Explorer {

// Left side panel with a flat list of the Markdown files (*.md, *.markdown, *.mdown) found in
// a single folder: a caption bar (with refresh/close buttons) and a tree used as a plain list.
class FileExplorerPanel {
public:
    FileExplorerPanel() = default;
    ~FileExplorerPanel();

    FileExplorerPanel(const FileExplorerPanel&) = delete;
    FileExplorerPanel& operator=(const FileExplorerPanel&) = delete;

    bool Create(HWND parent, HINSTANCE hInstance);
    HWND GetHwnd() const noexcept { return m_hwnd; }

    void SetBounds(int x, int y, int width, int height);
    void SetDarkMode(bool dark);
    void SetDpi(UINT dpi);
    void Focus();

    // Rescans `folder` and rebuilds the list. A no-op when it did not change (use Refresh() to
    // force a rescan, e.g. after a save that may have added a file).
    void SetFolder(const std::filesystem::path& folder);
    void Refresh();

    // Highlights `file` in the list, switching folders first if needed.
    void SetCurrentFile(const std::filesystem::path& file);

    // A file was double-clicked or activated with Enter.
    void SetOnOpenFile(std::function<void(const std::filesystem::path&)> callback) {
        m_onOpenFile = std::move(callback);
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
    RECT RefreshButtonRect() const;
    int ItemIndex(HTREEITEM item) const;
    void OpenIndex(int index);

    HWND m_hwnd = nullptr;
    HWND m_tree = nullptr;
    HINSTANCE m_hInstance = nullptr;
    UINT m_dpi = 96;
    bool m_dark = false;
    HFONT m_font = nullptr;
    HFONT m_captionFont = nullptr;
    bool m_closeHot = false;
    bool m_closePressed = false;
    bool m_refreshHot = false;
    bool m_refreshPressed = false;
    bool m_trackingMouse = false;

    std::filesystem::path m_folder;
    std::filesystem::path m_currentFile;
    std::vector<std::filesystem::path> m_files; // Sorted by filename, parallel to m_items
    std::vector<HTREEITEM> m_items;
    HTREEITEM m_placeholder = nullptr;

    std::function<void(const std::filesystem::path&)> m_onOpenFile;
    std::function<void()> m_onClose;
};

} // namespace Pluma::Explorer
