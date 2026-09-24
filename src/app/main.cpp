#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shobjidl.h> // IFileDialog
#include <shellapi.h>
#include <shlobj.h>   // SHAddToRecentDocs
#include <filesystem>
#include <string>
#include <string_view>
#include <memory>
#include <algorithm>

#include "../platform/dpi.h"
#include "../platform/theme.h"
#include "../io/document_io.h"
#include "../editor/editor_view.h"
#include "../markdown/md4c_adapter.h"
#include "../markdown/parse_worker.h"
#include "../preview/preview_view.h"
#include "../sync/sync_scroll.h"
#include "../export/html_exporter.h"
#include "../export/pdf_exporter.h"
#include "../config/settings.h"
#include "../outline/outline_panel.h"
#include "../platform/file_association.h"
#include "settings_dialog.h"
#include "../../res/resource.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PlumaMainWindowClass";
constexpr int kEditorControlId = 1010;
constexpr int kStatusBarControlId = 1011;
constexpr UINT WM_APP_REPARSE = WM_APP + 1;     // Coalesces bursts of edits into one parse request
constexpr UINT kOutlineFirstCommand = 50000;
constexpr size_t kOutlineMaxItems = 1000;
UINT g_uFindReplaceMsg = 0;
bool g_firstPaintSignaled = false;

std::string Utf16ToUtf8(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), result.data(), count, nullptr, nullptr);
    return result;
}

using ViewMode = Pluma::Config::ViewLayout;

// Draggable dividers of the main window.
enum class Divider {
    None,
    Outline, // Between the headings panel and the editor
    Split    // Between the editor and the preview
};

static HANDLE g_hStartupEvent = nullptr;

void SignalStartupEvent() {
    if (g_firstPaintSignaled) {
        return;
    }

    // Signal named event for startup benchmark if listening
    DWORD pid = GetCurrentProcessId();
    std::wstring eventName = L"Local\\PlumaStartupEvent_" + std::to_wstring(pid);
    g_hStartupEvent = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    if (g_hStartupEvent) {
        SetEvent(g_hStartupEvent);
    }
    g_firstPaintSignaled = true;
}

class MainWindow {
public:
    MainWindow(HINSTANCE hInstance) : m_hInstance(hInstance) {}

    bool Create(int nShowCmd, const std::filesystem::path& initialFile = {}) {
        m_hSizeWeCursor = LoadCursorW(nullptr, IDC_SIZEWE);

        m_settingsPath = Pluma::Config::DefaultSettingsPath();
        m_settings = Pluma::Config::LoadSettings(m_settingsPath);
        m_appTheme = m_settings.theme;
        m_viewMode = m_settings.InitialView();
        m_splitRatio = m_settings.splitRatio;

        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcex.lpfnWndProc = StaticWndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = m_hInstance;
        wcex.hIcon = static_cast<HICON>(LoadImageW(
            m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
        wcex.lpszClassName = kWindowClassName;
        wcex.hIconSm = static_cast<HICON>(LoadImageW(
            m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

        RegisterClassExW(&wcex);

        // Initial size in logical pixels scaled to the primary monitor; WM_DPICHANGED fixes other monitors.
        const UINT systemDpi = GetDpiForSystem();
        int width = Pluma::Platform::ScaleForDpi(1100, systemDpi);
        int height = Pluma::Platform::ScaleForDpi(760, systemDpi);

        m_hwnd = CreateWindowExW(
            WS_EX_ACCEPTFILES,
            kWindowClassName,
            L"Pluma",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            width, height,
            nullptr, nullptr, m_hInstance, this
        );

        if (!m_hwnd) {
            return false;
        }

        SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wcex.hIcon));
        SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wcex.hIconSm));

        // Setup drag and drop (F-01, M1.5)
        DragAcceptFiles(m_hwnd, TRUE);

        // Previous size and position; SetWindowPlacement keeps the window on a connected monitor.
        if (m_settings.rememberWindow && m_settings.hasWindowRect) {
            WINDOWPLACEMENT wp{sizeof(wp)};
            wp.showCmd = SW_HIDE;
            wp.rcNormalPosition = RECT{m_settings.windowX, m_settings.windowY,
                                       m_settings.windowX + m_settings.windowWidth,
                                       m_settings.windowY + m_settings.windowHeight};
            SetWindowPlacement(m_hwnd, &wp);
            if (m_settings.windowMaximized && (nShowCmd == SW_SHOWNORMAL || nShowCmd == SW_SHOWDEFAULT)) {
                nShowCmd = SW_SHOWMAXIMIZED;
            }
        }

        ShowWindow(m_hwnd, nShowCmd);
        UpdateWindow(m_hwnd);

        std::filesystem::path fileToOpen = initialFile;
        if (fileToOpen.empty() && m_settings.reopenLastFile && !m_settings.lastFile.empty()) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(m_settings.lastFile, ec)) {
                fileToOpen = m_settings.lastFile;
            }
        }
        if (!fileToOpen.empty()) {
            OpenFile(fileToOpen);
        } else {
            UpdateTitle();
        }

        return true;
    }

    HWND GetHwnd() const noexcept { return m_hwnd; }
    HWND GetFindReplaceDialog() const noexcept { return m_hFindReplaceDlg; }

    void OpenFile(const std::filesystem::path& requestedPath) {
        std::error_code ec;
        std::filesystem::path path = std::filesystem::absolute(requestedPath, ec);
        if (ec) path = requestedPath;

        if (!std::filesystem::is_regular_file(path, ec)) {
            std::wstring message = L"No se encontró el archivo:\n" + path.wstring();
            MessageBoxW(m_hwnd, message.c_str(), L"Pluma", MB_OK | MB_ICONWARNING);
            return;
        }

        Pluma::IO::DocumentData doc;
        try {
            doc = Pluma::IO::ReadDocument(path);
        } catch (...) {
            // Never keep the path of a document we could not read: saving would overwrite it.
            std::wstring message = L"No se pudo abrir el archivo:\n" + path.wstring() +
                                   L"\n\nPuede que esté bloqueado por otra aplicación o que no tenga permisos de lectura.";
            MessageBoxW(m_hwnd, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        m_currentPath = path;
        m_encoding = doc.encoding;
        m_lineEnding = doc.lineEnding;
        m_hasBom = doc.hasBom;

        m_editor.SetEolMode(ToScintillaEol(m_lineEnding));
        m_editor.SetText(doc.contentUtf8);
        m_preview.SetBaseDirectory(m_currentPath.parent_path());
        m_preview.ResetScroll();
        if (m_syncScroll) m_syncScroll->Reset();
        SHAddToRecentDocs(SHARD_PATHW, m_currentPath.c_str());

        UpdateTitle();
        TriggerParse();
        UpdateStatusBar();
    }

    bool SaveFile() {
        if (m_currentPath.empty()) {
            return SaveAsFile();
        }
        return WriteCurrentDocument(m_currentPath);
    }

    bool WriteCurrentDocument(const std::filesystem::path& path) {
        std::string content = m_editor.GetText();
        if (!Pluma::IO::CanEncodeLosslessly(content, m_encoding)) {
            const int answer = MessageBoxW(m_hwnd,
                L"El documento contiene caracteres que no existen en la codificación ANSI (Windows-1252) "
                L"del archivo, como emojis o letras de otros alfabetos.\n\n"
                L"¿Desea guardarlo en UTF-8 para conservarlos?\n\n"
                L"Sí: guardar en UTF-8.   No: guardar en ANSI (esos caracteres se sustituyen por \"?\").",
                L"Pluma", MB_YESNOCANCEL | MB_ICONWARNING);
            if (answer == IDCANCEL) {
                return false;
            }
            if (answer == IDYES) {
                m_encoding = Pluma::IO::Encoding::Utf8;
                m_hasBom = false;
            }
        }
        try {
            Pluma::IO::WriteDocumentAtomic(path, content, m_encoding, m_lineEnding);
        } catch (...) {
            std::wstring message = L"Error al guardar el archivo:\n" + path.wstring() +
                                   L"\n\nCompruebe que la carpeta existe y que tiene permisos de escritura.";
            MessageBoxW(m_hwnd, message.c_str(), L"Error", MB_OK | MB_ICONERROR);
            return false;
        }
        m_currentPath = path;
        m_editor.SetSavePoint();
        m_preview.SetBaseDirectory(m_currentPath.parent_path());
        UpdateTitle();
        UpdateStatusBar();
        return true;
    }

    static int ToScintillaEol(Pluma::IO::LineEnding lineEnding) {
        switch (lineEnding) {
        case Pluma::IO::LineEnding::LF: return SC_EOL_LF;
        case Pluma::IO::LineEnding::CR: return SC_EOL_CR;
        default:                        return SC_EOL_CRLF;
        }
    }

    bool SaveAsFile() {
        IFileSaveDialog* pFileSave = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                      IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));
        if (FAILED(hr)) {
            return false;
        }

        COMDLG_FILTERSPEC filterSpec[] = {
            { L"Archivos Markdown (*.md;*.markdown;*.mdown)", L"*.md;*.markdown;*.mdown" },
            { L"Todos los archivos (*.*)", L"*.*" }
        };
        pFileSave->SetFileTypes(2, filterSpec);
        pFileSave->SetDefaultExtension(L"md");

        if (!m_currentPath.empty()) {
            pFileSave->SetFileName(m_currentPath.filename().c_str());
        }

        hr = pFileSave->Show(m_hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileSave->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    // The current path only changes once the write succeeded.
                    std::filesystem::path target(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                    pItem->Release();
                    pFileSave->Release();
                    return WriteCurrentDocument(target);
                }
                pItem->Release();
            }
        }
        pFileSave->Release();
        return false;
    }

    void ExportHtml() {
        IFileSaveDialog* pFileSave = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                      IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));
        if (FAILED(hr)) return;

        COMDLG_FILTERSPEC filterSpec[] = {
            { L"Documento HTML (*.html;*.htm)", L"*.html;*.htm" },
            { L"Todos los archivos (*.*)", L"*.*" }
        };
        pFileSave->SetFileTypes(2, filterSpec);
        pFileSave->SetDefaultExtension(L"html");

        if (!m_currentPath.empty()) {
            auto htmlName = m_currentPath.stem().wstring() + L".html";
            pFileSave->SetFileName(htmlName.c_str());
        } else {
            pFileSave->SetFileName(L"documento.html");
        }

        hr = pFileSave->Show(m_hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileSave->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    std::filesystem::path outPath(pszFilePath);
                    CoTaskMemFree(pszFilePath);

                    Pluma::Export::HtmlExportOptions options;
                    // path::string() uses the ANSI code page and throws on characters it cannot represent.
                    options.title = m_currentPath.empty() ? "Documento Pluma" : Utf16ToUtf8(m_currentPath.stem().wstring());
                    options.theme = m_settings.htmlTheme;

                    std::string markdown = m_editor.GetText();
                    bool success = Pluma::Export::HtmlExporter::ExportToFile(outPath, markdown, options);
                    if (!success) {
                        MessageBoxW(m_hwnd, L"Error al exportar el archivo HTML.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
                pItem->Release();
            }
        }
        pFileSave->Release();
    }

    void ExportPdf() {
        IFileSaveDialog* pFileSave = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
                                      IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));
        if (FAILED(hr)) return;

        COMDLG_FILTERSPEC filterSpec[] = {
            { L"Documento PDF (*.pdf)", L"*.pdf" },
            { L"Todos los archivos (*.*)", L"*.*" }
        };
        pFileSave->SetFileTypes(2, filterSpec);
        pFileSave->SetDefaultExtension(L"pdf");

        if (!m_currentPath.empty()) {
            auto pdfName = m_currentPath.stem().wstring() + L".pdf";
            pFileSave->SetFileName(pdfName.c_str());
        } else {
            pFileSave->SetFileName(L"documento.pdf");
        }

        hr = pFileSave->Show(m_hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileSave->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    std::filesystem::path outPath(pszFilePath);
                    CoTaskMemFree(pszFilePath);

                    Pluma::Export::PdfExportOptions options;
                    // path::string() uses the ANSI code page and throws on characters it cannot represent.
                    options.title = m_currentPath.empty() ? "Documento Pluma" : Utf16ToUtf8(m_currentPath.stem().wstring());
                    options.pageSize = m_settings.pdfPageSize;
                    options.marginMm = static_cast<float>(m_settings.pdfMarginMm);

                    std::string markdown = m_editor.GetText();
                    bool success = Pluma::Export::PdfExporter::ExportMarkdownToFile(outPath, markdown, options);
                    if (!success) {
                        MessageBoxW(m_hwnd, L"Error al exportar el archivo PDF.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
                pItem->Release();
            }
        }
        pFileSave->Release();
    }

    void ShowOpenDialog() {
        if (!PromptSaveChanges()) {
            return;
        }

        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                      IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if (FAILED(hr)) {
            return;
        }

        COMDLG_FILTERSPEC filterSpec[] = {
            { L"Archivos Markdown (*.md;*.markdown;*.mdown)", L"*.md;*.markdown;*.mdown" },
            { L"Todos los archivos (*.*)", L"*.*" }
        };
        pFileOpen->SetFileTypes(2, filterSpec);

        hr = pFileOpen->Show(m_hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    OpenFile(pszFilePath);
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    void NewFile() {
        if (!PromptSaveChanges()) {
            return;
        }
        m_currentPath.clear();
        m_encoding = Pluma::IO::Encoding::Utf8;
        m_lineEnding = m_settings.newFileLineEnding;
        m_hasBom = false;
        m_editor.SetEolMode(ToScintillaEol(m_lineEnding));
        m_editor.SetText("");
        m_preview.SetBaseDirectory({});
        m_preview.ResetScroll();
        if (m_syncScroll) m_syncScroll->Reset();
        UpdateTitle();
        TriggerParse();
        UpdateStatusBar();
    }

    bool PromptSaveChanges() {
        if (!m_editor.IsModified()) {
            return true;
        }

        std::wstring docName = m_currentPath.empty() ? L"Sin título" : m_currentPath.filename().wstring();
        std::wstring message = L"¿Desea guardar los cambios en " + docName + L"?";

        int result = MessageBoxW(m_hwnd, message.c_str(), L"Pluma",
                                 MB_YESNOCANCEL | MB_ICONWARNING);
        if (result == IDYES) {
            return SaveFile();
        }
        if (result == IDNO) {
            return true;
        }
        return false; // Cancel
    }

    void UpdateTitle() {
        std::wstring title;
        if (m_editor.IsModified()) {
            title += L"*";
        }
        if (m_currentPath.empty()) {
            title += L"Sin título";
        } else {
            title += m_currentPath.filename().wstring();
        }
        title += L" - Pluma";
        SetWindowTextW(m_hwnd, title.c_str());
    }

    void TriggerParse() {
        m_reparsePosted = false;
        m_statsValid = false;
        if (!m_parseWorker) return;
        m_docVersion++;
        std::string text = m_editor.GetText();
        m_parseWorker->RequestParse(std::move(text), m_docVersion);
    }

    // Schedules a parse once the current burst of modifications (typing, paste, Replace All)
    // has been processed, so the document is copied once instead of once per change.
    void ScheduleParse() {
        m_statsValid = false;
        if (!m_reparsePosted) {
            m_reparsePosted = PostMessageW(m_hwnd, WM_APP_REPARSE, 0, 0) != FALSE;
        }
    }

    struct Layout {
        RECT outline{};        // Empty when the headings panel is hidden
        RECT outlineDivider{};
        RECT editor{};         // Empty in "preview only"
        RECT splitDivider{};   // Empty unless in split view
        RECT preview{};        // Empty in "editor only"
    };

    // Positions of the panels in client coordinates (status bar excluded).
    Layout ComputeLayout() const {
        Layout layout;
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        const int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (m_hwndStatusBar && IsWindowVisible(m_hwndStatusBar)) {
            RECT rcSB{};
            GetWindowRect(m_hwndStatusBar, &rcSB);
            h -= rcSB.bottom - rcSB.top;
        }
        h = (std::max)(10, h);
        if (w <= 0) return layout;

        const int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
        int contentX = 0;
        if (m_settings.showOutline) {
            // The panel never squeezes the document below a usable width.
            const int maxWidth = w - dividerW - Pluma::Platform::ScaleForDpi(160, m_dpi);
            const int outlineW = (std::min)(Pluma::Platform::ScaleForDpi(m_settings.outlineWidth, m_dpi), maxWidth);
            if (outlineW >= Pluma::Platform::ScaleForDpi(80, m_dpi)) {
                layout.outline = RECT{0, 0, outlineW, h};
                layout.outlineDivider = RECT{outlineW, 0, outlineW + dividerW, h};
                contentX = outlineW + dividerW;
            }
        }
        const int contentW = (std::max)(1, w - contentX);

        switch (m_viewMode) {
        case ViewMode::EditorOnly:
            layout.editor = RECT{contentX, 0, contentX + contentW, h};
            break;
        case ViewMode::PreviewOnly:
            layout.preview = RECT{contentX, 0, contentX + contentW, h};
            break;
        case ViewMode::Split: {
            int editorW = static_cast<int>((contentW - dividerW) * m_splitRatio);
            editorW = (std::clamp)(editorW, 50, (std::max)(50, contentW - dividerW - 50));
            const int dividerX = contentX + editorW;
            layout.editor = RECT{contentX, 0, dividerX, h};
            layout.splitDivider = RECT{dividerX, 0, dividerX + dividerW, h};
            layout.preview = RECT{dividerX + dividerW, 0, (std::max)(dividerX + dividerW + 50, contentX + contentW), h};
            break;
        }
        }
        return layout;
    }

    Divider HitTestDivider(POINT pt) const {
        const Layout layout = ComputeLayout();
        if (PtInRect(&layout.splitDivider, pt)) return Divider::Split;
        if (PtInRect(&layout.outlineDivider, pt)) return Divider::Outline;
        return Divider::None;
    }

    static void PlaceWindow(HWND hwnd, const RECT& rc) {
        if (IsRectEmpty(&rc)) {
            ShowWindow(hwnd, SW_HIDE);
        } else {
            SetWindowPos(hwnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }

    void RelayoutChildren() {
        if (!m_hwnd || !m_editor.GetHwnd() || !m_preview.GetHwnd()) return;

        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        if (rc.right - rc.left <= 0 || rc.bottom - rc.top <= 0) return;

        if (m_hwndStatusBar && IsWindowVisible(m_hwndStatusBar)) {
            SendMessageW(m_hwndStatusBar, WM_SIZE, 0, 0);
        }

        const Layout layout = ComputeLayout();
        if (m_outline.GetHwnd()) PlaceWindow(m_outline.GetHwnd(), layout.outline);
        PlaceWindow(m_editor.GetHwnd(), layout.editor);
        PlaceWindow(m_preview.GetHwnd(), layout.preview);

        HMENU hMenu = GetMenu(m_hwnd);
        if (hMenu) {
            UINT checkId = IDM_VIEW_SPLIT;
            if (m_viewMode == ViewMode::EditorOnly) checkId = IDM_VIEW_EDITOR_ONLY;
            else if (m_viewMode == ViewMode::PreviewOnly) checkId = IDM_VIEW_PREVIEW_ONLY;
            CheckMenuRadioItem(hMenu, IDM_VIEW_EDITOR_ONLY, IDM_VIEW_PREVIEW_ONLY, checkId, MF_BYCOMMAND);
            CheckMenuItem(hMenu, IDM_VIEW_OUTLINE_PANEL,
                          MF_BYCOMMAND | (m_settings.showOutline ? MF_CHECKED : MF_UNCHECKED));
            CheckMenuItem(hMenu, IDM_VIEW_STATUSBAR,
                          MF_BYCOMMAND | (m_settings.showStatusBar ? MF_CHECKED : MF_UNCHECKED));
        }

        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    void UpdateStatusBarParts() {
        if (!m_hwndStatusBar) return;
        int p0 = Pluma::Platform::ScaleForDpi(130, m_dpi);
        int p1 = Pluma::Platform::ScaleForDpi(390, m_dpi);
        int p2 = Pluma::Platform::ScaleForDpi(500, m_dpi);
        int p3 = Pluma::Platform::ScaleForDpi(570, m_dpi);
        int sbParts[5] = { p0, p1, p2, p3, -1 };
        SendMessageW(m_hwndStatusBar, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(sbParts));
    }

    void UpdateStatusBar() {
        if (!m_hwndStatusBar) return;

        auto pos = m_editor.GetCursorPosition();
        // Word count scans the whole buffer: only redo it after the text changed, not on caret moves.
        if (!m_statsValid) {
            m_cachedStats = m_editor.GetDocumentStats();
            m_statsValid = true;
        }
        const auto& stats = m_cachedStats;

        wchar_t bufPos[64];
        swprintf_s(bufPos, L"Lín %d, Col %d", pos.line, pos.column);
        SetStatusText(0, bufPos);

        wchar_t bufStats[128];
        swprintf_s(bufStats, L"%zu %s, %zu %s", stats.words, stats.words == 1 ? L"palabra" : L"palabras",
                   stats.characters, stats.characters == 1 ? L"carácter" : L"caracteres");
        SetStatusText(1, bufStats);

        const wchar_t* encStr = L"UTF-8";
        switch (m_encoding) {
        case Pluma::IO::Encoding::Utf8: encStr = m_hasBom ? L"UTF-8 BOM" : L"UTF-8"; break;
        case Pluma::IO::Encoding::Utf8Bom: encStr = L"UTF-8 BOM"; break;
        case Pluma::IO::Encoding::Utf16LE: encStr = L"UTF-16 LE"; break;
        case Pluma::IO::Encoding::Utf16BE: encStr = L"UTF-16 BE"; break;
        case Pluma::IO::Encoding::Ansi: encStr = L"ANSI (1252)"; break;
        }
        SetStatusText(2, encStr);

        const wchar_t* leStr = L"CRLF";
        switch (m_lineEnding) {
        case Pluma::IO::LineEnding::LF: leStr = L"LF"; break;
        case Pluma::IO::LineEnding::CR: leStr = L"CR"; break;
        default: break;
        }
        SetStatusText(3, leStr);

        const wchar_t* modeStr = L"Vista dividida";
        switch (m_viewMode) {
        case ViewMode::EditorOnly: modeStr = L"Solo editor"; break;
        case ViewMode::Split: modeStr = L"Vista dividida"; break;
        case ViewMode::PreviewOnly: modeStr = L"Solo vista previa"; break;
        }
        SetStatusText(4, modeStr);
    }

    // Updates a status bar part only when its text changed (avoids repainting on every caret move).
    void SetStatusText(int part, const wchar_t* text) {
        if (part < 0 || part >= static_cast<int>(std::size(m_statusTexts))) return;
        if (m_statusTexts[part] == text) return;
        m_statusTexts[part] = text;
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, static_cast<WPARAM>(part), reinterpret_cast<LPARAM>(text));
    }

    void ShowOutlinePopup() {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        // Parsed now (not the cached headings) so the menu reflects the latest keystrokes.
        std::string mdText = m_editor.GetText();
        auto tree = Pluma::Markdown::Md4cAdapter::Parse(mdText);
        std::vector<Pluma::Outline::Heading> headings;
        if (tree) {
            headings = Pluma::Outline::CollectHeadings(*tree, kOutlineMaxItems);
        }
        for (auto& heading : headings) {
            if (heading.title.size() > 80) heading.title = heading.title.substr(0, 79) + L"…";
        }

        if (headings.empty()) {
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(No hay encabezados)");
        } else {
            const int currentLine = m_editor.GetCursorPosition().line;
            for (size_t i = 0; i < headings.size(); ++i) {
                // Indentation shows the hierarchy; "&" would be read as a mnemonic prefix.
                std::wstring itemText(static_cast<size_t>((headings[i].level - 1) * 4), L' ');
                itemText += (headings[i].level == 1) ? L"■  " : (headings[i].level == 2) ? L"▪  " : L"·  ";
                for (wchar_t ch : headings[i].title) {
                    itemText += ch;
                    if (ch == L'&') itemText += L'&';
                }
                UINT flags = MF_STRING;
                // Mark the section that contains the caret.
                const bool isCurrent = headings[i].line <= currentLine &&
                                       (i + 1 == headings.size() || headings[i + 1].line > currentLine);
                if (isCurrent) flags |= MF_CHECKED;
                AppendMenuW(hMenu, flags, kOutlineFirstCommand + static_cast<UINT>(i), itemText.c_str());
            }
        }

        // Open next to the caret when invoked from the keyboard.
        POINT pt{};
        GetCursorPos(&pt);
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            const sptr_t caret = m_editor.Call(SCI_GETCURRENTPOS);
            pt.x = static_cast<LONG>(m_editor.Call(SCI_POINTXFROMPOSITION, 0, caret));
            pt.y = static_cast<LONG>(m_editor.Call(SCI_POINTYFROMPOSITION, 0, caret) +
                                     m_editor.Call(SCI_TEXTHEIGHT, 0));
            ClientToScreen(m_editor.GetHwnd(), &pt);
        }
        const int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_NONOTIFY,
                                       pt.x, pt.y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd >= static_cast<int>(kOutlineFirstCommand) &&
            static_cast<size_t>(cmd - kOutlineFirstCommand) < headings.size()) {
            NavigateToLine(headings[cmd - kOutlineFirstCommand].line, true);
        }
    }

    // Moves the caret to `line` with that line at the top of the editor and the preview.
    void NavigateToLine(int line, bool focusView) {
        m_editor.GotoLine(line);
        m_editor.Call(SCI_SETFIRSTVISIBLELINE, m_editor.Call(SCI_VISIBLEFROMDOCLINE, line - 1));
        m_preview.ScrollToLine(line);
        if (focusView) FocusMainView();
        UpdateStatusBar();
    }

    void FocusMainView() {
        if (m_viewMode == ViewMode::PreviewOnly) {
            ::SetFocus(m_preview.GetHwnd()); // Keyboard scrolling in the preview
        } else {
            m_editor.SetFocus();
        }
    }

    void ShowGotoLineDialog() {
        static bool s_registered = false;
        if (!s_registered) {
            WNDCLASSEXW wc{ sizeof(WNDCLASSEXW) };
            wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                switch (msg) {
                case WM_CREATE: {
                    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                    self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));

                    bool dark = self && self->IsDarkMode();
                    Pluma::Platform::ApplyThemeToWindow(hwnd, dark);

                    UINT dpi = self ? self->m_dpi : 96;
                    HFONT hFont = CreateFontW(
                        -Pluma::Platform::ScaleForDpi(13, dpi), 0, 0, 0,
                        FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

                    int pad = Pluma::Platform::ScaleForDpi(16, dpi);
                    int lblH = Pluma::Platform::ScaleForDpi(20, dpi);
                    int editH = Pluma::Platform::ScaleForDpi(26, dpi);
                    int btnW = Pluma::Platform::ScaleForDpi(84, dpi);
                    int btnH = Pluma::Platform::ScaleForDpi(28, dpi);
                    int spacing = Pluma::Platform::ScaleForDpi(12, dpi);

                    RECT rcClient{};
                    GetClientRect(hwnd, &rcClient);
                    const int innerW = (rcClient.right - rcClient.left) - 2 * pad;

                    const int lineCount = self ? static_cast<int>(self->m_editor.Call(SCI_GETLINECOUNT)) : 1;
                    const std::wstring label = L"Número de línea (1 - " + std::to_wstring(lineCount) + L"):";
                    HWND hStatic = CreateWindowW(L"STATIC", label.c_str(), WS_CHILD | WS_VISIBLE,
                                                 pad, pad, innerW, lblH, hwnd, nullptr, nullptr, nullptr);
                    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
                                                 pad, pad + lblH + Pluma::Platform::ScaleForDpi(4, dpi), innerW, editH, hwnd,
                                                 reinterpret_cast<HMENU>(101), nullptr, nullptr);
                    int btnY = pad + lblH + editH + spacing;
                    const int btnGap = Pluma::Platform::ScaleForDpi(8, dpi);
                    HWND hOk = CreateWindowW(L"BUTTON", L"Aceptar",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                             pad + innerW - 2 * btnW - btnGap, btnY, btnW, btnH, hwnd,
                                             reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                    HWND hCancel = CreateWindowW(L"BUTTON", L"Cancelar",
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                 pad + innerW - btnW, btnY, btnW, btnH, hwnd,
                                                 reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);

                    // Pre-fill with the current line, selected so typing replaces it.
                    if (self) {
                        const std::wstring current = std::to_wstring(self->m_editor.GetCursorPosition().line);
                        SetWindowTextW(hEdit, current.c_str());
                        SendMessageW(hEdit, EM_SETSEL, 0, -1);
                    }
                    SetPropW(hwnd, L"PlumaDlgFont", hFont);

                    if (dark) {
                        SetWindowTheme(hEdit, L"DarkMode_Explorer", nullptr);
                        SetWindowTheme(hOk, L"DarkMode_Explorer", nullptr);
                        SetWindowTheme(hCancel, L"DarkMode_Explorer", nullptr);
                    }

                    if (hFont) {
                        SendMessageW(hStatic, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                        SendMessageW(hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                        SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                        SendMessageW(hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
                    }
                    SetFocus(hEdit);
                    return 0;
                }
                case WM_NCDESTROY: {
                    if (auto hDlgFont = static_cast<HFONT>(RemovePropW(hwnd, L"PlumaDlgFont"))) {
                        DeleteObject(hDlgFont);
                    }
                    break;
                }
                case WM_ERASEBKGND: {
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    bool dark = self && self->IsDarkMode();
                    HBRUSH hbr = CreateSolidBrush(dark ? RGB(32, 32, 32) : RGB(243, 243, 243));
                    FillRect(hdc, &rc, hbr);
                    DeleteObject(hbr);
                    return 1;
                }
                case WM_CTLCOLORSTATIC: {
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    SetBkMode(hdc, TRANSPARENT);
                    bool dark = self && self->IsDarkMode();
                    SetTextColor(hdc, dark ? RGB(225, 225, 225) : RGB(30, 30, 30));
                    return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
                }
                case WM_CTLCOLOREDIT: {
                    HDC hdc = reinterpret_cast<HDC>(wParam);
                    bool dark = self && self->IsDarkMode();
                    if (dark) {
                        SetBkColor(hdc, RGB(42, 42, 42));
                        SetTextColor(hdc, RGB(255, 255, 255));
                        static HBRUSH s_hbrEdit = CreateSolidBrush(RGB(42, 42, 42));
                        return reinterpret_cast<LRESULT>(s_hbrEdit);
                    }
                    return DefWindowProcW(hwnd, msg, wParam, lParam);
                }
                case WM_CTLCOLORBTN: {
                    bool dark = self && self->IsDarkMode();
                    if (dark) {
                        static HBRUSH s_hbrBtn = CreateSolidBrush(RGB(32, 32, 32));
                        return reinterpret_cast<LRESULT>(s_hbrBtn);
                    }
                    return DefWindowProcW(hwnd, msg, wParam, lParam);
                }
                case WM_COMMAND: {
                    int id = LOWORD(wParam);
                    if (id == IDOK) {
                        wchar_t buf[32]{};
                        GetDlgItemTextW(hwnd, 101, buf, 31);
                        int line = _wtoi(buf);
                        if (line > 0 && self) {
                            const int lineCount = static_cast<int>(self->m_editor.Call(SCI_GETLINECOUNT));
                            line = (std::min)(line, lineCount);
                            self->m_editor.GotoLine(line);
                            self->m_preview.ScrollToLine(line);
                            self->m_editor.SetFocus();
                            self->UpdateStatusBar();
                        }
                        DestroyWindow(hwnd);
                        return 0;
                    } else if (id == IDCANCEL) {
                        DestroyWindow(hwnd);
                        return 0;
                    }
                    break;
                }
                case WM_CLOSE:
                    DestroyWindow(hwnd);
                    return 0;
                }
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            };
            wc.hInstance = m_hInstance;
            wc.lpszClassName = L"PlumaGotoLineClass";
            wc.hbrBackground = nullptr;
            wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            RegisterClassExW(&wc);
            s_registered = true;
        }

        // Client area sized in logical units; the frame is added for the current DPI.
        RECT rcDlg{0, 0, Pluma::Platform::ScaleForDpi(300, m_dpi), Pluma::Platform::ScaleForDpi(128, m_dpi)};
        const DWORD dlgStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        const DWORD dlgExStyle = WS_EX_DLGMODALFRAME;
        AdjustWindowRectExForDpi(&rcDlg, dlgStyle, FALSE, dlgExStyle, m_dpi);
        const int dlgW = rcDlg.right - rcDlg.left;
        const int dlgH = rcDlg.bottom - rcDlg.top;

        RECT rc{};
        GetWindowRect(m_hwnd, &rc);
        int x = rc.left + (rc.right - rc.left - dlgW) / 2;
        int y = rc.top + (rc.bottom - rc.top - dlgH) / 2;
        HWND hDlg = CreateWindowExW(dlgExStyle, L"PlumaGotoLineClass", L"Ir a línea",
                                   dlgStyle | WS_VISIBLE,
                                   x, y, dlgW, dlgH, m_hwnd, nullptr, m_hInstance, this);
        if (!hDlg) return;
        EnableWindow(m_hwnd, FALSE);
        MSG msg{};
        while (IsWindow(hDlg)) {
            const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
            if (got == 0) {
                PostQuitMessage(static_cast<int>(msg.wParam)); // Let the main loop see WM_QUIT
                break;
            }
            if (got < 0) break;
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                DestroyWindow(hDlg);
                break;
            }
            if (!IsDialogMessageW(hDlg, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        EnableWindow(m_hwnd, TRUE);
        if (IsWindow(hDlg)) DestroyWindow(hDlg);
        SetForegroundWindow(m_hwnd);
        m_editor.SetFocus();
    }

    void ShowFindDialog() {
        if (m_hFindReplaceDlg) {
            SetFocus(m_hFindReplaceDlg);
            return;
        }

        ZeroMemory(&m_fr, sizeof(m_fr));
        m_fr.lStructSize = sizeof(m_fr);
        m_fr.hwndOwner = m_hwnd;
        m_fr.lpstrFindWhat = m_szFindWhat;
        m_fr.wFindWhatLen = ARRAYSIZE(m_szFindWhat);
        m_fr.Flags = FR_DOWN;

        m_hFindReplaceDlg = FindTextW(&m_fr);
        if (m_hFindReplaceDlg) {
            Pluma::Platform::ApplyThemeToWindow(m_hFindReplaceDlg, IsDarkMode());
        }
    }

    void ShowReplaceDialog() {
        if (m_hFindReplaceDlg) {
            SetFocus(m_hFindReplaceDlg);
            return;
        }

        ZeroMemory(&m_fr, sizeof(m_fr));
        m_fr.lStructSize = sizeof(m_fr);
        m_fr.hwndOwner = m_hwnd;
        m_fr.lpstrFindWhat = m_szFindWhat;
        m_fr.wFindWhatLen = ARRAYSIZE(m_szFindWhat);
        m_fr.lpstrReplaceWith = m_szReplaceWith;
        m_fr.wReplaceWithLen = ARRAYSIZE(m_szReplaceWith);
        m_fr.Flags = FR_DOWN;

        m_hFindReplaceDlg = ReplaceTextW(&m_fr);
        if (m_hFindReplaceDlg) {
            Pluma::Platform::ApplyThemeToWindow(m_hFindReplaceDlg, IsDarkMode());
        }
    }

    void HandleFindReplaceMsg(FINDREPLACEW* pfr) {
        if (!pfr) return;
        if (pfr->Flags & FR_DIALOGTERM) {
            m_hFindReplaceDlg = nullptr;
            return;
        }

        std::string findStr = Utf16ToUtf8(pfr->lpstrFindWhat);
        bool matchCase = (pfr->Flags & FR_MATCHCASE) != 0;
        bool wholeWord = (pfr->Flags & FR_WHOLEWORD) != 0;
        bool forward = (pfr->Flags & FR_DOWN) != 0;

        if (pfr->Flags & FR_FINDNEXT) {
            bool found = m_editor.FindNext(findStr, matchCase, wholeWord, false, forward);
            if (!found) {
                MessageBoxW(m_hwnd, L"No se encontraron más coincidencias.", L"Buscar", MB_OK | MB_ICONINFORMATION);
            }
        } else if (pfr->Flags & FR_REPLACE) {
            std::string repStr = Utf16ToUtf8(pfr->lpstrReplaceWith);
            // Replace only when the selection is the current match; otherwise just find it first.
            if (m_editor.SelectionMatches(findStr, matchCase)) {
                m_editor.Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(repStr.c_str()));
            }
            if (!m_editor.FindNext(findStr, matchCase, wholeWord, false, forward)) {
                MessageBoxW(m_hwnd, L"No se encontraron más coincidencias.", L"Reemplazar", MB_OK | MB_ICONINFORMATION);
            }
        } else if (pfr->Flags & FR_REPLACEALL) {
            std::string repStr = Utf16ToUtf8(pfr->lpstrReplaceWith);
            int replaced = m_editor.ReplaceAll(findStr, repStr, matchCase, wholeWord, false);
            std::wstring msg = L"Se reemplazaron " + std::to_wstring(replaced) + L" ocurrencia(s).";
            MessageBoxW(m_hwnd, msg.c_str(), L"Reemplazar", MB_OK | MB_ICONINFORMATION);
        }
    }

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        MainWindow* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = reinterpret_cast<MainWindow*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->m_hwnd = hwnd;
        } else {
            self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self) {
            return self->HandleMessage(msg, wParam, lParam);
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        LRESULT uahLr = 0;
        if (Pluma::Platform::HandleUAHMenuBarMessage(m_hwnd, msg, wParam, lParam, &uahLr, IsDarkMode())) {
            return uahLr;
        }

        if (msg == g_uFindReplaceMsg && g_uFindReplaceMsg != 0) {
            HandleFindReplaceMsg(reinterpret_cast<FINDREPLACEW*>(lParam));
            return 0;
        }

        switch (msg) {
        case WM_CREATE: {
            bool darkMode = IsDarkMode();
            Pluma::Platform::SetPreferredThemeMode(m_appTheme);
            Pluma::Platform::ApplyThemeToWindow(m_hwnd, darkMode);

            m_dpi = Pluma::Platform::GetWindowDpi(m_hwnd);

            RECT rc;
            GetClientRect(m_hwnd, &rc);
            int width = (std::max)(100, (int)(rc.right - rc.left));
            int height = (std::max)(100, (int)(rc.bottom - rc.top));

            m_hwndStatusBar = CreateStatusWindowW(
                WS_CHILD | (m_settings.showStatusBar ? WS_VISIBLE : 0) | SBARS_SIZEGRIP,
                nullptr, m_hwnd, kStatusBarControlId);
            SetWindowSubclass(m_hwndStatusBar, StatusBarSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
            UpdateStatusBarParts();

            m_editor.Create(m_hwnd, m_hInstance, kEditorControlId, 0, 0, width / 2, height);
            ApplyEditorSettings();
            m_editor.ApplyTheme(darkMode);
            m_lineEnding = m_settings.newFileLineEnding;
            m_editor.SetEolMode(ToScintillaEol(m_lineEnding));

            m_preview.Create(m_hwnd, m_hInstance, width / 2, 0, width / 2, height);
            m_preview.SetDarkMode(darkMode);
            m_preview.SetDpi(PreviewDpi());

            m_outline.Create(m_hwnd, m_hInstance);
            m_outline.SetDarkMode(darkMode);
            m_outline.SetOnNavigate([this](int line, bool focusView) { NavigateToLine(line, focusView); });
            m_outline.SetOnClose([this] { SetOutlineVisible(false); });
            m_preview.SetOnOpenDocumentCallback([this](const std::filesystem::path& path) {
                // Links to other Markdown files open in Pluma itself.
                if (PromptSaveChanges()) {
                    OpenFile(path);
                }
            });

            m_syncScroll = std::make_unique<Pluma::Sync::SyncScrollController>(&m_editor, &m_preview);
            UpdateSyncScrollState();
            m_parseWorker = std::make_unique<Pluma::Markdown::ParseWorker>(m_hwnd, Pluma::Markdown::WM_USER_PARSE_COMPLETE);

            RelayoutChildren();
            TriggerParse();
            UpdateStatusBar();
            UpdateThemeMenuRadio();
            UpdateWordWrapMenuCheck();
            FocusMainView();
            return 0;
        }

        case WM_SIZE: {
            RelayoutChildren();
            UpdateStatusBar();
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SETFOCUS:
            FocusMainView();
            return 0;

        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            // Query the length first: dropped paths can exceed MAX_PATH.
            const UINT length = DragQueryFileW(hDrop, 0, nullptr, 0);
            std::wstring filePath(length, L'\0');
            const bool gotPath = length > 0 && DragQueryFileW(hDrop, 0, filePath.data(), length + 1) > 0;
            DragFinish(hDrop);
            if (gotPath) {
                SetForegroundWindow(m_hwnd);
                if (PromptSaveChanges()) {
                    OpenFile(filePath);
                }
            }
            return 0;
        }

        case WM_NOTIFY: {
            auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
            if (nmhdr->idFrom == kEditorControlId) {
                auto* scn = reinterpret_cast<SCNotification*>(lParam);
                if (scn->nmhdr.code == SCN_SAVEPOINTREACHED || scn->nmhdr.code == SCN_SAVEPOINTLEFT) {
                    UpdateTitle();
                } else if (scn->nmhdr.code == SCN_MODIFIED) {
                    if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
                        ScheduleParse();
                        if (scn->linesAdded != 0) {
                            m_editor.UpdateLineNumberMargin();
                        }
                    }
                } else if (scn->nmhdr.code == SCN_UPDATEUI) {
                    UpdateStatusBar();
                    if (m_settings.showOutline && (scn->updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))) {
                        m_outline.HighlightLine(m_editor.GetCursorPosition().line);
                    }
                    if (scn->updated & SC_UPDATE_V_SCROLL) {
                        if (m_syncScroll && m_viewMode == ViewMode::Split) {
                            m_syncScroll->OnEditorScrolled();
                        }
                    }
                }
            }
            return 0;
        }

        case WM_APP_REPARSE:
            if (m_reparsePosted) {
                TriggerParse();
            }
            return 0;

        case Pluma::Markdown::WM_USER_PARSE_COMPLETE: {
            uint64_t version = static_cast<uint64_t>(wParam);
            auto* rawTree = reinterpret_cast<Pluma::Markdown::BlockTree*>(lParam);
            if (version >= m_lastAppliedVersion) {
                m_lastAppliedVersion = version;
                m_headings = Pluma::Outline::CollectHeadings(*rawTree);
                if (m_settings.showOutline) {
                    m_outline.SetHeadings(m_headings);
                    m_outline.HighlightLine(m_editor.GetCursorPosition().line);
                }
                m_preview.SetBlockTree(std::unique_ptr<Pluma::Markdown::BlockTree>(rawTree));
            } else {
                delete rawTree;
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            const Divider divider = HitTestDivider(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            if (divider != Divider::None) {
                m_dragDivider = divider;
                SetCapture(m_hwnd);
                InvalidateDividers();
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            // Signed coordinates: while captured the mouse may go left of the window.
            const POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (m_dragDivider != Divider::None) {
                DragDividerTo(pt.x);
                return 0;
            }

            const Divider hovered = HitTestDivider(pt);
            if (hovered != m_hoverDivider) {
                m_hoverDivider = hovered;
                InvalidateDividers();
                if (hovered != Divider::None) {
                    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hwnd, 0 };
                    TrackMouseEvent(&tme);
                }
            }
            break;
        }

        case WM_MOUSELEAVE: {
            if (m_hoverDivider != Divider::None) {
                m_hoverDivider = Divider::None;
                InvalidateDividers();
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_dragDivider != Divider::None) {
                m_dragDivider = Divider::None;
                ReleaseCapture();
                InvalidateDividers();
                return 0;
            }
            break;
        }

        case WM_CAPTURECHANGED:
            m_dragDivider = Divider::None;
            break;

        case WM_LBUTTONDBLCLK: {
            // Double-click on a divider restores its default position.
            switch (HitTestDivider(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)})) {
            case Divider::Split:
                m_splitRatio = 0.5f;
                RelayoutChildren();
                return 0;
            case Divider::Outline:
                m_settings.outlineWidth = Pluma::Config::Settings{}.outlineWidth;
                RelayoutChildren();
                return 0;
            default:
                break;
            }
            break;
        }

        case WM_SETCURSOR: {
            if (m_dragDivider != Divider::None) {
                SetCursor(m_hSizeWeCursor);
                return TRUE;
            }
            if (reinterpret_cast<HWND>(wParam) == m_hwnd) {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(m_hwnd, &pt);
                if (HitTestDivider(pt) != Divider::None) {
                    SetCursor(m_hSizeWeCursor);
                    return TRUE;
                }
            }
            break;
        }

        case WM_SETTINGCHANGE: {
            if (Pluma::Platform::IsColorSchemeChangeMessage(lParam)) {
                if (m_appTheme == Pluma::Platform::AppTheme::System) {
                    ApplyCurrentTheme();
                }
            }
            return 0;
        }

        case WM_THEMECHANGED: {
            ApplyCurrentTheme();
            return 0;
        }

        case WM_DPICHANGED: {
            m_dpi = HIWORD(wParam);
            m_preview.SetDpi(PreviewDpi());
            m_outline.SetDpi(m_dpi);
            m_statusFont.reset();
            UpdateStatusBarParts();
            auto* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(m_hwnd, nullptr,
                         prcNewWindow->left, prcNewWindow->top,
                         prcNewWindow->right - prcNewWindow->left,
                         prcNewWindow->bottom - prcNewWindow->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            RelayoutChildren();
            UpdateStatusBar();
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            const Layout layout = ComputeLayout();
            PaintDivider(hdc, layout.outlineDivider, Divider::Outline);
            PaintDivider(hdc, layout.splitDivider, Divider::Split);
            EndPaint(m_hwnd, &ps);
            SignalStartupEvent();
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDM_FILE_NEW:
                NewFile();
                return 0;
            case IDM_FILE_OPEN:
                ShowOpenDialog();
                return 0;
            case IDM_FILE_SAVE:
                SaveFile();
                return 0;
            case IDM_FILE_SAVEAS:
                SaveAsFile();
                return 0;
            case IDM_FILE_EXPORT_HTML:
                ExportHtml();
                return 0;
            case IDM_FILE_EXPORT_PDF:
                ExportPdf();
                return 0;
            case IDM_FILE_EXIT:
                SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
                return 0;

            case IDM_EDIT_UNDO:
                m_editor.Undo();
                return 0;
            case IDM_EDIT_REDO:
                m_editor.Redo();
                return 0;
            case IDM_EDIT_CUT:
                m_editor.Cut();
                return 0;
            case IDM_EDIT_COPY:
                m_editor.Copy();
                return 0;
            case IDM_EDIT_PASTE:
                m_editor.Paste();
                return 0;
            case IDM_EDIT_WRAP:
                m_editor.ToggleWordWrap();
                m_settings.wordWrap = m_editor.GetWordWrap();
                UpdateWordWrapMenuCheck();
                return 0;
            case IDM_EDIT_SELECTALL:
                m_editor.SelectAll();
                return 0;
            case IDM_EDIT_FIND:
                ShowFindDialog();
                return 0;
            case IDM_EDIT_REPLACE:
                ShowReplaceDialog();
                return 0;
            case IDM_EDIT_GOTO:
                ShowGotoLineDialog();
                return 0;

            case IDM_FORMAT_BOLD:
                m_editor.InsertBold();
                return 0;
            case IDM_FORMAT_ITALIC:
                m_editor.InsertItalic();
                return 0;
            case IDM_FORMAT_CODE:
                m_editor.InsertCode();
                return 0;
            case IDM_FORMAT_STRIKE:
                m_editor.InsertStrikethrough();
                return 0;
            case IDM_FORMAT_LINK:
                m_editor.InsertLink();
                return 0;

            case IDM_VIEW_EDITOR_ONLY:
                SetViewMode(ViewMode::EditorOnly);
                return 0;
            case IDM_VIEW_SPLIT:
                SetViewMode(ViewMode::Split);
                return 0;
            case IDM_VIEW_PREVIEW_ONLY:
                SetViewMode(ViewMode::PreviewOnly);
                return 0;
            case IDM_VIEW_OUTLINE:
                ShowOutlinePopup();
                return 0;
            case IDM_VIEW_OUTLINE_PANEL:
                SetOutlineVisible(!m_settings.showOutline);
                if (m_settings.showOutline) m_outline.Focus();
                return 0;
            case IDM_VIEW_STATUSBAR:
                SetStatusBarVisible(!m_settings.showStatusBar);
                return 0;

            case IDM_SETTINGS_PREFERENCES:
                ShowPreferences();
                return 0;
            case IDM_SETTINGS_DEFAULT_EDITOR:
                MakeDefaultEditor(m_hwnd);
                return 0;

            case IDM_VIEW_THEME_SYSTEM:
                SetThemeMode(Pluma::Platform::AppTheme::System);
                return 0;
            case IDM_VIEW_THEME_DARK:
                SetThemeMode(Pluma::Platform::AppTheme::Dark);
                return 0;
            case IDM_VIEW_THEME_LIGHT:
                SetThemeMode(Pluma::Platform::AppTheme::Light);
                return 0;

            case IDM_HELP_ABOUT:
                MessageBoxW(m_hwnd,
                            L"Pluma - Editor Markdown nativo para Windows v0.1\n\n"
                            L"Desarrollado para velocidad instantánea con Win32, Scintilla y DirectWrite.",
                            L"Acerca de Pluma", MB_OK | MB_ICONINFORMATION);
                return 0;

            default:
                return DefWindowProcW(m_hwnd, msg, wParam, lParam);
            }
        }

        case WM_CLOSE:
            if (PromptSaveChanges()) {
                SaveSettingsNow();
                DestroyWindow(m_hwnd);
            }
            return 0;

        case WM_DESTROY: {
            if (m_parseWorker) {
                m_parseWorker->Stop();
            }
            // Trees already posted by the worker are owned by their messages: free them.
            MSG pending;
            while (PeekMessageW(&pending, m_hwnd, Pluma::Markdown::WM_USER_PARSE_COMPLETE,
                                Pluma::Markdown::WM_USER_PARSE_COMPLETE, PM_REMOVE)) {
                delete reinterpret_cast<Pluma::Markdown::BlockTree*>(pending.lParam);
            }
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
        }
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }

    Pluma::Platform::AppTheme m_appTheme = Pluma::Platform::AppTheme::System;

    bool IsDarkMode() const {
        return Pluma::Platform::IsDarkModeActive(m_appTheme);
    }

    void SetThemeMode(Pluma::Platform::AppTheme theme) {
        if (m_appTheme == theme) return;
        m_appTheme = theme;
        ApplyCurrentTheme();
    }

    void ApplyCurrentTheme() {
        static bool s_isApplyingTheme = false;
        if (s_isApplyingTheme) return;
        struct ThemeGuard {
            bool& ref;
            explicit ThemeGuard(bool& r) : ref(r) { ref = true; }
            ~ThemeGuard() { ref = false; }
        } guard(s_isApplyingTheme);

        bool darkMode = IsDarkMode();
        Pluma::Platform::SetPreferredThemeMode(m_appTheme);
        Pluma::Platform::ApplyThemeToWindow(m_hwnd, darkMode);
        m_editor.ApplyTheme(darkMode);
        m_preview.SetDarkMode(darkMode);
        m_outline.SetDarkMode(darkMode);

        UpdateThemeMenuRadio();
        Pluma::Platform::RefreshWindowFrame(m_hwnd);

        if (m_hwndStatusBar) {
            InvalidateRect(m_hwndStatusBar, nullptr, TRUE);
        }
        InvalidateRect(m_hwnd, nullptr, TRUE);
        UpdateStatusBar();
    }

    // Preview zoom is applied as a DPI scale: layout and text grow uniformly.
    UINT PreviewDpi() const {
        return static_cast<UINT>(MulDiv(static_cast<int>(m_dpi), m_settings.previewZoom, 100));
    }

    void ApplyEditorSettings() {
        m_editor.SetFont(m_settings.editorFont, m_settings.editorFontSize);
        m_editor.SetWordWrap(m_settings.wordWrap);
        m_editor.SetShowLineNumbers(m_settings.showLineNumbers);
        m_editor.SetHighlightCurrentLine(m_settings.highlightCurrentLine);
        m_editor.SetTabSettings(m_settings.tabWidth, m_settings.useTabs);
    }

    void UpdateSyncScrollState() {
        if (m_syncScroll) {
            m_syncScroll->SetEnabled(m_viewMode == ViewMode::Split && m_settings.syncScroll);
        }
    }

    void SetViewMode(ViewMode mode) {
        m_viewMode = mode;
        UpdateSyncScrollState();
        RelayoutChildren();
        UpdateStatusBar();
        // Keep the keyboard in a visible view.
        const HWND focus = GetFocus();
        if (focus == m_editor.GetHwnd() || focus == m_preview.GetHwnd()) FocusMainView();
    }

    void SetOutlineVisible(bool visible) {
        const bool hadFocus = !visible && m_outline.GetHwnd() && IsChild(m_outline.GetHwnd(), GetFocus());
        m_settings.showOutline = visible;
        if (visible) {
            m_outline.SetHeadings(m_headings);
            m_outline.HighlightLine(m_editor.GetCursorPosition().line);
        }
        RelayoutChildren();
        if (hadFocus) FocusMainView();
    }

    void SetStatusBarVisible(bool visible) {
        m_settings.showStatusBar = visible;
        if (m_hwndStatusBar) ShowWindow(m_hwndStatusBar, visible ? SW_SHOW : SW_HIDE);
        RelayoutChildren();
    }

    // Applies preferences edited in the Preferences dialog.
    void ApplySettings(const Pluma::Config::Settings& settings) {
        const bool zoomChanged = settings.previewZoom != m_settings.previewZoom;
        m_settings = settings;
        ApplyEditorSettings();
        UpdateWordWrapMenuCheck();
        if (zoomChanged) m_preview.SetDpi(PreviewDpi());
        if (m_hwndStatusBar) ShowWindow(m_hwndStatusBar, m_settings.showStatusBar ? SW_SHOW : SW_HIDE);
        SetOutlineVisible(m_settings.showOutline);
        UpdateSyncScrollState();
        if (settings.theme != m_appTheme) {
            SetThemeMode(settings.theme);
        }
        UpdateStatusBar();
    }

    // Copies the state changed outside the Preferences dialog (menus, dividers, window) into m_settings.
    void CaptureRuntimeState() {
        m_settings.theme = m_appTheme;
        m_settings.lastView = m_viewMode;
        m_settings.splitRatio = m_splitRatio;
        m_settings.wordWrap = m_editor.GetWordWrap();
        m_settings.lastFile = m_currentPath.wstring();

        WINDOWPLACEMENT wp{sizeof(wp)};
        if (m_hwnd && GetWindowPlacement(m_hwnd, &wp)) {
            const RECT& r = wp.rcNormalPosition;
            m_settings.hasWindowRect = r.right > r.left && r.bottom > r.top;
            m_settings.windowX = r.left;
            m_settings.windowY = r.top;
            m_settings.windowWidth = r.right - r.left;
            m_settings.windowHeight = r.bottom - r.top;
            m_settings.windowMaximized =
                IsZoomed(m_hwnd) || (IsIconic(m_hwnd) && (wp.flags & WPF_RESTORETOMAXIMIZED));
        }
        m_settings.Sanitize();
    }

    void SaveSettingsNow() {
        CaptureRuntimeState();
        Pluma::Config::SaveSettings(m_settingsPath, m_settings);
    }

    void ShowPreferences() {
        CaptureRuntimeState();
        Pluma::App::SettingsDialogCallbacks callbacks;
        callbacks.apply = [this](const Pluma::Config::Settings& settings) {
            ApplySettings(settings);
            SaveSettingsNow();
        };
        callbacks.makeDefaultEditor = [this] { MakeDefaultEditor(GetActiveWindow()); };
        callbacks.isDefaultEditor = [] {
            return Pluma::Platform::IsDefaultMarkdownHandler(Pluma::Platform::GetExecutablePath());
        };
        Pluma::App::ShowSettingsDialog(m_hwnd, m_hInstance, m_settings, callbacks);
        FocusMainView();
    }

    // Registers Pluma for .md files and asks Windows to make it the default editor.
    void MakeDefaultEditor(HWND owner) {
        const std::wstring exePath = Pluma::Platform::GetExecutablePath();
        const bool wasDefault = Pluma::Platform::IsDefaultMarkdownHandler(exePath);
        if (!Pluma::Platform::RegisterMarkdownHandler(exePath)) {
            MessageBoxW(owner, L"No se pudo registrar Pluma como aplicación para archivos Markdown.",
                        L"Editor predeterminado", MB_OK | MB_ICONERROR);
            return;
        }
        if (Pluma::Platform::IsDefaultMarkdownHandler(exePath)) {
            MessageBoxW(owner,
                        wasDefault ? L"Pluma ya es el editor predeterminado de los archivos Markdown "
                                     L"(.md, .markdown, .mdown)."
                                   : L"Listo: los archivos Markdown (.md, .markdown, .mdown) se abrirán con Pluma.",
                        L"Editor predeterminado", MB_OK | MB_ICONINFORMATION);
            return;
        }

        const int answer = MessageBoxW(owner,
            L"Pluma se ha registrado como aplicación para archivos Markdown.\n\n"
            L"Windows solo permite cambiar el editor predeterminado desde Configuración. "
            L"A continuación se abrirá Configuración > Aplicaciones > Aplicaciones predeterminadas:\n\n"
            L"\u2022 Windows 11: asigne Pluma a .md, .markdown y .mdown.\n"
            L"\u2022 Windows 10: pulse \u00abEstablecer valores predeterminados por aplicación\u00bb, "
            L"elija Pluma, pulse \u00abAdministrar\u00bb y asigne Pluma a cada extensión.",
            L"Editor predeterminado", MB_OKCANCEL | MB_ICONINFORMATION);
        if (answer != IDOK) return;

        // Settings is non-blocking: the result shows up the next time the preferences are opened.
        if (!Pluma::Platform::OpenDefaultAppsSettings(owner)) {
            MessageBoxW(owner,
                        L"No se pudo abrir Configuración.\n\n"
                        L"Puede elegir Pluma en Configuración > Aplicaciones > Aplicaciones predeterminadas.",
                        L"Editor predeterminado", MB_OK | MB_ICONWARNING);
        }
    }

    void InvalidateDividers() {
        const Layout layout = ComputeLayout();
        InvalidateRect(m_hwnd, &layout.outlineDivider, FALSE);
        InvalidateRect(m_hwnd, &layout.splitDivider, FALSE);
    }

    void DragDividerTo(int mouseX) {
        const Layout layout = ComputeLayout();
        const int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
        if (m_dragDivider == Divider::Split) {
            const int contentX = layout.editor.left;
            const int contentW = layout.preview.right - contentX;
            if (contentW > dividerW) {
                const float ratio = static_cast<float>(mouseX - contentX - dividerW / 2) /
                                    static_cast<float>(contentW - dividerW);
                m_splitRatio = (std::clamp)(ratio, Pluma::Config::kMinSplitRatio, Pluma::Config::kMaxSplitRatio);
            }
        } else if (m_dragDivider == Divider::Outline) {
            const int logical = MulDiv(mouseX - dividerW / 2, 96, static_cast<int>(m_dpi));
            m_settings.outlineWidth =
                (std::clamp)(logical, Pluma::Config::kMinOutlineWidth, Pluma::Config::kMaxOutlineWidth);
        }
        RelayoutChildren();
        UpdateWindow(m_hwnd);
    }

    void PaintDivider(HDC hdc, const RECT& rc, Divider divider) const {
        if (IsRectEmpty(&rc)) return;
        const bool dark = IsDarkMode();
        COLORREF divBg = dark ? RGB(30, 30, 30) : RGB(243, 243, 243);
        COLORREF divLine = dark ? RGB(45, 45, 45) : RGB(220, 220, 220);
        if (m_dragDivider == divider || m_hoverDivider == divider) {
            divLine = dark ? RGB(86, 156, 214) : RGB(0, 120, 215);
        }
        HBRUSH hbr = CreateSolidBrush(divBg);
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);

        const int midX = rc.left + (rc.right - rc.left) / 2;
        RECT lineRc{ midX, rc.top, midX + 1, rc.bottom };
        HBRUSH hbrLine = CreateSolidBrush(divLine);
        FillRect(hdc, &lineRc, hbrLine);
        DeleteObject(hbrLine);
    }

    void UpdateWordWrapMenuCheck() {
        if (HMENU hMenu = GetMenu(m_hwnd)) {
            CheckMenuItem(hMenu, IDM_EDIT_WRAP, MF_BYCOMMAND | (m_editor.GetWordWrap() ? MF_CHECKED : MF_UNCHECKED));
        }
    }

    void UpdateThemeMenuRadio() {
        HMENU hMenu = GetMenu(m_hwnd);
        if (hMenu) {
            UINT checkId = IDM_VIEW_THEME_SYSTEM;
            if (m_appTheme == Pluma::Platform::AppTheme::Dark) checkId = IDM_VIEW_THEME_DARK;
            else if (m_appTheme == Pluma::Platform::AppTheme::Light) checkId = IDM_VIEW_THEME_LIGHT;
            CheckMenuRadioItem(hMenu, IDM_VIEW_THEME_SYSTEM, IDM_VIEW_THEME_LIGHT, checkId, MF_BYCOMMAND);
        }
    }

    static LRESULT CALLBACK StatusBarSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        auto* self = reinterpret_cast<MainWindow*>(dwRefData);
        if (!self) {
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        switch (uMsg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            if (rcClient.right <= 0 || rcClient.bottom <= 0) {
                EndPaint(hWnd, &ps);
                return 0;
            }

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
            HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

            bool dark = self->IsDarkMode();
            COLORREF bgCol = dark ? RGB(26, 26, 26) : RGB(243, 243, 243);
            COLORREF borderCol = dark ? RGB(45, 45, 45) : RGB(220, 220, 220);
            COLORREF textCol = dark ? RGB(200, 200, 200) : RGB(40, 40, 40);
            COLORREF sepCol = dark ? RGB(50, 50, 50) : RGB(215, 215, 215);

            // Fill background
            HBRUSH hbrBg = CreateSolidBrush(bgCol);
            FillRect(memDC, &rcClient, hbrBg);
            DeleteObject(hbrBg);

            // Top hairline border
            RECT rcTopBorder = { 0, 0, rcClient.right, 1 };
            HBRUSH hbrBorder = CreateSolidBrush(borderCol);
            FillRect(memDC, &rcTopBorder, hbrBorder);
            DeleteObject(hbrBorder);

            // Font (cached per DPI)
            if (!self->m_statusFont) {
                self->m_statusFont.reset(CreateFontW(
                    -Pluma::Platform::ScaleForDpi(12, self->m_dpi), 0, 0, 0,
                    FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"));
            }
            HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, self->m_statusFont.get()));
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, textCol);

            int partCount = static_cast<int>(SendMessageW(hWnd, SB_GETPARTS, 0, 0));
            for (int i = 0; i < partCount; ++i) {
                RECT rcPart;
                SendMessageW(hWnd, SB_GETRECT, i, reinterpret_cast<LPARAM>(&rcPart));
                if (rcPart.right <= rcPart.left) continue;

                wchar_t text[128] = { 0 };
                SendMessageW(hWnd, SB_GETTEXTW, i, reinterpret_cast<LPARAM>(text));

                RECT rcText = rcPart;
                rcText.left += Pluma::Platform::ScaleForDpi(8, self->m_dpi);
                rcText.right -= Pluma::Platform::ScaleForDpi(8, self->m_dpi);
                DrawTextW(memDC, text, -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

                if (i < partCount - 1 && rcPart.right < rcClient.right - 20) {
                    int h = rcPart.bottom - rcPart.top;
                    int sepTop = rcPart.top + h / 4;
                    int sepBottom = rcPart.bottom - h / 4;
                    RECT rcSep = { rcPart.right, sepTop, rcPart.right + 1, sepBottom };
                    HBRUSH hbrSep = CreateSolidBrush(sepCol);
                    FillRect(memDC, &rcSep, hbrSep);
                    DeleteObject(hbrSep);
                }
            }

            // Size grip dots
            if (!IsZoomed(self->m_hwnd)) {
                COLORREF dotCol = dark ? RGB(80, 80, 80) : RGB(180, 180, 180);
                int gx = rcClient.right - Pluma::Platform::ScaleForDpi(16, self->m_dpi);
                int gy = rcClient.bottom - Pluma::Platform::ScaleForDpi(16, self->m_dpi);
                int dotSize = Pluma::Platform::ScaleForDpi(2, self->m_dpi);
                int step = Pluma::Platform::ScaleForDpi(4, self->m_dpi);

                HBRUSH hbrDot = CreateSolidBrush(dotCol);
                for (int r = 0; r < 3; ++r) {
                    for (int c = 2 - r; c < 3; ++c) {
                        RECT rcDot{ gx + c * step, gy + r * step,
                                    gx + c * step + dotSize, gy + r * step + dotSize };
                        FillRect(memDC, &rcDot, hbrDot);
                    }
                }
                DeleteObject(hbrDot);
            }

            SelectObject(memDC, oldFont);

            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, StatusBarSubclassProc, uIdSubclass);
            break;
        }

        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Pluma::Editor::EditorView m_editor;
    Pluma::Preview::PreviewView m_preview;
    std::unique_ptr<Pluma::Sync::SyncScrollController> m_syncScroll;
    std::unique_ptr<Pluma::Markdown::ParseWorker> m_parseWorker;

    uint64_t m_docVersion = 0;
    uint64_t m_lastAppliedVersion = 0;

    ViewMode m_viewMode = ViewMode::Split;
    float m_splitRatio = 0.5f;
    Divider m_dragDivider = Divider::None;
    Divider m_hoverDivider = Divider::None;
    int m_splitterWidth = 6;

    Pluma::Config::Settings m_settings;
    std::filesystem::path m_settingsPath;
    Pluma::Outline::OutlinePanel m_outline;
    std::vector<Pluma::Outline::Heading> m_headings; // From the last applied parse
    UINT m_dpi = 96;
    HCURSOR m_hSizeWeCursor = nullptr;

    std::filesystem::path m_currentPath;
    Pluma::IO::Encoding m_encoding = Pluma::IO::Encoding::Utf8;
    Pluma::IO::LineEnding m_lineEnding = Pluma::IO::LineEnding::CRLF;
    bool m_hasBom = false;

    bool m_reparsePosted = false;
    bool m_statsValid = false;
    Pluma::Editor::EditorView::DocumentStats m_cachedStats{};
    std::wstring m_statusTexts[5];

    struct FontDeleter {
        void operator()(HFONT font) const noexcept { if (font) DeleteObject(font); }
    };
    std::unique_ptr<std::remove_pointer_t<HFONT>, FontDeleter> m_statusFont;

    HWND m_hwndStatusBar = nullptr;
    HWND m_hFindReplaceDlg = nullptr;
    FINDREPLACEW m_fr{};
    wchar_t m_szFindWhat[256] = L"";
    wchar_t m_szReplaceWith[256] = L"";
};

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nShowCmd) {
    // Initialize dark mode support for the process before creating any windows
    Pluma::Platform::InitializeDarkMode();

    // Initialize COM for modern IFileDialog and Direct2D/DirectWrite
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX iccex{};
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&iccex);

    g_uFindReplaceMsg = RegisterWindowMessageW(FINDMSGSTRINGW);

    // Check command line arguments for initial file opening (F-01, M1.5)
    std::filesystem::path initialFilePath;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1) {
            initialFilePath = argv[1];
        }
        LocalFree(argv);
    }

    MainWindow mainWindow(hInstance);
    if (!mainWindow.Create(nShowCmd, initialFilePath)) {
        CoUninitialize();
        return 1;
    }

    HACCEL hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATOR));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        HWND hDlg = mainWindow.GetFindReplaceDialog();
        if (hDlg && IsDialogMessageW(hDlg, &msg)) {
            continue;
        }
        if (!hAccelTable || !TranslateAcceleratorW(mainWindow.GetHwnd(), hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
