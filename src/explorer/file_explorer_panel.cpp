#include "file_explorer_panel.h"

#include <shlobj.h>   // SHOpenFolderAndSelectItems
#include <shobjidl.h> // IFileOpenDialog
#include <uxtheme.h>
#include <windowsx.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string_view>
#include <utility>

#include "../platform/dpi.h"
#include "../platform/icon_font.h"

namespace Pluma::Explorer {

namespace {

constexpr wchar_t kPanelClassName[] = L"PlumaFileExplorerPanelClass";
// Work requested from tree notifications, posted so that it runs once the tree finished handling
// the click or key (opening a file can show a message box; going up rebuilds the whole tree).
constexpr UINT WM_APP_ACTION = WM_APP + 20; // wParam = Action
constexpr int kTreeControlId = 1;
constexpr UINT_PTR kTreeSubclassId = 1;
constexpr UINT kExpandActionMask = TVE_COLLAPSE | TVE_EXPAND; // Of NMTREEVIEW::action

enum class Action : WPARAM { Activate, OpenPending, GoUp, Refresh, ChooseFolder, PrepareForDisplay };

// Logical pixels (96 DPI)
constexpr int kCaptionHeight = 32;
constexpr int kPathBarHeight = 28;
constexpr int kButtonSize = 24;
constexpr int kSegmentHeight = 22;
constexpr int kSegmentPadding = 5;
constexpr int kSeparatorWidth = 14;
constexpr int kTreeIconSize = 16;
constexpr int kTreeItemHeight = 24;

// Caption and path bar targets; the breadcrumb segment i is kHitSegment + i.
constexpr int kHitNone = -1;
constexpr int kHitOpenFolder = 0;
constexpr int kHitRefresh = 1;
constexpr int kHitClose = 2;
constexpr int kHitUp = 3;
constexpr int kHitOverflow = 4;
constexpr int kHitSegment = 100;
constexpr UINT_PTR kToolPath = 10; // Tooltip of the breadcrumb: the root's full path

// Tree images
constexpr int kImageFolder = 0;
constexpr int kImageFolderOpen = 1;
constexpr int kImageFile = 2;
constexpr int kImageNone = 3; // Transparent

constexpr wchar_t kOverflowText[] = L"…";

struct Palette {
    COLORREF background;
    COLORREF text;
    COLORREF mutedText;
    COLORREF disabledText;
    COLORREF buttonHot;
    COLORREF buttonPressed;
    COLORREF link;
    COLORREF folderIcon;
    COLORREF fileIcon;
};

Palette GetPalette(bool dark) {
    if (dark) {
        return {RGB(37, 37, 38), RGB(212, 212, 212), RGB(145, 145, 145), RGB(90, 90, 92), RGB(62, 62, 64),
                RGB(75, 75, 78), RGB(96, 165, 230), RGB(224, 184, 92), RGB(128, 172, 216)};
    }
    return {RGB(246, 248, 250), RGB(36, 41, 47), RGB(101, 109, 118), RGB(184, 189, 196), RGB(226, 229, 233),
            RGB(210, 214, 219), RGB(0, 95, 184), RGB(204, 146, 28), RGB(66, 122, 184)};
}

bool IsSeparator(wchar_t c) {
    return c == L'\\' || c == L'/';
}

// Windows file names compare case-insensitively.
bool SameName(std::wstring_view a, std::wstring_view b) {
    if (a.size() != b.size()) return false;
    if (a.empty()) return true;
    return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) ==
           CSTR_EQUAL;
}

// The order of the Windows Explorer: digits compare as numbers ("2" before "10") and case is ignored.
bool NaturalLess(const std::wstring& a, const std::wstring& b) {
    const int order = CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS, a.c_str(),
                                      static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), nullptr,
                                      nullptr, 0);
    if (order == CSTR_LESS_THAN) return true;
    if (order == CSTR_GREATER_THAN) return false;
    return a < b; // Equal for the locale ("1" and "01"): still a strict order
}

// Repository metadata, never worth browsing. Git hides .git, but other tools do not.
bool IsVersionControlFolder(std::wstring_view name) {
    return SameName(name, L".git") || SameName(name, L".svn") || SameName(name, L".hg");
}

// "\" separators, no "." or ".." parts and no trailing separator, except at the top ("C:\").
std::filesystem::path NormalizePath(const std::filesystem::path& path) {
    if (path.empty()) return {};
    std::filesystem::path normal = path.lexically_normal();
    if (!normal.has_filename() && normal.has_relative_path()) normal = normal.parent_path();
    return normal;
}

bool SamePath(const std::filesystem::path& a, const std::filesystem::path& b) {
    return SameName(a.native(), b.native());
}

// "C:\docs\notes" -> {"C:", "docs", "notes"}
std::vector<std::wstring> PathElements(const std::filesystem::path& path) {
    std::vector<std::wstring> elements;
    for (const auto& part : path.lexically_normal()) {
        std::wstring name = part.wstring();
        if (!name.empty() && !(name.size() == 1 && IsSeparator(name[0]))) elements.push_back(std::move(name));
    }
    return elements;
}

// "\\server" (a computer on the network), not "\\?\C:" (a path prefix).
bool IsNetworkServer(const std::filesystem::path& path) {
    const std::wstring name = path.root_name().wstring();
    return name.size() > 2 && IsSeparator(name[0]) && IsSeparator(name[1]) && name[2] != L'?' && name[2] != L'.' &&
           !path.has_relative_path();
}

// Name of the first folder of a chain: "C:\" -> "C:", "\\server\share" stays as is.
std::wstring TopFolderName(const std::filesystem::path& folder) {
    std::wstring name = folder.wstring();
    while (name.size() > 1 && IsSeparator(name.back())) name.pop_back();
    return name;
}

// Menus take "&" as the prefix of the access key.
std::wstring MenuLabel(std::wstring_view text) {
    std::wstring label;
    for (const wchar_t c : text) {
        if (c == L'&') label += L'&';
        label += c;
    }
    return label;
}

void PostAction(HWND hwnd, Action action) {
    PostMessageW(hwnd, WM_APP_ACTION, static_cast<WPARAM>(action), 0);
}

// Opens the folder that contains `path` in the Windows Explorer, with `path` selected.
void ShowInExplorer(const std::filesystem::path& path) {
    if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(path.c_str())) {
        SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
    }
}

void CopyToClipboard(HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        void* target = GlobalLock(memory);
        if (target) {
            std::memcpy(target, text.c_str(), bytes);
            GlobalUnlock(memory);
        }
        if (!target || !SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    }
    CloseClipboard();
}

void FillRounded(HDC hdc, const RECT& rc, int radius, COLORREF fill) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, fill);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

} // namespace

bool IsMarkdownFile(const std::filesystem::path& path) {
    const std::wstring ext = path.extension().wstring();
    return SameName(ext, L".md") || SameName(ext, L".markdown") || SameName(ext, L".mdown");
}

std::optional<std::vector<FolderEntry>> ListFolder(const std::filesystem::path& folder) {
    if (folder.empty()) return std::nullopt;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileExW((folder / L"*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr,
                                   FIND_FIRST_EX_LARGE_FETCH);
    if (find == INVALID_HANDLE_VALUE) {
        // The top of an empty drive has no "." entry: nothing matched, but the folder exists.
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return std::vector<FolderEntry>{};
        return std::nullopt;
    }
    std::vector<std::wstring> folders;
    std::vector<std::wstring> files;
    do {
        const std::wstring_view name = data.cFileName;
        if (name == L"." || name == L".." || (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!IsVersionControlFolder(name)) folders.emplace_back(name);
        } else if (IsMarkdownFile(std::filesystem::path(name))) {
            files.emplace_back(name);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);

    std::sort(folders.begin(), folders.end(), NaturalLess);
    std::sort(files.begin(), files.end(), NaturalLess);
    std::vector<FolderEntry> entries;
    entries.reserve(folders.size() + files.size());
    for (const std::wstring& name : folders) entries.push_back({folder / name, true});
    for (const std::wstring& name : files) entries.push_back({folder / name, false});
    return entries;
}

bool IsWithinFolder(const std::filesystem::path& path, const std::filesystem::path& folder) {
    if (path.empty() || folder.empty()) return false;
    const std::vector<std::wstring> inner = PathElements(path);
    const std::vector<std::wstring> outer = PathElements(folder);
    if (outer.empty() || inner.size() < outer.size()) return false;
    for (size_t i = 0; i < outer.size(); ++i) {
        if (!SameName(inner[i], outer[i])) return false;
    }
    return true;
}

std::vector<std::filesystem::path> FolderChain(const std::filesystem::path& folder) {
    std::vector<std::filesystem::path> chain;
    std::filesystem::path current = NormalizePath(folder);
    while (!current.empty()) {
        chain.push_back(current);
        std::filesystem::path parent = current.parent_path();
        // "C:\" is its own parent; a network server ("\\server\") is not a folder.
        if (parent.empty() || parent == current || IsNetworkServer(parent)) break;
        current = std::move(parent);
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

size_t FirstVisibleSegment(std::span<const int> widths, int separator, int overflow, int available) {
    const size_t count = widths.size();
    if (count <= 1) return 0;
    int total = separator * static_cast<int>(count - 1);
    for (const int width : widths) total += width;
    if (total <= available) return 0;
    // The overflow button (and its separator) stands for the hidden segments. The first segment never
    // comes back: with it every segment would be shown, and they do not fit.
    size_t first = count - 1;
    int used = overflow + separator + widths[first];
    while (first > 1 && used + separator + widths[first - 1] <= available) {
        used += separator + widths[first - 1];
        --first;
    }
    return first;
}

FileExplorerPanel::~FileExplorerPanel() {
    if (m_font) DeleteObject(m_font);
    if (m_captionFont) DeleteObject(m_captionFont);
    if (m_iconFont) DeleteObject(m_iconFont);
    if (m_chevronFont) DeleteObject(m_chevronFont);
    if (m_images) ImageList_Destroy(m_images);
}

bool FileExplorerPanel::Create(HWND parent, HINSTANCE hInstance) {
    m_hInstance = hInstance;

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
        wc.lpfnWndProc = StaticWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kPanelClassName;
        if (!RegisterClassExW(&wc)) return false;
        s_registered = true;
    }

    m_hwnd = CreateWindowExW(0, kPanelClassName, L"", WS_CHILD | WS_CLIPCHILDREN,
                             0, 0, 0, 0, parent, nullptr, hInstance, this);
    if (!m_hwnd) return false;

    m_dpi = GetDpiForWindow(m_hwnd);
    if (m_dpi == 0) m_dpi = 96;

    m_tree = CreateWindowExW(0, WC_TREEVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_LINESATROOT |
                                 TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_DISABLEDRAGDROP,
                             0, 0, 0, 0, m_hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTreeControlId)), hInstance, nullptr);
    if (!m_tree) return false;
    SetWindowSubclass(m_tree, TreeSubclassProc, kTreeSubclassId, reinterpret_cast<DWORD_PTR>(this));
    TreeView_SetExtendedStyle(m_tree, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

    UpdateFonts();
    ApplyColors();
    Rebuild(); // The placeholder, until there is a folder to show
    return true;
}

void FileExplorerPanel::SetBounds(int x, int y, int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void FileExplorerPanel::SetDarkMode(bool dark) {
    m_dark = dark;
    ApplyColors();
}

void FileExplorerPanel::SetDpi(UINT dpi) {
    if (dpi == 0 || dpi == m_dpi) return;
    m_dpi = dpi;
    UpdateFonts();
    if (m_images) UpdateImages();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    SendMessageW(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void FileExplorerPanel::Focus() {
    if (m_tree) SetFocus(m_tree);
}

bool FileExplorerPanel::IsShown() const {
    return m_hwnd && (GetWindowLongPtrW(m_hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
}

void FileExplorerPanel::SetRootFolder(const std::filesystem::path& folder) {
    const std::filesystem::path root = NormalizePath(folder);
    if (SamePath(root, m_root)) return;
    const std::filesystem::path previous = m_root;
    const bool goingUp = !previous.empty() && !root.empty() && IsWithinFolder(previous, root);
    if (goingUp) {
        // Keep the way down to the previous root open, so that it is easy to see where it was.
        for (std::filesystem::path open = previous; open.has_relative_path() && !SamePath(open, root);
             open = open.parent_path()) {
            AddExpanded(open);
        }
    }
    m_root = root;
    UpdateSegments();
    if (!IsShown()) {
        m_stale = true;
        return;
    }
    Rebuild();
    if (goingUp) {
        if (const HTREEITEM item = FindItem(previous, false)) {
            TreeView_SelectItem(m_tree, item);
            TreeView_EnsureVisible(m_tree, item);
        }
    }
}

void FileExplorerPanel::Refresh() {
    if (!m_tree) return;
    if (!IsShown()) {
        m_stale = true;
        return;
    }
    const std::filesystem::path selected = PathOfItem(TreeView_GetSelection(m_tree));
    const std::filesystem::path firstVisible = PathOfItem(TreeView_GetFirstVisible(m_tree));
    Rebuild();
    if (const HTREEITEM item = FindItem(selected, false)) TreeView_SelectItem(m_tree, item);
    if (const HTREEITEM item = FindItem(firstVisible, false)) TreeView_SelectSetFirstVisible(m_tree, item);
}

void FileExplorerPanel::SetCurrentFile(const std::filesystem::path& file) {
    const std::filesystem::path current = NormalizePath(file);
    const bool changed = !SamePath(current, m_currentFile);
    m_currentFile = current;
    if (current.empty()) {
        SetBoldItem(nullptr); // A new document: the folder stays on screen
        return;
    }
    const std::filesystem::path folder = current.parent_path();
    if (m_root.empty() || !IsWithinFolder(folder, m_root)) {
        SetRootFolder(folder);
    } else if (!changed) {
        return; // Saved again
    }
    if (!IsShown()) {
        m_stale = true; // Revealed when the panel is shown
        return;
    }
    if (m_stale) Rebuild();
    ShowCurrentFile();
}

bool FileExplorerPanel::ChooseRootFolder() {
    if (m_choosing) return false; // A second click while the picker opens
    m_choosing = true;
    std::filesystem::path chosen;
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Abrir carpeta");
        Microsoft::WRL::ComPtr<IShellItem> start;
        if (!m_root.empty() && SUCCEEDED(SHCreateItemFromParsingName(m_root.c_str(), nullptr, IID_PPV_ARGS(&start)))) {
            dialog->SetFolder(start.Get());
        }
        Microsoft::WRL::ComPtr<IShellItem> result;
        PWSTR path = nullptr;
        if (SUCCEEDED(dialog->Show(GetAncestor(m_hwnd, GA_ROOT))) && SUCCEEDED(dialog->GetResult(&result)) &&
            SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            chosen = path;
            CoTaskMemFree(path);
        }
    }
    m_choosing = false;
    if (chosen.empty()) return false;
    SetRootFolder(chosen);
    return true;
}

void FileExplorerPanel::Rebuild() {
    if (!m_tree) return;
    m_stale = false;
    SendMessageW(m_tree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(m_tree); // TVN_DELETEITEM frees the nodes
    FillRoot();
    SendMessageW(m_tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_tree, nullptr, TRUE);
}

void FileExplorerPanel::FillRoot() {
    if (m_root.empty()) {
        InsertPlaceholder(L"Abrir una carpeta...", kImageFolderOpen);
        return;
    }
    const auto entries = ListFolder(m_root);
    if (!entries) {
        InsertPlaceholder(L"No se puede leer esta carpeta", kImageNone);
    } else if (entries->empty()) {
        InsertPlaceholder(L"No hay archivos Markdown", kImageNone);
    } else {
        InsertEntries(TVI_ROOT, *entries);
    }
}

void FileExplorerPanel::InsertPlaceholder(const wchar_t* text, int image) {
    std::wstring label = text;
    TVINSERTSTRUCTW ins{};
    ins.hParent = TVI_ROOT;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    ins.item.pszText = label.data();
    ins.item.lParam = 0; // No node
    ins.item.iImage = image;
    ins.item.iSelectedImage = image;
    m_placeholder = TreeView_InsertItem(m_tree, &ins);
}

void FileExplorerPanel::InsertEntries(HTREEITEM parent, const std::vector<FolderEntry>& entries) {
    TVINSERTSTRUCTW ins{};
    ins.hParent = parent;
    ins.hInsertAfter = TVI_LAST;
    ins.itemex.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_STATE;
    ins.itemex.stateMask = TVIS_BOLD;
    for (const FolderEntry& entry : entries) {
        std::wstring name = entry.path.filename().wstring();
        const bool current = !entry.isFolder && SamePath(entry.path, m_currentFile);
        auto node = std::make_unique<Node>(Node{entry.path, entry.isFolder});
        ins.itemex.pszText = name.data();
        ins.itemex.lParam = reinterpret_cast<LPARAM>(node.get());
        // A folder's icon (closed or open) is asked for when drawn: TVN_GETDISPINFO.
        ins.itemex.iImage = entry.isFolder ? I_IMAGECALLBACK : kImageFile;
        ins.itemex.iSelectedImage = ins.itemex.iImage;
        ins.itemex.cChildren = entry.isFolder ? 1 : 0; // Folders are read when expanded
        ins.itemex.state = current ? TVIS_BOLD : 0;
        const HTREEITEM item = TreeView_InsertItem(m_tree, &ins);
        if (!item) continue;
        static_cast<void>(node.release()); // Freed on TVN_DELETEITEM
        if (current) m_currentItem = item;
        if (entry.isFolder && IsExpandedFolder(entry.path)) ExpandItem(item);
    }
}

void FileExplorerPanel::EnsureLoaded(HTREEITEM item, Node& node) {
    if (node.loaded || !node.isFolder) return;
    node.loaded = true;
    const auto entries = ListFolder(node.path);
    if (entries && !entries->empty()) {
        InsertEntries(item, *entries);
        return;
    }
    // Nothing to show: the expand button goes away.
    TVITEMW tvi{};
    tvi.mask = TVIF_CHILDREN;
    tvi.hItem = item;
    tvi.cChildren = 0;
    TreeView_SetItem(m_tree, &tvi);
}

void FileExplorerPanel::Relist(HTREEITEM item) {
    if (item == TVI_ROOT) {
        Rebuild();
        return;
    }
    Node* node = NodeOf(item);
    if (!node || !node->isFolder || !node->loaded) return;
    const bool expanded = (TreeView_GetItemState(m_tree, item, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
    // Deletes the children and lets the next expansion notify again (TVIS_EXPANDEDONCE is reset).
    TreeView_Expand(m_tree, item, TVE_COLLAPSE | TVE_COLLAPSERESET);
    node->loaded = false;
    TVITEMW tvi{};
    tvi.mask = TVIF_CHILDREN;
    tvi.hItem = item;
    tvi.cChildren = 1;
    TreeView_SetItem(m_tree, &tvi);
    if (expanded) ExpandItem(item);
}

void FileExplorerPanel::ExpandItem(HTREEITEM item) {
    Node* node = NodeOf(item);
    if (!node || !node->isFolder) return;
    EnsureLoaded(item, *node);
    TreeView_Expand(m_tree, item, TVE_EXPAND);
    AddExpanded(node->path);
}

void FileExplorerPanel::ToggleItem(HTREEITEM item) {
    const Node* node = NodeOf(item);
    if (!node || !node->isFolder) return;
    if (TreeView_GetItemState(m_tree, item, TVIS_EXPANDED) & TVIS_EXPANDED) {
        TreeView_Expand(m_tree, item, TVE_COLLAPSE);
        RemoveExpanded(node->path);
    } else {
        ExpandItem(item);
    }
}

FileExplorerPanel::Node* FileExplorerPanel::NodeOf(HTREEITEM item) const {
    if (!item || item == TVI_ROOT || !m_tree) return nullptr;
    TVITEMW tvi{};
    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;
    if (!TreeView_GetItem(m_tree, &tvi)) return nullptr;
    return reinterpret_cast<Node*>(tvi.lParam);
}

std::filesystem::path FileExplorerPanel::PathOfItem(HTREEITEM item) const {
    const Node* node = NodeOf(item);
    return node ? node->path : std::filesystem::path{};
}

HTREEITEM FileExplorerPanel::FindChild(HTREEITEM parent, const std::wstring& name) const {
    HTREEITEM child = parent == TVI_ROOT ? TreeView_GetRoot(m_tree) : TreeView_GetChild(m_tree, parent);
    for (; child; child = TreeView_GetNextSibling(m_tree, child)) {
        const Node* node = NodeOf(child);
        if (node && SameName(node->path.filename().native(), name)) return child;
    }
    return nullptr;
}

HTREEITEM FileExplorerPanel::FindItem(const std::filesystem::path& path, bool expand, bool* folderShown) {
    if (folderShown) *folderShown = false;
    if (!m_tree || path.empty() || m_root.empty() || !IsWithinFolder(path, m_root)) return nullptr;
    const std::vector<std::wstring> names = PathElements(path);
    HTREEITEM parent = TVI_ROOT;
    bool freshlyListed = false; // `parent` was read during this search
    for (size_t i = PathElements(m_root).size(); i < names.size(); ++i) {
        const bool last = i + 1 == names.size();
        if (last && folderShown) *folderShown = true;
        HTREEITEM item = FindChild(parent, names[i]);
        // A folder listed before `path` was created (e.g. a document just saved there) is read again.
        // Only Markdown files are listed, so a missing document of another type is not looked for.
        if (!item && expand && !freshlyListed && (!last || IsMarkdownFile(path))) {
            Relist(parent);
            item = FindChild(parent, names[i]);
        }
        if (!item || last) return item;
        const Node* node = NodeOf(item);
        if (!node || !node->isFolder) return nullptr;
        if (expand) {
            freshlyListed = !node->loaded;
            ExpandItem(item);
        } else if (!node->loaded) {
            return nullptr;
        }
        parent = item;
    }
    return nullptr; // `path` is the root itself
}

bool FileExplorerPanel::RevealCurrentFile(bool expand) {
    if (m_currentFile.empty() || m_root.empty() || !IsWithinFolder(m_currentFile, m_root)) {
        SetBoldItem(nullptr);
        return true;
    }
    bool folderShown = false;
    const HTREEITEM item = FindItem(m_currentFile, expand, &folderShown);
    SetBoldItem(item);
    if (item && expand) {
        TreeView_SelectItem(m_tree, item);
        TreeView_EnsureVisible(m_tree, item);
    }
    return folderShown || !expand;
}

void FileExplorerPanel::ShowCurrentFile() {
    if (RevealCurrentFile(true)) return;
    // The document is in a folder the tree leaves out (a hidden folder): show that folder instead.
    m_root = NormalizePath(m_currentFile.parent_path());
    UpdateSegments();
    Rebuild();
    RevealCurrentFile(true);
}

void FileExplorerPanel::SetBoldItem(HTREEITEM item) {
    if (item == m_currentItem || !m_tree) return;
    if (m_currentItem) TreeView_SetItemState(m_tree, m_currentItem, 0, TVIS_BOLD);
    m_currentItem = item;
    if (item) TreeView_SetItemState(m_tree, item, TVIS_BOLD, TVIS_BOLD);
}

bool FileExplorerPanel::IsExpandedFolder(const std::filesystem::path& folder) const {
    return std::ranges::any_of(m_expanded, [&](const std::filesystem::path& open) { return SamePath(open, folder); });
}

void FileExplorerPanel::AddExpanded(const std::filesystem::path& folder) {
    if (!IsExpandedFolder(folder)) m_expanded.push_back(folder);
}

void FileExplorerPanel::RemoveExpanded(const std::filesystem::path& folder) {
    std::erase_if(m_expanded, [&](const std::filesystem::path& open) { return SamePath(open, folder); });
}

void FileExplorerPanel::Activate(HTREEITEM item) {
    if (!item) return;
    if (item == m_placeholder) {
        if (m_root.empty() && ChooseRootFolder()) Focus();
        return;
    }
    const Node* node = NodeOf(item);
    if (!node) return;
    if (node->isFolder) {
        ToggleItem(item);
    } else if (m_onOpenFile) {
        const std::filesystem::path path = node->path; // Opening the file can rebuild the tree
        m_onOpenFile(path);
    }
}

void FileExplorerPanel::GoUp() {
    if (m_segments.size() < 2) return;
    const std::filesystem::path parent = m_segments[m_segments.size() - 2].path; // Copied: the path bar changes
    SetRootFolder(parent);
}

void FileExplorerPanel::ShowItemMenu(LPARAM screenPoint) {
    POINT pt{GET_X_LPARAM(screenPoint), GET_Y_LPARAM(screenPoint)};
    HTREEITEM item = nullptr;
    if (screenPoint == -1) {
        // Shift+F10 or the menu key: the selected item.
        item = TreeView_GetSelection(m_tree);
        RECT rc{};
        pt = (item && TreeView_GetItemRect(m_tree, item, &rc, TRUE)) ? POINT{rc.left, rc.bottom} : POINT{0, 0};
        ClientToScreen(m_tree, &pt);
    } else {
        TVHITTESTINFO hit{};
        hit.pt = pt;
        ScreenToClient(m_tree, &hit.pt);
        item = TreeView_HitTest(m_tree, &hit);
        if (item) TreeView_SelectItem(m_tree, item);
    }
    const Node* node = NodeOf(item);
    if (!node) return;
    const std::filesystem::path path = node->path; // The chosen action can rebuild the tree
    const bool isFolder = node->isFolder;

    enum : UINT { kOpen = 1, kEnterFolder, kShowInExplorer, kCopyPath };
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    if (isFolder) {
        AppendMenuW(menu, MF_STRING, kEnterFolder, L"&Entrar en la carpeta");
    } else {
        AppendMenuW(menu, MF_STRING, kOpen, L"&Abrir");
        SetMenuDefaultItem(menu, kOpen, FALSE);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kShowInExplorer, L"&Mostrar en el Explorador de Windows");
    AppendMenuW(menu, MF_STRING, kCopyPath, L"&Copiar ruta");
    const auto command = static_cast<UINT>(TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y,
                                                             m_hwnd, nullptr));
    DestroyMenu(menu);
    switch (command) {
    case kOpen:
        if (m_onOpenFile) m_onOpenFile(path);
        break;
    case kEnterFolder:
        SetRootFolder(path);
        break;
    case kShowInExplorer:
        ShowInExplorer(path);
        break;
    case kCopyPath:
        CopyToClipboard(m_hwnd, path.wstring());
        break;
    default:
        break;
    }
}

void FileExplorerPanel::ShowOverflowMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (size_t i = 0; i < m_firstSegment; ++i) {
        AppendMenuW(menu, MF_STRING, i + 1, MenuLabel(m_segments[i].name).c_str());
    }
    POINT pt{m_overflowRc.left, m_overflowRc.bottom};
    ClientToScreen(m_hwnd, &pt);
    const auto command = static_cast<UINT>(
        TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, m_hwnd, nullptr));
    DestroyMenu(menu);
    if (command > 0 && command <= m_firstSegment) {
        const std::filesystem::path folder = m_segments[command - 1].path; // Copied: the path bar changes
        SetRootFolder(folder);
    }
}

void FileExplorerPanel::UpdateSegments() {
    m_segments.clear();
    const std::vector<std::filesystem::path> chain = FolderChain(m_root);
    for (size_t i = 0; i < chain.size(); ++i) {
        m_segments.push_back({chain[i], i == 0 ? TopFolderName(chain[i]) : chain[i].filename().wstring()});
    }
    m_hot = kHitNone;
    m_pressed = kHitNone;
    MeasureSegments();
    LayoutBars();
    UpdatePathTooltip();
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void FileExplorerPanel::MeasureSegments() {
    if (!m_hwnd || !m_font) return;
    HDC hdc = GetDC(m_hwnd);
    HGDIOBJ oldFont = SelectObject(hdc, m_font);
    const int padding = 2 * Platform::ScaleForDpi(kSegmentPadding, m_dpi);
    const auto measure = [&](std::wstring_view text) {
        SIZE size{};
        GetTextExtentPoint32W(hdc, text.data(), static_cast<int>(text.size()), &size);
        return static_cast<int>(size.cx) + padding;
    };
    for (Segment& segment : m_segments) segment.width = measure(segment.name);
    m_overflowWidth = measure(kOverflowText);
    SelectObject(hdc, oldFont);
    ReleaseDC(m_hwnd, hdc);
}

void FileExplorerPanel::LayoutBars() {
    if (!m_hwnd) return;
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const auto scale = [this](int value) { return Platform::ScaleForDpi(value, m_dpi); };

    const int captionH = CaptionHeight();
    const int button = scale(kButtonSize);
    const int margin = (captionH - button) / 2;
    const int gap = scale(2);
    m_closeRc = RECT{client.right - margin - button, margin, client.right - margin, margin + button};
    m_refreshRc = RECT{m_closeRc.left - gap - button, margin, m_closeRc.left - gap, margin + button};
    m_openRc = RECT{m_refreshRc.left - gap - button, margin, m_refreshRc.left - gap, margin + button};

    // Path bar: the up button, then the breadcrumb.
    const int barTop = captionH;
    const int barH = BarsHeight() - captionH;
    const int upTop = barTop + (barH - button) / 2;
    m_upRc = RECT{scale(6), upTop, scale(6) + button, upTop + button};

    const int segmentH = scale(kSegmentHeight);
    const int segmentTop = barTop + (barH - segmentH) / 2;
    const int separator = scale(kSeparatorWidth);
    const int right = client.right - scale(6);
    int x = m_upRc.right + scale(4);
    std::vector<int> widths;
    widths.reserve(m_segments.size());
    for (const Segment& segment : m_segments) widths.push_back(segment.width);
    m_firstSegment = FirstVisibleSegment(widths, separator, m_overflowWidth, (std::max)(0, right - x));
    m_overflowRc = RECT{};
    if (m_firstSegment > 0) {
        m_overflowRc = RECT{x, segmentTop, x + m_overflowWidth, segmentTop + segmentH};
        x = m_overflowRc.right + separator;
    }
    for (size_t i = 0; i < m_segments.size(); ++i) {
        Segment& segment = m_segments[i];
        if (i < m_firstSegment) {
            segment.rc = RECT{};
            continue;
        }
        // The root's name is cut with an ellipsis when even it alone does not fit.
        const bool last = i + 1 == m_segments.size();
        const int width = last ? (std::min)(segment.width, (std::max)(0, right - x)) : segment.width;
        segment.rc = RECT{x, segmentTop, x + width, segmentTop + segmentH};
        x += width + separator;
    }

    if (m_tooltip) {
        const std::pair<UINT_PTR, RECT> tools[] = {
            {kHitOpenFolder, m_openRc},
            {kHitRefresh, m_refreshRc},
            {kHitClose, m_closeRc},
            {kHitUp, m_upRc},
            {kHitOverflow, m_overflowRc},
            {kToolPath, RECT{m_upRc.right, barTop, client.right, barTop + barH}},
        };
        for (const auto& [id, rc] : tools) {
            TTTOOLINFOW ti{};
            ti.cbSize = sizeof(ti);
            ti.hwnd = m_hwnd;
            ti.uId = id;
            ti.rect = rc;
            SendMessageW(m_tooltip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&ti));
        }
    }
}

void FileExplorerPanel::UpdateImages() {
    if (!m_tree) return;
    const int size = Platform::ScaleForDpi(kTreeIconSize, m_dpi);
    const Palette p = GetPalette(m_dark);
    const Platform::GlyphColor glyphs[] = {
        {Platform::Glyph::kFolderClosed, p.folderIcon}, // kImageFolder
        {Platform::Glyph::kFolder, p.folderIcon},       // kImageFolderOpen
        {Platform::Glyph::kFile, p.fileIcon},           // kImageFile
        {L"", p.fileIcon},                              // kImageNone
    };
    HBITMAP mask = nullptr;
    HBITMAP strip = Platform::CreateGlyphStrip(glyphs, size, &mask);
    // The mask keeps the blank image transparent: without alpha values it would be drawn black.
    HIMAGELIST images =
        strip ? ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, static_cast<int>(std::size(glyphs)), 0) : nullptr;
    const bool added = images && ImageList_Add(images, strip, mask) == 0;
    if (strip) DeleteObject(strip);
    if (mask) DeleteObject(mask);
    if (!added) {
        if (images) ImageList_Destroy(images);
        return;
    }
    TreeView_SetImageList(m_tree, images, TVSIL_NORMAL);
    if (m_images) ImageList_Destroy(m_images);
    m_images = images;
    // Setting the images resizes the rows to fit them.
    TreeView_SetItemHeight(m_tree, Platform::ScaleForDpi(kTreeItemHeight, m_dpi));
}

void FileExplorerPanel::ApplyColors() {
    if (!m_tree) return;
    const Palette p = GetPalette(m_dark);
    SetWindowTheme(m_tree, m_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    TreeView_SetBkColor(m_tree, p.background);
    TreeView_SetTextColor(m_tree, p.text);
    if (m_tooltip) SetWindowTheme(m_tooltip, m_dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    if (m_images) UpdateImages(); // The icons are drawn in the theme's colors
    InvalidateRect(m_hwnd, nullptr, TRUE);
    InvalidateRect(m_tree, nullptr, TRUE);
}

void FileExplorerPanel::PrepareForDisplay() {
    if (m_prepared) return;
    m_prepared = true;
    UpdateImages();
    m_tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr,
                                m_hInstance, nullptr);
    if (!m_tooltip) return;
    SetWindowTheme(m_tooltip, m_dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    const std::pair<UINT_PTR, const wchar_t*> tools[] = {
        {kHitOpenFolder, L"Abrir carpeta..."},
        {kHitRefresh, L"Actualizar (F5)"},
        {kHitClose, L"Cerrar"},
        {kHitUp, L"Subir un nivel (Alt+Flecha arriba)"},
        {kHitOverflow, L"Carpetas superiores"},
        {kToolPath, L""},
    };
    for (const auto& [id, text] : tools) {
        TTTOOLINFOW ti{};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = m_hwnd;
        ti.uId = id;
        ti.lpszText = const_cast<wchar_t*>(text);
        SendMessageW(m_tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
    LayoutBars(); // The tools' rectangles
    UpdatePathTooltip();
}

void FileExplorerPanel::UpdatePathTooltip() {
    if (!m_tooltip) return;
    m_tooltipText = m_root.wstring();
    TTTOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.hwnd = m_hwnd;
    ti.uId = kToolPath;
    ti.lpszText = m_tooltipText.data();
    SendMessageW(m_tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
}

void FileExplorerPanel::UpdateFonts() {
    if (m_font) DeleteObject(m_font);
    if (m_captionFont) DeleteObject(m_captionFont);
    if (m_iconFont) DeleteObject(m_iconFont);
    if (m_chevronFont) DeleteObject(m_chevronFont);
    m_iconFont = Platform::CreateIconFont(Platform::ScaleForDpi(12, m_dpi));
    m_chevronFont = Platform::CreateIconFont(Platform::ScaleForDpi(8, m_dpi));
    m_font = CreateFontW(-MulDiv(9, static_cast<int>(m_dpi), 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_captionFont = CreateFontW(-MulDiv(8, static_cast<int>(m_dpi), 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
                                FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (m_tree) {
        SendMessageW(m_tree, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        TreeView_SetItemHeight(m_tree, Platform::ScaleForDpi(kTreeItemHeight, m_dpi));
        TreeView_SetIndent(m_tree, Platform::ScaleForDpi(12, m_dpi));
    }
    MeasureSegments();
    LayoutBars();
}

int FileExplorerPanel::CaptionHeight() const {
    return Platform::ScaleForDpi(kCaptionHeight, m_dpi);
}

int FileExplorerPanel::BarsHeight() const {
    return Platform::ScaleForDpi(kCaptionHeight + kPathBarHeight, m_dpi);
}

int FileExplorerPanel::HitTest(POINT pt) const {
    const std::pair<int, const RECT*> buttons[] = {
        {kHitOpenFolder, &m_openRc}, {kHitRefresh, &m_refreshRc}, {kHitClose, &m_closeRc},
        {kHitUp, &m_upRc},           {kHitOverflow, &m_overflowRc},
    };
    for (const auto& [hit, rc] : buttons) {
        if (PtInRect(rc, pt)) return hit;
    }
    // The last segment is the root itself: it does nothing.
    for (size_t i = m_firstSegment; i + 1 < m_segments.size(); ++i) {
        if (PtInRect(&m_segments[i].rc, pt)) return kHitSegment + static_cast<int>(i);
    }
    return kHitNone;
}

bool FileExplorerPanel::IsEnabled(int hit) const {
    if (hit == kHitNone) return false;
    return hit != kHitUp || m_segments.size() >= 2;
}

void FileExplorerPanel::Execute(int hit) {
    switch (hit) {
    case kHitOpenFolder:
        if (ChooseRootFolder()) Focus();
        break;
    case kHitRefresh:
        Refresh();
        break;
    case kHitClose:
        if (m_onClose) m_onClose();
        break;
    case kHitUp:
        GoUp();
        break;
    case kHitOverflow:
        ShowOverflowMenu();
        break;
    default:
        if (hit >= kHitSegment && static_cast<size_t>(hit - kHitSegment) < m_segments.size()) {
            const std::filesystem::path folder = m_segments[hit - kHitSegment].path; // Copied: the path bar changes
            SetRootFolder(folder);
        }
        break;
    }
}

void FileExplorerPanel::Paint(HDC hdc) {
    RECT client{};
    GetClientRect(m_hwnd, &client);
    const RECT bars{0, 0, client.right, BarsHeight()};
    if (bars.right <= 0) return;

    const Palette p = GetPalette(m_dark);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, bars.right, bars.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, bmp);

    HBRUSH bg = CreateSolidBrush(p.background);
    FillRect(memDC, &bars, bg);
    DeleteObject(bg);

    HGDIOBJ oldFont = SelectObject(memDC, m_captionFont);
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, p.mutedText);
    RECT titleRc{Platform::ScaleForDpi(12, m_dpi), 0, m_openRc.left - Platform::ScaleForDpi(4, m_dpi),
                 CaptionHeight()};
    DrawTextW(memDC, L"ARCHIVOS", -1, &titleRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

    // Hover and pressed states: a rounded highlight, as in the toolbar.
    const int radius = Platform::ScaleForDpi(6, m_dpi);
    const auto highlight = [&](const RECT& rc, int hit) {
        if (m_hot != hit) return false;
        FillRounded(memDC, rc, radius, m_pressed == hit ? p.buttonPressed : p.buttonHot);
        return true;
    };
    const auto drawButton = [&](const RECT& rc, const wchar_t* glyph, int hit) {
        const bool enabled = IsEnabled(hit);
        if (enabled) highlight(rc, hit);
        SelectObject(memDC, m_iconFont);
        Platform::DrawGlyph(memDC, glyph, rc, enabled ? p.text : p.disabledText);
    };
    drawButton(m_openRc, Platform::Glyph::kFolder, kHitOpenFolder);
    drawButton(m_refreshRc, Platform::Glyph::kRefresh, kHitRefresh);
    drawButton(m_closeRc, Platform::Glyph::kClose, kHitClose);
    drawButton(m_upRc, Platform::Glyph::kUp, kHitUp);

    // Breadcrumb: "… › PROYECTOS › Pluma", the root last.
    const int separatorW = Platform::ScaleForDpi(kSeparatorWidth, m_dpi);
    const auto drawSeparator = [&](int left) {
        const RECT rc{left, m_upRc.top, left + separatorW, m_upRc.bottom};
        SelectObject(memDC, m_chevronFont);
        Platform::DrawGlyph(memDC, Platform::Glyph::kChevronRight, rc, p.mutedText);
    };
    const auto drawText = [&](const RECT& rc, std::wstring_view text, COLORREF color) {
        RECT textRc = rc;
        InflateRect(&textRc, -Platform::ScaleForDpi(kSegmentPadding, m_dpi), 0);
        SelectObject(memDC, m_font);
        SetTextColor(memDC, color);
        DrawTextW(memDC, text.data(), static_cast<int>(text.size()), &textRc,
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX | DT_END_ELLIPSIS);
    };
    if (!IsRectEmpty(&m_overflowRc)) {
        drawText(m_overflowRc, kOverflowText, highlight(m_overflowRc, kHitOverflow) ? p.text : p.mutedText);
        drawSeparator(m_overflowRc.right);
    }
    for (size_t i = m_firstSegment; i < m_segments.size(); ++i) {
        const Segment& segment = m_segments[i];
        if (i + 1 == m_segments.size()) {
            drawText(segment.rc, segment.name, p.text);
            break;
        }
        const bool hot = highlight(segment.rc, kHitSegment + static_cast<int>(i));
        drawText(segment.rc, segment.name, hot ? p.text : p.mutedText);
        drawSeparator(segment.rc.right);
    }

    SelectObject(memDC, oldFont);
    BitBlt(hdc, 0, 0, bars.right, bars.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK FileExplorerPanel::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FileExplorerPanel* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<FileExplorerPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<FileExplorerPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->HandleMessage(msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Alt+Up goes up one folder, as in the Windows Explorer. The tree ignores system keys, so they
// never reach TVN_KEYDOWN.
LRESULT CALLBACK FileExplorerPanel::TreeSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                     UINT_PTR /*id*/, DWORD_PTR refData) {
    if (msg == WM_SYSKEYDOWN && wParam == VK_UP) {
        PostAction(reinterpret_cast<FileExplorerPanel*>(refData)->m_hwnd, Action::GoUp);
        return 0;
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(hwnd, TreeSubclassProc, kTreeSubclassId);
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT FileExplorerPanel::HandleTreeNotify(NMHDR* hdr) {
    switch (hdr->code) {
    case NM_CLICK:
    case NM_DBLCLK: {
        TVHITTESTINFO hit{};
        GetCursorPos(&hit.pt);
        ScreenToClient(m_tree, &hit.pt);
        const HTREEITEM item = TreeView_HitTest(m_tree, &hit);
        constexpr UINT kOnRow = TVHT_ONITEM | TVHT_ONITEMINDENT | TVHT_ONITEMRIGHT;
        if (!item || !(hit.flags & kOnRow) || (hit.flags & TVHT_ONITEMBUTTON)) return 0;
        if (item == m_placeholder) {
            // "Abrir una carpeta..." works like a link.
            if (hdr->code == NM_CLICK && m_root.empty()) PostAction(m_hwnd, Action::ChooseFolder);
            return 0;
        }
        // Folders expand and collapse with the tree's own double-click handling.
        const Node* node = NodeOf(item);
        if (hdr->code == NM_DBLCLK && node && !node->isFolder) {
            m_pendingOpen = node->path;
            PostAction(m_hwnd, Action::OpenPending);
        }
        return 0;
    }
    case TVN_KEYDOWN: {
        auto* nm = reinterpret_cast<NMTVKEYDOWN*>(hdr);
        if (nm->wVKey == VK_RETURN) {
            PostAction(m_hwnd, Action::Activate);
            return TRUE; // Not part of incremental search (no beep)
        }
        if (nm->wVKey == VK_F5) PostAction(m_hwnd, Action::Refresh);
        return 0;
    }
    case TVN_ITEMEXPANDINGW: {
        auto* nm = reinterpret_cast<NMTREEVIEWW*>(hdr);
        auto* node = reinterpret_cast<Node*>(nm->itemNew.lParam);
        if ((nm->action & kExpandActionMask) == TVE_EXPAND && node) EnsureLoaded(nm->itemNew.hItem, *node);
        return FALSE;
    }
    case TVN_ITEMEXPANDEDW: {
        auto* nm = reinterpret_cast<NMTREEVIEWW*>(hdr);
        const auto* node = reinterpret_cast<const Node*>(nm->itemNew.lParam);
        if (node && node->isFolder) {
            if ((nm->action & kExpandActionMask) == TVE_EXPAND) AddExpanded(node->path);
            else if ((nm->action & kExpandActionMask) == TVE_COLLAPSE) RemoveExpanded(node->path);
        }
        return 0;
    }
    case TVN_GETDISPINFOW: {
        // Only folder icons are callbacks. The expanded image of the tree would not apply to the
        // selected item, which always shows its selected image.
        auto* info = reinterpret_cast<NMTVDISPINFOW*>(hdr);
        const bool open = (TreeView_GetItemState(m_tree, info->item.hItem, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
        const int image = open ? kImageFolderOpen : kImageFolder;
        if (info->item.mask & TVIF_IMAGE) info->item.iImage = image;
        if (info->item.mask & TVIF_SELECTEDIMAGE) info->item.iSelectedImage = image;
        return 0;
    }
    case TVN_DELETEITEMW: {
        auto* nm = reinterpret_cast<NMTREEVIEWW*>(hdr);
        delete reinterpret_cast<Node*>(nm->itemOld.lParam);
        if (nm->itemOld.hItem == m_currentItem) m_currentItem = nullptr;
        if (nm->itemOld.hItem == m_placeholder) m_placeholder = nullptr;
        return 0;
    }
    case NM_CUSTOMDRAW: {
        auto* cd = reinterpret_cast<NMTVCUSTOMDRAW*>(hdr);
        if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
        if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT && cd->nmcd.lItemlParam == 0) {
            const Palette p = GetPalette(m_dark);
            cd->clrText = m_root.empty() ? p.link : p.mutedText; // Placeholder
        }
        return CDRF_DODEFAULT;
    }
    default:
        return 0;
    }
}

LRESULT FileExplorerPanel::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int barsH = BarsHeight();
        LayoutBars();
        if (m_tree) {
            SetWindowPos(m_tree, nullptr, 0, barsH, width, (std::max)(0, height - barsH),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;
    }

    // Shown after changes made while hidden: nothing was read from disk until now. SetWindowPos
    // (the main window's layout) only sends WM_WINDOWPOSCHANGED, ShowWindow only WM_SHOWWINDOW
    // when the parent is hidden. The icons and tooltips are made once the window has painted, so
    // they never delay the start of Pluma.
    case WM_WINDOWPOSCHANGED: {
        const LRESULT result = DefWindowProcW(m_hwnd, msg, wParam, lParam); // Sends WM_SIZE
        if (IsShown()) {
            if (m_stale) {
                Rebuild();
                ShowCurrentFile();
            }
            if (!m_prepared) PostAction(m_hwnd, Action::PrepareForDisplay);
        }
        return result;
    }

    case WM_SHOWWINDOW:
        if (wParam) {
            if (m_stale) {
                Rebuild();
                ShowCurrentFile();
            }
            if (!m_prepared) PostAction(m_hwnd, Action::PrepareForDisplay);
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(m_hwnd, &ps);
        Paint(hdc);
        EndPaint(m_hwnd, &ps);
        return 0;
    }

    case WM_SETFOCUS:
        if (m_tree) SetFocus(m_tree);
        return 0;

    case WM_MOUSEMOVE: {
        const int hit = HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        const int hot = IsEnabled(hit) ? hit : kHitNone;
        if (hot != m_hot) {
            m_hot = hot;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        if (!m_trackingMouse) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, m_hwnd, 0};
            m_trackingMouse = TrackMouseEvent(&tme) != FALSE;
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        m_trackingMouse = false;
        if (m_hot != kHitNone) {
            m_hot = kHitNone;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        const int hit = HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (IsEnabled(hit)) {
            m_pressed = hit;
            SetCapture(m_hwnd);
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (m_pressed == kHitNone) return 0;
        const int pressed = m_pressed;
        const int hit = HitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        m_pressed = kHitNone;
        ReleaseCapture();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        if (hit == pressed) Execute(pressed);
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (m_pressed != kHitNone) {
            m_pressed = kHitNone;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_CONTEXTMENU:
        if (reinterpret_cast<HWND>(wParam) == m_tree) {
            ShowItemMenu(lParam);
            return 0;
        }
        break;

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr->hwndFrom == m_tree) return HandleTreeNotify(hdr);
        break;
    }

    case WM_APP_ACTION:
        switch (static_cast<Action>(wParam)) {
        case Action::Activate:
            Activate(TreeView_GetSelection(m_tree));
            break;
        case Action::OpenPending: {
            const std::filesystem::path path = std::exchange(m_pendingOpen, {});
            if (!path.empty() && m_onOpenFile) m_onOpenFile(path);
            break;
        }
        case Action::GoUp:
            GoUp();
            break;
        case Action::Refresh:
            Refresh();
            break;
        case Action::ChooseFolder:
            if (m_root.empty() && ChooseRootFolder()) Focus();
            break;
        case Action::PrepareForDisplay:
            PrepareForDisplay();
            break;
        }
        return 0;

    case WM_DESTROY:
        if (m_tree) TreeView_DeleteAllItems(m_tree); // Frees the nodes while this window still gets the notifications
        if (m_tooltip) DestroyWindow(m_tooltip);
        m_tree = nullptr;
        m_tooltip = nullptr;
        break;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace Pluma::Explorer
