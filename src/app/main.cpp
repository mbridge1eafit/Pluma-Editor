#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shobjidl.h> // IFileDialog
#include <shellapi.h>
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
#include "../../res/resource.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PlumaMainWindowClass";
constexpr int kEditorControlId = 1010;
constexpr int kStatusBarControlId = 1011;
UINT g_uFindReplaceMsg = 0;
bool g_firstPaintSignaled = false;

std::wstring Utf8ToUtf16(std::string_view utf8) {
    if (utf8.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

std::string Utf16ToUtf8(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), result.data(), count, nullptr, nullptr);
    return result;
}

enum class ViewMode {
    EditorOnly,
    Split,
    PreviewOnly
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

        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
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

        int width = Pluma::Platform::ScaleForDpi(1024, 96);
        int height = Pluma::Platform::ScaleForDpi(720, 96);

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

        ShowWindow(m_hwnd, nShowCmd);
        UpdateWindow(m_hwnd);

        if (!initialFile.empty()) {
            OpenFile(initialFile);
        } else {
            UpdateTitle();
        }

        return true;
    }

    HWND GetHwnd() const noexcept { return m_hwnd; }
    HWND GetFindReplaceDialog() const noexcept { return m_hFindReplaceDlg; }

    void OpenFile(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            return;
        }

        try {
            auto doc = Pluma::IO::ReadDocument(path);
            m_currentPath = path;
            m_encoding = doc.encoding;
            m_lineEnding = doc.lineEnding;
            m_hasBom = doc.hasBom;

            m_editor.SetText(doc.contentUtf8);
            UpdateTitle();
            TriggerParse();
            UpdateStatusBar();
        } catch (...) {
            MessageBoxW(m_hwnd, L"No se pudo abrir el archivo especificado.",
                        L"Error", MB_OK | MB_ICONERROR);
        }
    }

    bool SaveFile() {
        if (m_currentPath.empty()) {
            return SaveAsFile();
        }

        try {
            std::string content = m_editor.GetText();
            Pluma::IO::WriteDocumentAtomic(m_currentPath, content, m_encoding, m_lineEnding);
            m_editor.SetSavePoint();
            UpdateTitle();
            UpdateStatusBar();
            return true;
        } catch (...) {
            MessageBoxW(m_hwnd, L"Error al guardar el archivo.",
                        L"Error", MB_OK | MB_ICONERROR);
            return false;
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
                    m_currentPath = pszFilePath;
                    CoTaskMemFree(pszFilePath);
                    pItem->Release();
                    pFileSave->Release();
                    return SaveFile();
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
                    options.title = m_currentPath.empty() ? "Pluma Document" : m_currentPath.stem().string();
                    options.theme = Pluma::Export::HtmlTheme::Auto;

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
                    options.title = m_currentPath.empty() ? "Pluma Document" : m_currentPath.stem().string();
                    options.pageSize = Pluma::Export::PageSize::A4;
                    options.marginMm = 20.0f;

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
        m_lineEnding = Pluma::IO::LineEnding::CRLF;
        m_hasBom = false;
        m_editor.SetText("");
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
        if (!m_parseWorker) return;
        m_docVersion++;
        std::string text = m_editor.GetText();
        m_parseWorker->RequestParse(std::move(text), m_docVersion);
    }

    void RelayoutChildren() {
        if (!m_hwnd || !m_editor.GetHwnd() || !m_preview.GetHwnd()) return;

        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) return;

        int sbHeight = 0;
        if (m_hwndStatusBar && IsWindowVisible(m_hwndStatusBar)) {
            SendMessageW(m_hwndStatusBar, WM_SIZE, 0, 0);
            RECT rcSB{};
            GetWindowRect(m_hwndStatusBar, &rcSB);
            sbHeight = rcSB.bottom - rcSB.top;
        }
        h = (std::max)(10, h - sbHeight);

        switch (m_viewMode) {
        case ViewMode::EditorOnly:
            m_editor.SetBounds(0, 0, w, h);
            ShowWindow(m_editor.GetHwnd(), SW_SHOW);
            ShowWindow(m_preview.GetHwnd(), SW_HIDE);
            break;

        case ViewMode::PreviewOnly:
            ShowWindow(m_editor.GetHwnd(), SW_HIDE);
            SetWindowPos(m_preview.GetHwnd(), nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
            break;

        case ViewMode::Split: {
            int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
            int editorW = static_cast<int>((w - dividerW) * m_splitRatio);
            editorW = (std::clamp)(editorW, 50, (std::max)(50, w - dividerW - 50));
            int previewX = editorW + dividerW;
            int previewW = (std::max)(50, w - previewX);

            m_editor.SetBounds(0, 0, editorW, h);
            ShowWindow(m_editor.GetHwnd(), SW_SHOW);
            SetWindowPos(m_preview.GetHwnd(), nullptr, previewX, 0, previewW, h, SWP_NOZORDER | SWP_SHOWWINDOW);
            break;
        }
        }

        HMENU hMenu = GetMenu(m_hwnd);
        if (hMenu) {
            UINT checkId = IDM_VIEW_SPLIT;
            if (m_viewMode == ViewMode::EditorOnly) checkId = IDM_VIEW_EDITOR_ONLY;
            else if (m_viewMode == ViewMode::PreviewOnly) checkId = IDM_VIEW_PREVIEW_ONLY;
            CheckMenuRadioItem(hMenu, IDM_VIEW_EDITOR_ONLY, IDM_VIEW_PREVIEW_ONLY, checkId, MF_BYCOMMAND);
        }

        InvalidateRect(m_hwnd, nullptr, FALSE);
    }

    void UpdateStatusBarParts() {
        if (!m_hwndStatusBar) return;
        int p0 = Pluma::Platform::ScaleForDpi(120, m_dpi);
        int p1 = Pluma::Platform::ScaleForDpi(340, m_dpi);
        int p2 = Pluma::Platform::ScaleForDpi(460, m_dpi);
        int p3 = Pluma::Platform::ScaleForDpi(550, m_dpi);
        int sbParts[5] = { p0, p1, p2, p3, -1 };
        SendMessageW(m_hwndStatusBar, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(sbParts));
    }

    void UpdateStatusBar() {
        if (!m_hwndStatusBar) return;

        auto pos = m_editor.GetCursorPosition();
        auto stats = m_editor.GetDocumentStats();

        wchar_t bufPos[64];
        swprintf_s(bufPos, L"Lín %d, Col %d", pos.line, pos.column);
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(bufPos));

        wchar_t bufStats[128];
        swprintf_s(bufStats, L"%zu palabras, %zu caracteres", stats.words, stats.characters);
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(bufStats));

        const wchar_t* encStr = L"UTF-8";
        switch (m_encoding) {
        case Pluma::IO::Encoding::Utf8: encStr = m_hasBom ? L"UTF-8 BOM" : L"UTF-8"; break;
        case Pluma::IO::Encoding::Utf8Bom: encStr = L"UTF-8 BOM"; break;
        case Pluma::IO::Encoding::Utf16LE: encStr = L"UTF-16 LE"; break;
        case Pluma::IO::Encoding::Utf16BE: encStr = L"UTF-16 BE"; break;
        }
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(encStr));

        const wchar_t* leStr = (m_lineEnding == Pluma::IO::LineEnding::LF) ? L"LF" : L"CRLF";
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(leStr));

        const wchar_t* modeStr = L"Vista dividida";
        switch (m_viewMode) {
        case ViewMode::EditorOnly: modeStr = L"Solo editor"; break;
        case ViewMode::Split: modeStr = L"Vista dividida"; break;
        case ViewMode::PreviewOnly: modeStr = L"Solo vista previa"; break;
        }
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(modeStr));
    }

    void ShowOutlinePopup() {
        HMENU hMenu = CreatePopupMenu();
        if (!hMenu) return;

        std::string mdText = m_editor.GetText();
        auto tree = Pluma::Markdown::Md4cAdapter::Parse(mdText);

        struct HeadingItem {
            int line = 0;
            int level = 0;
            std::wstring title;

            HeadingItem(int ln, int lvl, std::wstring t)
                : line(ln), level(lvl), title(std::move(t)) {}
        };
        std::vector<HeadingItem> headings;

        if (tree && tree->root) {
            for (const auto& child : tree->root->children) {
                if (child && child->type == Pluma::Markdown::BlockType::Heading) {
                    std::string title;
                    for (const auto& span : child->inlineContent) {
                        title += span.text;
                    }
                    std::wstring wTitle = Utf8ToUtf16(title);
                    if (wTitle.empty()) wTitle = L"(Sin título)";
                    headings.emplace_back(child->startLine, child->level, std::move(wTitle));
                }
            }
        }

        if (headings.empty()) {
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"(No hay encabezados)");
        } else {
            for (size_t i = 0; i < headings.size(); ++i) {
                std::wstring prefix(static_cast<size_t>((headings[i].level - 1) * 2), L' ');
                for (int l = 0; l < headings[i].level; ++l) prefix += L'#';
                prefix += L" ";
                std::wstring itemText = prefix + headings[i].title;
                AppendMenuW(hMenu, MF_STRING, 50000 + i, itemText.c_str());
            }
        }

        POINT pt{};
        GetCursorPos(&pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);

        if (cmd >= 50000 && static_cast<size_t>(cmd - 50000) < headings.size()) {
            int line = headings[cmd - 50000].line;
            m_editor.GotoLine(line);
            m_preview.ScrollToLine(line);
            m_editor.SetFocus();
            UpdateStatusBar();
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

                    HWND hStatic = CreateWindowW(L"STATIC", L"Número de línea:", WS_CHILD | WS_VISIBLE,
                                                 pad, pad, 200, lblH, hwnd, nullptr, nullptr, nullptr);
                    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
                                                 pad, pad + lblH + 4, 210, editH, hwnd,
                                                 reinterpret_cast<HMENU>(101), nullptr, nullptr);
                    int btnY = pad + lblH + editH + spacing;
                    HWND hOk = CreateWindowW(L"BUTTON", L"Aceptar",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                             pad + 15, btnY, btnW, btnH, hwnd,
                                             reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                    HWND hCancel = CreateWindowW(L"BUTTON", L"Cancelar",
                                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                                 pad + 15 + btnW + 12, btnY, btnW, btnH, hwnd,
                                                 reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);

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

        int dlgW = Pluma::Platform::ScaleForDpi(260, m_dpi);
        int dlgH = Pluma::Platform::ScaleForDpi(160, m_dpi);

        RECT rc{};
        GetWindowRect(m_hwnd, &rc);
        int x = rc.left + (rc.right - rc.left - dlgW) / 2;
        int y = rc.top + (rc.bottom - rc.top - dlgH) / 2;
        HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"PlumaGotoLineClass", L"Ir a línea",
                                   WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                   x, y, dlgW, dlgH, m_hwnd, nullptr, m_hInstance, this);
        EnableWindow(m_hwnd, FALSE);
        MSG msg;
        while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
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
        SetFocus(m_hwnd);
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
            sptr_t start = m_editor.Call(SCI_GETSELECTIONSTART);
            sptr_t end = m_editor.Call(SCI_GETSELECTIONEND);
            if (start != end) {
                m_editor.Call(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(repStr.c_str()));
                TriggerParse();
                UpdateStatusBar();
            }
            m_editor.FindNext(findStr, matchCase, wholeWord, false, forward);
        } else if (pfr->Flags & FR_REPLACEALL) {
            std::string repStr = Utf16ToUtf8(pfr->lpstrReplaceWith);
            int replaced = m_editor.ReplaceAll(findStr, repStr, matchCase, wholeWord, false);
            if (replaced > 0) {
                TriggerParse();
                UpdateStatusBar();
            }
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

            RECT rc;
            GetClientRect(m_hwnd, &rc);
            int width = (std::max)(100, (int)(rc.right - rc.left));
            int height = (std::max)(100, (int)(rc.bottom - rc.top));

            m_hwndStatusBar = CreateStatusWindowW(
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                nullptr, m_hwnd, kStatusBarControlId);
            SetWindowSubclass(m_hwndStatusBar, StatusBarSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
            UpdateStatusBarParts();

            m_editor.Create(m_hwnd, m_hInstance, kEditorControlId, 0, 0, width / 2, height);
            m_editor.ApplyTheme(darkMode);

            m_preview.Create(m_hwnd, m_hInstance, width / 2, 0, width / 2, height);
            m_preview.SetDarkMode(darkMode);

            m_syncScroll = std::make_unique<Pluma::Sync::SyncScrollController>(&m_editor, &m_preview);
            m_parseWorker = std::make_unique<Pluma::Markdown::ParseWorker>(m_hwnd, Pluma::Markdown::WM_USER_PARSE_COMPLETE);

            RelayoutChildren();
            TriggerParse();
            UpdateStatusBar();
            UpdateThemeMenuRadio();
            m_editor.SetFocus();
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
            if (m_viewMode != ViewMode::PreviewOnly) {
                m_editor.SetFocus();
            }
            return 0;

        case WM_DROPFILES: {
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            wchar_t filePath[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH) > 0) {
                if (PromptSaveChanges()) {
                    OpenFile(filePath);
                }
            }
            DragFinish(hDrop);
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
                        TriggerParse();
                    }
                } else if (scn->nmhdr.code == SCN_UPDATEUI) {
                    UpdateStatusBar();
                    if (scn->updated & SC_UPDATE_V_SCROLL) {
                        if (m_syncScroll && m_viewMode == ViewMode::Split) {
                            m_syncScroll->OnEditorScrolled();
                        }
                    }
                }
            }
            return 0;
        }

        case Pluma::Markdown::WM_USER_PARSE_COMPLETE: {
            uint64_t version = static_cast<uint64_t>(wParam);
            auto* rawTree = reinterpret_cast<Pluma::Markdown::BlockTree*>(lParam);
            if (version >= m_lastAppliedVersion) {
                m_lastAppliedVersion = version;
                m_preview.SetBlockTree(std::unique_ptr<Pluma::Markdown::BlockTree>(rawTree));
            } else {
                delete rawTree;
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (m_viewMode == ViewMode::Split) {
                int mouseX = LOWORD(lParam);
                RECT rc{};
                GetClientRect(m_hwnd, &rc);
                int clientW = static_cast<int>(rc.right - rc.left);
                int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
                int editorW = static_cast<int>((clientW - dividerW) * m_splitRatio);
                editorW = (std::clamp)(editorW, 50, (std::max)(50, clientW - dividerW - 50));

                if (mouseX >= editorW && mouseX <= editorW + dividerW) {
                    m_isDraggingSplitter = true;
                    SetCapture(m_hwnd);
                    return 0;
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (m_isDraggingSplitter) {
                int mouseX = LOWORD(lParam);
                RECT rc{};
                GetClientRect(m_hwnd, &rc);
                int w = rc.right - rc.left;
                if (w > 0) {
                    m_splitRatio = (std::clamp)(static_cast<float>(mouseX) / static_cast<float>(w), 0.15f, 0.85f);
                    RelayoutChildren();
                }
                return 0;
            }

            if (m_viewMode == ViewMode::Split) {
                int mouseX = LOWORD(lParam);
                RECT rc{};
                GetClientRect(m_hwnd, &rc);
                int clientW = static_cast<int>(rc.right - rc.left);
                int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
                int editorW = static_cast<int>((clientW - dividerW) * m_splitRatio);
                editorW = (std::clamp)(editorW, 50, (std::max)(50, clientW - dividerW - 50));

                bool hovering = (mouseX >= editorW && mouseX <= editorW + dividerW);
                if (hovering != m_isHoveringSplitter) {
                    m_isHoveringSplitter = hovering;
                    RECT divRc{ editorW, 0, editorW + dividerW, static_cast<int>(rc.bottom - rc.top) };
                    InvalidateRect(m_hwnd, &divRc, FALSE);

                    if (hovering) {
                        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, m_hwnd, 0 };
                        TrackMouseEvent(&tme);
                    }
                }
            }
            break;
        }

        case WM_MOUSELEAVE: {
            if (m_isHoveringSplitter) {
                m_isHoveringSplitter = false;
                InvalidateRect(m_hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (m_isDraggingSplitter) {
                m_isDraggingSplitter = false;
                ReleaseCapture();
                return 0;
            }
            break;
        }

        case WM_SETCURSOR: {
            if (m_isDraggingSplitter) {
                SetCursor(m_hSizeWeCursor);
                return TRUE;
            }
            if (m_viewMode == ViewMode::Split && reinterpret_cast<HWND>(wParam) == m_hwnd) {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(m_hwnd, &pt);
                RECT rc{};
                GetClientRect(m_hwnd, &rc);
                int clientW = static_cast<int>(rc.right - rc.left);
                int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
                int editorW = static_cast<int>((clientW - dividerW) * m_splitRatio);
                editorW = (std::clamp)(editorW, 50, (std::max)(50, clientW - dividerW - 50));
                if (pt.x >= editorW && pt.x <= editorW + dividerW) {
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
            m_preview.SetDpi(m_dpi);
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
            if (m_viewMode == ViewMode::Split) {
                RECT rc;
                GetClientRect(m_hwnd, &rc);
                int clientW = static_cast<int>(rc.right - rc.left);
                int dividerW = Pluma::Platform::ScaleForDpi(m_splitterWidth, m_dpi);
                int editorW = static_cast<int>((clientW - dividerW) * m_splitRatio);
                editorW = (std::clamp)(editorW, 50, (std::max)(50, clientW - dividerW - 50));
                RECT divRc{ editorW, 0, editorW + dividerW, static_cast<int>(rc.bottom - rc.top) };

                bool dark = IsDarkMode();
                COLORREF divBg = dark ? RGB(30, 30, 30) : RGB(243, 243, 243);
                COLORREF divLine = dark ? RGB(45, 45, 45) : RGB(220, 220, 220);
                if (m_isDraggingSplitter || m_isHoveringSplitter) {
                    divLine = dark ? RGB(86, 156, 214) : RGB(0, 120, 215);
                }
                HBRUSH hbr = CreateSolidBrush(divBg);
                FillRect(hdc, &divRc, hbr);
                DeleteObject(hbr);

                int midX = divRc.left + (divRc.right - divRc.left) / 2;
                RECT lineRc{ midX, 0, midX + 1, divRc.bottom };
                HBRUSH hbrLine = CreateSolidBrush(divLine);
                FillRect(hdc, &lineRc, hbrLine);
                DeleteObject(hbrLine);
            }
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
                m_viewMode = ViewMode::EditorOnly;
                if (m_syncScroll) m_syncScroll->SetEnabled(false);
                RelayoutChildren();
                UpdateStatusBar();
                return 0;
            case IDM_VIEW_SPLIT:
                m_viewMode = ViewMode::Split;
                if (m_syncScroll) m_syncScroll->SetEnabled(true);
                RelayoutChildren();
                UpdateStatusBar();
                return 0;
            case IDM_VIEW_PREVIEW_ONLY:
                m_viewMode = ViewMode::PreviewOnly;
                if (m_syncScroll) m_syncScroll->SetEnabled(false);
                RelayoutChildren();
                UpdateStatusBar();
                return 0;
            case IDM_VIEW_OUTLINE:
                ShowOutlinePopup();
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
                DestroyWindow(m_hwnd);
            }
            return 0;

        case WM_DESTROY:
            if (m_parseWorker) {
                m_parseWorker->Stop();
            }
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }

    Pluma::Platform::AppTheme m_appTheme = Pluma::Platform::AppTheme::System;
    bool m_isHoveringSplitter = false;

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

        UpdateThemeMenuRadio();
        Pluma::Platform::RefreshWindowFrame(m_hwnd);

        if (m_hwndStatusBar) {
            InvalidateRect(m_hwndStatusBar, nullptr, TRUE);
        }
        InvalidateRect(m_hwnd, nullptr, TRUE);
        UpdateStatusBar();
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

            // Font
            HFONT hFont = CreateFontW(
                -Pluma::Platform::ScaleForDpi(12, self->m_dpi), 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, hFont));
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
            DeleteObject(hFont);

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
    bool m_isDraggingSplitter = false;
    int m_splitterWidth = 6;
    UINT m_dpi = 96;
    HCURSOR m_hSizeWeCursor = nullptr;

    std::filesystem::path m_currentPath;
    Pluma::IO::Encoding m_encoding = Pluma::IO::Encoding::Utf8;
    Pluma::IO::LineEnding m_lineEnding = Pluma::IO::LineEnding::CRLF;
    bool m_hasBom = false;

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
    if (argv && argc > 1) {
        initialFilePath = argv[1];
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
