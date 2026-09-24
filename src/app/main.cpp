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

#include "../platform/dpi.h"
#include "../platform/theme.h"
#include "../io/document_io.h"
#include "../editor/editor_view.h"
#include "../../res/resource.h"

namespace {

constexpr wchar_t kWindowClassName[] = L"PlumaMainWindowClass";
constexpr int kEditorControlId = 1010;
bool g_firstPaintSignaled = false;

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
            m_editor.Create(m_hwnd, m_hInstance, kEditorControlId,
                            0, 0, rc.right - rc.left, rc.bottom - rc.top);
            m_editor.ApplyTheme(darkMode);
            m_editor.SetFocus();
            return 0;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            m_editor.SetBounds(0, 0, width, height);
            return 0;
        }

        case WM_SETFOCUS:
            m_editor.SetFocus();
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
                }
            }
            return 0;
        }

        case WM_SETTINGCHANGE: {
            if (lParam && wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0) {
                bool darkMode = Pluma::Platform::IsSystemDarkMode();
                Pluma::Platform::ApplyThemeToWindow(m_hwnd, darkMode);
                m_editor.ApplyTheme(darkMode);
                InvalidateRect(m_hwnd, nullptr, TRUE);
            }
            return 0;
        }

        case WM_DPICHANGED: {
            auto* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(m_hwnd, nullptr,
                         prcNewWindow->left, prcNewWindow->top,
                         prcNewWindow->right - prcNewWindow->left,
                         prcNewWindow->bottom - prcNewWindow->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            (void)BeginPaint(m_hwnd, &ps);
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
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(m_hwnd, msg, wParam, lParam);
        }
    }

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    Pluma::Editor::EditorView m_editor;
    std::filesystem::path m_currentPath;
    Pluma::IO::Encoding m_encoding = Pluma::IO::Encoding::Utf8;
    Pluma::IO::LineEnding m_lineEnding = Pluma::IO::LineEnding::CRLF;
    bool m_hasBom = false;
};

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nShowCmd) {
    // Initialize COM for modern IFileDialog
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
