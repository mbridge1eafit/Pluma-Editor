#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Pluma::Explorer {

// A subfolder or a Markdown file shown by the explorer.
struct FolderEntry {
    std::filesystem::path path;
    bool isFolder = false;
};

// .md, .markdown and .mdown, in any letter case.
bool IsMarkdownFile(const std::filesystem::path& path);

// The subfolders and then the Markdown files of `folder`, each group in the natural order of the
// Windows Explorer ("2" before "10", ignoring case). Hidden entries and version control folders
// (.git, .svn, .hg) are left out. Returns nullopt when the folder cannot be read.
std::optional<std::vector<FolderEntry>> ListFolder(const std::filesystem::path& folder);

// Whether `path` is `folder` or lies anywhere below it. Names compare case-insensitively, as in Windows.
bool IsWithinFolder(const std::filesystem::path& path, const std::filesystem::path& folder);

// The folders from the top of `folder`'s drive or network share down to `folder` itself:
// "C:\a\b" -> {"C:\", "C:\a", "C:\a\b"}, "\\server\share\a" -> {"\\server\share", "\\server\share\a"}.
std::vector<std::filesystem::path> FolderChain(const std::filesystem::path& folder);

// Index of the first breadcrumb segment shown when segments `widths` pixels wide, joined by
// `separator` pixels, must fit in `available` pixels. The last segment is always shown; when not
// all of them fit, the leading ones collapse into an overflow button `overflow` pixels wide.
size_t FirstVisibleSegment(std::span<const int> widths, int separator, int overflow, int available);

// Left side panel to browse Markdown files: a caption bar (open folder, refresh, close), a path bar
// with the folders above the root (a click goes up to one of them) and a tree with the subfolders
// and Markdown files of the root folder. Each folder is read when it is first expanded, and nothing
// is read while the panel is hidden.
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

    // The folder at the top of the tree. Folders that were expanded stay expanded while they are
    // inside the root, and going up expands the way down to the previous root.
    void SetRootFolder(const std::filesystem::path& folder);
    const std::filesystem::path& GetRootFolder() const noexcept { return m_root; }

    // Reads the root and the expanded folders again, keeping the selection and the scroll position.
    void Refresh();

    // The open document, shown in bold. A new document inside the root is revealed (its folders are
    // expanded); one outside the root makes its folder the root. An empty path keeps the root.
    void SetCurrentFile(const std::filesystem::path& file);

    // Shows the folder picker. Returns true when a folder was chosen (it becomes the root).
    bool ChooseRootFolder();

    // A file was double-clicked, activated with Enter or opened from the context menu.
    void SetOnOpenFile(std::function<void(const std::filesystem::path&)> callback) {
        m_onOpenFile = std::move(callback);
    }
    void SetOnClose(std::function<void()> callback) { m_onClose = std::move(callback); }

private:
    // Owned by its tree item (lParam) and freed on TVN_DELETEITEM. Placeholder items have none.
    struct Node {
        std::filesystem::path path;
        bool isFolder = false;
        bool loaded = false; // The folder's entries were inserted
    };

    // A folder of the path bar, from the top of the drive down to the root.
    struct Segment {
        std::filesystem::path path;
        std::wstring name;
        int width = 0; // Text plus padding, in pixels
        RECT rc{};     // Empty when collapsed into the overflow button
    };

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TreeSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR id, DWORD_PTR refData);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleTreeNotify(NMHDR* hdr);

    bool IsShown() const;
    void Rebuild();
    void FillRoot();
    void InsertPlaceholder(const wchar_t* text, int image);
    void InsertEntries(HTREEITEM parent, const std::vector<FolderEntry>& entries);
    void EnsureLoaded(HTREEITEM item, Node& node);
    void Relist(HTREEITEM item);
    void ExpandItem(HTREEITEM item);
    void ToggleItem(HTREEITEM item);
    Node* NodeOf(HTREEITEM item) const;
    std::filesystem::path PathOfItem(HTREEITEM item) const;
    HTREEITEM FindChild(HTREEITEM parent, const std::wstring& name) const;
    HTREEITEM FindItem(const std::filesystem::path& path, bool expand, bool* folderShown = nullptr);
    bool RevealCurrentFile(bool expand);
    void ShowCurrentFile();
    void SetBoldItem(HTREEITEM item);
    void AddExpanded(const std::filesystem::path& folder);
    void RemoveExpanded(const std::filesystem::path& folder);
    bool IsExpandedFolder(const std::filesystem::path& folder) const;

    void Activate(HTREEITEM item);
    void GoUp();
    void ShowItemMenu(LPARAM screenPoint);
    void ShowOverflowMenu();

    void UpdateSegments();
    void MeasureSegments();
    void LayoutBars();
    void UpdateImages();
    // Makes the icons and the tooltips, which are only needed once the panel is on screen.
    void PrepareForDisplay();
    void UpdatePathTooltip();
    void ApplyColors();
    void UpdateFonts();
    void Paint(HDC hdc);
    int CaptionHeight() const;
    int BarsHeight() const;
    int HitTest(POINT pt) const;
    bool IsEnabled(int hit) const;
    void Execute(int hit);

    HWND m_hwnd = nullptr;
    HWND m_tree = nullptr;
    HWND m_tooltip = nullptr;
    HINSTANCE m_hInstance = nullptr;
    UINT m_dpi = 96;
    bool m_dark = false;
    HFONT m_font = nullptr;
    HFONT m_captionFont = nullptr;
    HFONT m_iconFont = nullptr;
    HFONT m_chevronFont = nullptr;
    HIMAGELIST m_images = nullptr;
    int m_hot = -1;     // Caption or path bar target under the mouse
    int m_pressed = -1; // Target pressed with the mouse
    bool m_trackingMouse = false;
    bool m_choosing = false; // The folder picker is open
    bool m_prepared = false; // PrepareForDisplay ran

    // Caption and path bar layout
    RECT m_openRc{};
    RECT m_refreshRc{};
    RECT m_closeRc{};
    RECT m_upRc{};
    RECT m_overflowRc{};
    int m_overflowWidth = 0;
    std::vector<Segment> m_segments;
    size_t m_firstSegment = 0; // Earlier segments are in the overflow menu
    std::wstring m_tooltipText;

    std::filesystem::path m_root;
    std::filesystem::path m_currentFile;
    std::vector<std::filesystem::path> m_expanded; // Folders left expanded, also outside the current root
    HTREEITEM m_currentItem = nullptr;             // Bold item of the open document
    HTREEITEM m_placeholder = nullptr;
    bool m_stale = false;                          // Changed while hidden: rebuild when shown
    std::filesystem::path m_pendingOpen;

    std::function<void(const std::filesystem::path&)> m_onOpenFile;
    std::function<void()> m_onClose;
};

} // namespace Pluma::Explorer
