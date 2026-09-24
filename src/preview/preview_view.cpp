#include "preview_view.h"
#include <shellapi.h>
#include <algorithm>

namespace Pluma::Preview {

PreviewView::PreviewView() {
    m_colors = PreviewThemeColors::Light();
}

PreviewView::~PreviewView() {
    DiscardDirect2DResources();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool PreviewView::Create(HWND hParent, HINSTANCE hInstance, int x, int y, int width, int height) {
    m_hParent = hParent;
    m_hInstance = hInstance;

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = StaticWndProc;
    wcex.hInstance = m_hInstance;
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.lpszClassName = kPreviewViewClassName;

    RegisterClassExW(&wcex);

    m_hwnd = CreateWindowExW(
        0,
        kPreviewViewClassName,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
        x, y, width, height,
        m_hParent, nullptr, m_hInstance, this
    );

    if (!m_hwnd) {
        return false;
    }

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory),
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())
    );
    if (FAILED(hr)) return false;

    m_layout.Initialize(m_dwriteFactory.Get());

    m_hHandCursor = LoadCursorW(nullptr, IDC_HAND);
    m_hArrowCursor = LoadCursorW(nullptr, IDC_ARROW);

    m_pageHeight = height;

    return true;
}

LRESULT CALLBACK PreviewView::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreviewView* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<PreviewView*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->m_hwnd = hwnd;
        }
    } else {
        self = reinterpret_cast<PreviewView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT PreviewView::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT:
        Render();
        return 0;

    case WM_ERASEBKGND:
        return 1; // Prevent flicker; Direct2D clears background

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        OnResize(width, height);
        return 0;
    }

    case WM_VSCROLL: {
        int action = LOWORD(wParam);
        int newPos = m_scrollPos;
        switch (action) {
        case SB_LINEUP:        newPos -= 24; break;
        case SB_LINEDOWN:      newPos += 24; break;
        case SB_PAGEUP:        newPos -= m_pageHeight; break;
        case SB_PAGEDOWN:      newPos += m_pageHeight; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_TRACKPOS;
            GetScrollInfo(m_hwnd, SB_VERT, &si);
            newPos = si.nTrackPos;
            break;
        }
        default: break;
        }
        ScrollTo(newPos);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        int lines = 3;
        UINT scrollLines = 3;
        SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
        if (scrollLines > 0) lines = static_cast<int>(scrollLines);

        int scrollAmount = (delta / WHEEL_DELTA) * lines * 20;
        ScrollTo(m_scrollPos - scrollAmount);
        return 0;
    }

    case WM_MOUSEMOVE: {
        float x = static_cast<float>(LOWORD(lParam));
        float y = static_cast<float>(HIWORD(lParam));
        float contentY = y + static_cast<float>(m_scrollPos);

        std::string link = m_layout.HitTestLink(x, contentY);
        if (link != m_hoverUrl) {
            m_hoverUrl = std::move(link);
            SetCursor(m_hoverUrl.empty() ? m_hArrowCursor : m_hHandCursor);
        }
        return 0;
    }

    case WM_SETCURSOR: {
        if (!m_hoverUrl.empty()) {
            SetCursor(m_hHandCursor);
            return TRUE;
        }
        break;
    }

    case WM_LBUTTONUP: {
        if (!m_hoverUrl.empty()) {
            if (m_hoverUrl[0] == '#') {
                ScrollToAnchor(m_hoverUrl);
            } else {
                int count = MultiByteToWideChar(CP_UTF8, 0, m_hoverUrl.c_str(), -1, nullptr, 0);
                if (count > 0) {
                    std::wstring wideUrl(count, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, m_hoverUrl.c_str(), -1, wideUrl.data(), count);
                    ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        DiscardDirect2DResources();
        return 0;
    }

    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

bool PreviewView::EnsureDirect2DResources() {
    if (m_renderTarget) return true;
    if (!m_d2dFactory || !m_hwnd) return false;

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(
        (std::max)(1u, static_cast<UINT32>(rc.right - rc.left)),
        (std::max)(1u, static_cast<UINT32>(rc.bottom - rc.top)));

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(m_hwnd, size);

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(rtProps, hwndProps, &m_renderTarget);
    if (FAILED(hr)) return false;

    m_renderTarget->CreateSolidColorBrush(m_colors.text, &m_brushText);
    m_renderTarget->CreateSolidColorBrush(m_colors.headingText, &m_brushHeading);
    m_renderTarget->CreateSolidColorBrush(m_colors.linkText, &m_brushLink);
    m_renderTarget->CreateSolidColorBrush(m_colors.codeText, &m_brushCodeText);
    m_renderTarget->CreateSolidColorBrush(m_colors.codeBackground, &m_brushCodeBg);
    m_renderTarget->CreateSolidColorBrush(m_colors.codeBorder, &m_brushCodeBorder);
    m_renderTarget->CreateSolidColorBrush(m_colors.inlineCodeBg, &m_brushInlineCodeBg);
    m_renderTarget->CreateSolidColorBrush(m_colors.blockquoteBorder, &m_brushBlockquoteBorder);
    m_renderTarget->CreateSolidColorBrush(m_colors.ruleLine, &m_brushRuleLine);
    m_renderTarget->CreateSolidColorBrush(m_colors.tableBorder, &m_brushTableBorder);
    m_renderTarget->CreateSolidColorBrush(m_colors.tableHeaderBg, &m_brushTableHeaderBg);

    return true;
}

void PreviewView::DiscardDirect2DResources() {
    m_brushText.Reset();
    m_brushHeading.Reset();
    m_brushLink.Reset();
    m_brushCodeText.Reset();
    m_brushCodeBg.Reset();
    m_brushCodeBorder.Reset();
    m_brushInlineCodeBg.Reset();
    m_brushBlockquoteBorder.Reset();
    m_brushRuleLine.Reset();
    m_brushTableBorder.Reset();
    m_brushTableHeaderBg.Reset();
    m_renderTarget.Reset();
}

void PreviewView::Render() {
    PAINTSTRUCT ps{};
    BeginPaint(m_hwnd, &ps);

    if (EnsureDirect2DResources()) {
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(m_colors.background);

        // Apply scroll translation transform
        m_renderTarget->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, -static_cast<float>(m_scrollPos)));

        float viewTop = static_cast<float>(m_scrollPos);
        float viewBottom = viewTop + static_cast<float>(m_pageHeight);

        for (const auto& block : m_layout.GetBlocks()) {
            // Viewport culling
            if (block.bounds.bottom < viewTop || block.bounds.top > viewBottom) {
                continue;
            }

            switch (block.type) {
            case Markdown::BlockType::Heading: {
                if (block.textLayout) {
                    m_renderTarget->DrawTextLayout(
                        D2D1::Point2F(block.bounds.left, block.bounds.top),
                        block.textLayout.Get(),
                        m_brushHeading.Get()
                    );
                }
                // Bottom rule for H1 and H2
                if (block.level <= 2) {
                    m_renderTarget->DrawLine(
                        D2D1::Point2F(block.bounds.left, block.bounds.bottom - 4.0f),
                        D2D1::Point2F(block.bounds.right, block.bounds.bottom - 4.0f),
                        m_brushRuleLine.Get(),
                        1.0f
                    );
                }
                break;
            }

            case Markdown::BlockType::Paragraph: {
                // Draw inline code pills
                for (const auto& pill : block.inlineCodePills) {
                    m_renderTarget->FillRoundedRectangle(
                        D2D1::RoundedRect(pill, 3.0f, 3.0f),
                        m_brushInlineCodeBg.Get()
                    );
                }
                if (block.textLayout) {
                    m_renderTarget->DrawTextLayout(
                        D2D1::Point2F(block.bounds.left, block.bounds.top),
                        block.textLayout.Get(),
                        m_brushText.Get()
                    );
                }
                break;
            }

            case Markdown::BlockType::Blockquote: {
                // If it's the vertical bar block
                if (block.bounds.right - block.bounds.left <= 8.0f) {
                    m_renderTarget->FillRectangle(block.bounds, m_brushBlockquoteBorder.Get());
                } else if (block.textLayout) {
                    for (const auto& pill : block.inlineCodePills) {
                        m_renderTarget->FillRoundedRectangle(
                            D2D1::RoundedRect(pill, 3.0f, 3.0f),
                            m_brushInlineCodeBg.Get()
                        );
                    }
                    m_renderTarget->DrawTextLayout(
                        D2D1::Point2F(block.bounds.left, block.bounds.top),
                        block.textLayout.Get(),
                        m_brushText.Get()
                    );
                }
                break;
            }

            case Markdown::BlockType::CodeBlock: {
                D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(block.bounds, 4.0f, 4.0f);
                m_renderTarget->FillRoundedRectangle(rrect, m_brushCodeBg.Get());
                m_renderTarget->DrawRoundedRectangle(rrect, m_brushCodeBorder.Get(), 1.0f);

                if (block.textLayout) {
                    m_renderTarget->DrawTextLayout(
                        D2D1::Point2F(block.bounds.left + 12.0f, block.bounds.top + 12.0f),
                        block.textLayout.Get(),
                        m_brushCodeText.Get()
                    );
                }
                break;
            }

            case Markdown::BlockType::ThematicBreak: {
                m_renderTarget->DrawLine(
                    D2D1::Point2F(block.bounds.left, block.bounds.top),
                    D2D1::Point2F(block.bounds.right, block.bounds.top),
                    m_brushRuleLine.Get(),
                    1.5f
                );
                break;
            }

            case Markdown::BlockType::ListItem: {
                if (block.isTask) {
                    // Draw checkbox
                    D2D1_RECT_F boxRect = D2D1::RectF(
                        block.bounds.left + 4.0f, block.bounds.top + 2.0f,
                        block.bounds.left + 18.0f, block.bounds.top + 16.0f
                    );
                    m_renderTarget->DrawRectangle(boxRect, m_brushText.Get(), 1.5f);
                    if (block.isTaskChecked) {
                        m_renderTarget->DrawLine(
                            D2D1::Point2F(boxRect.left + 3.0f, boxRect.top + 7.0f),
                            D2D1::Point2F(boxRect.left + 6.0f, boxRect.bottom - 4.0f),
                            m_brushText.Get(), 2.0f
                        );
                        m_renderTarget->DrawLine(
                            D2D1::Point2F(boxRect.left + 6.0f, boxRect.bottom - 4.0f),
                            D2D1::Point2F(boxRect.right - 3.0f, boxRect.top + 3.0f),
                            m_brushText.Get(), 2.0f
                        );
                    }
                } else if (block.isOrdered) {
                    // Number was rendered into text or separate
                } else {
                    // Bullet
                    D2D1_ELLIPSE bullet = D2D1::Ellipse(
                        D2D1::Point2F(block.bounds.left + 10.0f, block.bounds.top + 9.0f),
                        3.0f, 3.0f
                    );
                    m_renderTarget->FillEllipse(bullet, m_brushText.Get());
                }

                if (block.textLayout) {
                    for (const auto& pill : block.inlineCodePills) {
                        m_renderTarget->FillRoundedRectangle(
                            D2D1::RoundedRect(pill, 3.0f, 3.0f),
                            m_brushInlineCodeBg.Get()
                        );
                    }
                    m_renderTarget->DrawTextLayout(
                        D2D1::Point2F(block.bounds.left + 28.0f, block.bounds.top),
                        block.textLayout.Get(),
                        m_brushText.Get()
                    );
                }
                break;
            }

            case Markdown::BlockType::Table: {
                for (const auto& row : block.tableRows) {
                    for (const auto& cell : row) {
                        if (cell.isHeader) {
                            m_renderTarget->FillRectangle(cell.rect, m_brushTableHeaderBg.Get());
                        }
                        m_renderTarget->DrawRectangle(cell.rect, m_brushTableBorder.Get(), 1.0f);
                        if (cell.textLayout) {
                            m_renderTarget->DrawTextLayout(
                                D2D1::Point2F(cell.rect.left + 8.0f, cell.rect.top + 6.0f),
                                cell.textLayout.Get(),
                                m_brushText.Get()
                            );
                        }
                    }
                }
                break;
            }

            default:
                break;
            }
        }

        HRESULT hr = m_renderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            DiscardDirect2DResources();
        }
    }

    EndPaint(m_hwnd, &ps);
}

void PreviewView::SetBlockTree(std::unique_ptr<Markdown::BlockTree> tree) {
    m_tree = std::move(tree);
    if (!m_tree) return;

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);

    m_layout.ComputeLayout(*m_tree, width, m_dpi, m_colors);
    UpdateScrollbars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::SetDarkMode(bool isDark) {
    m_isDarkMode = isDark;
    m_colors = isDark ? PreviewThemeColors::Dark() : PreviewThemeColors::Light();
    DiscardDirect2DResources();

    if (m_tree) {
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        m_layout.ComputeLayout(*m_tree, static_cast<float>(rc.right - rc.left), m_dpi, m_colors);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::SetDpi(UINT dpi) {
    m_dpi = dpi;
    if (m_tree) {
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        m_layout.ComputeLayout(*m_tree, static_cast<float>(rc.right - rc.left), m_dpi, m_colors);
        UpdateScrollbars();
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::OnResize(int width, int height) {
    m_pageHeight = height;

    if (m_renderTarget) {
        m_renderTarget->Resize(D2D1::SizeU(
            (std::max)(1u, static_cast<UINT32>(width)),
            (std::max)(1u, static_cast<UINT32>(height))));
    }

    if (m_tree) {
        m_layout.ComputeLayout(*m_tree, static_cast<float>(width), m_dpi, m_colors);
    }

    UpdateScrollbars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void PreviewView::UpdateScrollbars() {
    float totalH = m_layout.GetTotalHeight();
    m_maxScroll = (std::max)(0, static_cast<int>(totalH) - m_pageHeight);
    m_scrollPos = (std::clamp)(m_scrollPos, 0, m_maxScroll);

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = static_cast<int>(totalH);
    si.nPage = static_cast<UINT>(m_pageHeight);
    si.nPos = m_scrollPos;

    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

void PreviewView::ScrollTo(int newPos) {
    newPos = (std::clamp)(newPos, 0, m_maxScroll);
    if (newPos != m_scrollPos) {
        m_scrollPos = newPos;
        SetScrollPos(m_hwnd, SB_VERT, m_scrollPos, TRUE);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void PreviewView::ScrollToAnchor(std::string_view slug) {
    float y = m_layout.GetAnchorY(slug);
    if (y >= 0.0f) {
        ScrollTo(static_cast<int>(y - 16.0f));
    }
}

void PreviewView::ScrollToLine(int line) {
    for (const auto& block : m_layout.GetBlocks()) {
        if (block.startLine <= line && block.endLine >= line) {
            ScrollTo(static_cast<int>(block.bounds.top - 20.0f));
            break;
        }
    }
}

} // namespace Pluma::Preview
