#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <functional>

#include "preview_layout.h"
#include "preview_renderer.h"
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

    // Directory used to resolve relative links (e.g. "docs/otro.md").
    void SetBaseDirectory(std::filesystem::path dir) { m_baseDirectory = std::move(dir); }

    void ScrollToAnchor(std::string_view slug);
    void ScrollToLine(int line); // For F-14 sync scroll
    void ResetScroll();
    int GetScrollPos() const noexcept { return static_cast<int>(m_scrollPos); }
    int GetLineForCurrentScroll() const;

    void SetOnScrollCallback(std::function<void(int line)> callback) {
        m_onScrollCallback = std::move(callback);
    }

    // Invoked when a link to a local Markdown document is clicked.
    void SetOnOpenDocumentCallback(std::function<void(const std::filesystem::path&)> callback) {
        m_onOpenDocument = std::move(callback);
    }

    void OnResize(int width, int height);

private:
    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool EnsureDirect2DResources();
    void DiscardDirect2DResources();
    void Render();
    void Relayout();
    void UpdateScrollbars();
    void ScrollTo(float newPos, bool notify = true);
    void UpdateHover(float xDip, float yDip);
    void SetHoverLink(const LinkHitBox* link);
    void OpenLink(const std::string& url);

    float PxToDip(float px) const noexcept { return px * 96.0f / static_cast<float>(m_dpi); }

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;

    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    PreviewRenderer m_renderer;
    bool m_effectsApplied = false;

    LayoutEngine m_layout;
    std::unique_ptr<Markdown::BlockTree> m_tree;
    std::filesystem::path m_baseDirectory;

    bool m_isDarkMode = false;
    PreviewThemeColors m_colors = PreviewThemeColors::Light();
    UINT m_dpi = 96;

    int m_clientWidthPx = 0;
    int m_clientHeightPx = 0;
    float m_layoutWidthDip = -1.0f;
    float m_scrollPos = 0.0f;   // DIPs
    float m_maxScroll = 0.0f;   // DIPs
    float m_viewHeight = 0.0f;  // DIPs
    int m_wheelRemainder = 0;

    const LinkHitBox* m_hoverLink = nullptr;
    std::string m_hoverUrl;
    std::string m_pressedUrl;
    bool m_trackingMouse = false;
    HCURSOR m_hHandCursor = nullptr;
    HCURSOR m_hArrowCursor = nullptr;
    std::function<void(int line)> m_onScrollCallback;
    std::function<void(const std::filesystem::path&)> m_onOpenDocument;
};

} // namespace Pluma::Preview
