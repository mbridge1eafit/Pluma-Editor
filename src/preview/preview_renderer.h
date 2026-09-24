#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>

#include "preview_layout.h"

namespace Pluma::Preview {

// Draws a computed LayoutEngine onto any Direct2D render target (window or offscreen bitmap).
// All coordinates are DIPs; the render target DPI performs the pixel scaling.
class PreviewRenderer {
public:
    // (Re)creates the brushes for the given render target and theme.
    bool CreateResources(ID2D1RenderTarget* renderTarget, const PreviewThemeColors& colors);
    void DiscardResources();
    bool HasResources() const noexcept { return m_brushText != nullptr; }

    // Attaches the coloured drawing effects (links, inline code) to every text layout.
    // Must be called again after the layout is recomputed or the brushes are recreated.
    void ApplyTextEffects(const LayoutEngine& layout) const;

    void Draw(ID2D1RenderTarget* renderTarget, const LayoutEngine& layout, float scrollY, float viewHeight) const;

private:
    void DrawTextBlock(ID2D1RenderTarget* rt, const LayoutBlock& block) const;
    void DrawListMarker(ID2D1RenderTarget* rt, const LayoutBlock& block) const;
    void DrawCodeBlock(ID2D1RenderTarget* rt, const LayoutBlock& block) const;
    void DrawTable(ID2D1RenderTarget* rt, const LayoutBlock& block) const;
    ID2D1Brush* TextBrushFor(const LayoutBlock& block) const;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushText;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushMuted;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushHeading;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushLink;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushCodeText;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushCodeBg;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushCodeBorder;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushInlineCodeBg;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushInlineCodeText;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushQuoteBar;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushQuoteText;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushRule;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushTableBorder;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushTableHeaderBg;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushTableStripe;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushAccent;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brushAccentText;
};

} // namespace Pluma::Preview
