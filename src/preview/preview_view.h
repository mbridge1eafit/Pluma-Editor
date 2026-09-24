#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <string_view>
#include <functional>

#include "preview_layout.h"
#include "../markdown/block_tree.h"

namespace Pluma::Preview {

using Microsoft::WRL::ComPtr;

constexpr wchar_t kPreviewViewClassName[] = L"PlumaPreviewViewClass";

class PreviewView {
public:
    PreviewView();
    ~PreviewView();

    PreviewView(const PreviewView&) = delete;
    PreviewView& operator=(const PreviewView&) = delete;

    bool Create(HWND hParent, HINSTANCE hInstance, int x, int y, int width, int height);
    HWND GetHwnd() const noexcept { return m_hwnd; }

    void SetBlockTree(std::unique_ptr<Markdown::BlockTree> tree);
    void SetDarkMode(bool isDark);
    void SetDpi(UINT dpi);

    void ScrollToAnchor(std::string_view slug);
    void ScrollToLine(int line); // For F-14 sync scroll
    int GetScrollPos() const noexcept { return m_scrollPos; }
    int GetLineForCurrentScroll() const;

    void SetOnScrollCallback(std::function<void(int line)> callback) {
        m_onScrollCallback = std::move(callback);
    }

    void OnResize(int width, int height);

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool EnsureDirect2DResources();
    void DiscardDirect2DResources();
    void Render();
    void UpdateScrollbars();
    void ScrollTo(int newPos, bool notify = true);

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;

    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1HwndRenderTarget> m_renderTarget;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> m_brushText;
    ComPtr<ID2D1SolidColorBrush> m_brushHeading;
    ComPtr<ID2D1SolidColorBrush> m_brushLink;
    ComPtr<ID2D1SolidColorBrush> m_brushCodeText;
    ComPtr<ID2D1SolidColorBrush> m_brushCodeBg;
    ComPtr<ID2D1SolidColorBrush> m_brushCodeBorder;
    ComPtr<ID2D1SolidColorBrush> m_brushInlineCodeBg;
    ComPtr<ID2D1SolidColorBrush> m_brushBlockquoteBorder;
    ComPtr<ID2D1SolidColorBrush> m_brushRuleLine;
    ComPtr<ID2D1SolidColorBrush> m_brushTableBorder;
    ComPtr<ID2D1SolidColorBrush> m_brushTableHeaderBg;

    LayoutEngine m_layout;
    std::unique_ptr<Markdown::BlockTree> m_tree;

    bool m_isDarkMode = false;
    PreviewThemeColors m_colors = PreviewThemeColors::Light();
    UINT m_dpi = 96;

    int m_scrollPos = 0;
    int m_maxScroll = 0;
    int m_pageHeight = 0;

    std::string m_hoverUrl;
    HCURSOR m_hHandCursor = nullptr;
    HCURSOR m_hArrowCursor = nullptr;
    std::function<void(int line)> m_onScrollCallback;
};

} // namespace Pluma::Preview
