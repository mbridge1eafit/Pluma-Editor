#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
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
#include "../markdown/parse_worker.h"
#include "../preview/preview_view.h"
#include "../sync/sync_scroll.h"
#include "../export/html_exporter.h"
#include "../export/pdf_exporter.h"
#include "../../res/resource.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PlumaMainWindowClass";
constexpr int kEditorControlId = 1010;
bool g_firstPaintSignaled = false;

enum class ViewMode {
    EditorOnly,
    Split,
    PreviewOnly
};

void SignalStartupEvent() {
    if (g_firstPaintSignaled) {
        return;
    }
    g_firstPaintSignaled = true;

    // Signal named event for startup benchmark if listening
    DWORD pid = GetCurrentProcessId();
    std::wstring eventName = L"Local\\PlumaStartupEvent_" + std::to_wstring(pid);
    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
    if (hEvent) {
        SetEvent(hEvent);
        CloseHandle(hEvent);
    }
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
        wcex.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = nullptr;
        wcex.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
        wcex.lpszClassName = kWindowClassName;
        wcex.hIconSm = nullptr;

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
        switch (msg) {
        case WM_CREATE: {
            bool darkMode = Pluma::Platform::IsSystemDarkMode();
            Pluma::Platform::ApplyThemeToWindow(m_hwnd, darkMode);

            RECT rc;
            GetClientRect(m_hwnd, &rc);
            int width = (std::max)(100, (int)(rc.right - rc.left));
            int height = (std::max)(100, (int)(rc.bottom - rc.top));

            m_editor.Create(m_hwnd, m_hInstance, kEditorControlId, 0, 0, width / 2, height);
            m_editor.ApplyTheme(darkMode);

            m_preview.Create(m_hwnd, m_hInstance, width / 2, 0, width / 2, height);
            m_preview.SetDarkMode(darkMode);

            m_syncScroll = std::make_unique<Pluma::Sync::SyncScrollController>(&m_editor, &m_preview);
            m_parseWorker = std::make_unique<Pluma::Markdown::ParseWorker>(m_hwnd, Pluma::Markdown::WM_USER_PARSE_COMPLETE);

            RelayoutChildren();
            TriggerParse();
            m_editor.SetFocus();
            return 0;
        }

        case WM_SIZE: {
            RelayoutChildren();
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
            break;
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
            if (lParam && wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
                bool darkMode = Pluma::Platform::IsSystemDarkMode();
                Pluma::Platform::ApplyThemeToWindow(m_hwnd, darkMode);
                m_editor.ApplyTheme(darkMode);
                m_preview.SetDarkMode(darkMode);
                InvalidateRect(m_hwnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            m_dpi = HIWORD(wParam);
            m_preview.SetDpi(m_dpi);
            auto* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(m_hwnd, nullptr,
                         prcNewWindow->left, prcNewWindow->top,
                         prcNewWindow->right - prcNewWindow->left,
                         prcNewWindow->bottom - prcNewWindow->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            RelayoutChildren();
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

                bool dark = Pluma::Platform::IsSystemDarkMode();
                COLORREF divColor = dark ? RGB(45, 45, 45) : RGB(225, 225, 225);
                HBRUSH hbr = CreateSolidBrush(divColor);
                FillRect(hdc, &divRc, hbr);
                DeleteObject(hbr);
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

            case IDM_VIEW_EDITOR_ONLY:
                m_viewMode = ViewMode::EditorOnly;
                if (m_syncScroll) m_syncScroll->SetEnabled(false);
                RelayoutChildren();
                return 0;
            case IDM_VIEW_SPLIT:
                m_viewMode = ViewMode::Split;
                if (m_syncScroll) m_syncScroll->SetEnabled(true);
                RelayoutChildren();
                return 0;
            case IDM_VIEW_PREVIEW_ONLY:
                m_viewMode = ViewMode::PreviewOnly;
                if (m_syncScroll) m_syncScroll->SetEnabled(false);
                RelayoutChildren();
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
};

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nShowCmd) {
    // Initialize COM for modern IFileDialog and Direct2D/DirectWrite
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX iccex{};
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&iccex);

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
        if (!hAccelTable || !TranslateAcceleratorW(mainWindow.GetHwnd(), hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
