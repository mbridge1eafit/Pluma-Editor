#include "preview_renderer.h"

#include <algorithm>

namespace Pluma::Preview {

namespace {

// Colour emoji (Segoe UI Emoji) instead of monochrome outlines (Windows 8.1+).
constexpr D2D1_DRAW_TEXT_OPTIONS kTextOptions = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT;

} // namespace

bool PreviewRenderer::CreateResources(ID2D1RenderTarget* rt, const PreviewThemeColors& colors) {
    DiscardResources();
    if (!rt) return false;

    struct BrushSpec {
        const D2D1_COLOR_F& color;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>& brush;
    };
    const BrushSpec specs[] = {
        {colors.text, m_brushText},
        {colors.mutedText, m_brushMuted},
        {colors.headingText, m_brushHeading},
        {colors.linkText, m_brushLink},
        {colors.codeText, m_brushCodeText},
        {colors.codeBackground, m_brushCodeBg},
        {colors.codeBorder, m_brushCodeBorder},
        {colors.inlineCodeBg, m_brushInlineCodeBg},
        {colors.inlineCodeText, m_brushInlineCodeText},
        {colors.blockquoteBorder, m_brushQuoteBar},
        {colors.blockquoteText, m_brushQuoteText},
        {colors.ruleLine, m_brushRule},
        {colors.tableBorder, m_brushTableBorder},
        {colors.tableHeaderBg, m_brushTableHeaderBg},
        {colors.tableStripeBg, m_brushTableStripe},
        {colors.accent, m_brushAccent},
        {colors.accentText, m_brushAccentText},
    };
    for (const auto& spec : specs) {
        if (FAILED(rt->CreateSolidColorBrush(spec.color, &spec.brush))) {
            DiscardResources();
            return false;
        }
    }
    return true;
}

void PreviewRenderer::DiscardResources() {
    m_brushText.Reset();
    m_brushMuted.Reset();
    m_brushHeading.Reset();
    m_brushLink.Reset();
    m_brushCodeText.Reset();
    m_brushCodeBg.Reset();
    m_brushCodeBorder.Reset();
    m_brushInlineCodeBg.Reset();
    m_brushInlineCodeText.Reset();
    m_brushQuoteBar.Reset();
    m_brushQuoteText.Reset();
    m_brushRule.Reset();
    m_brushTableBorder.Reset();
    m_brushTableHeaderBg.Reset();
    m_brushTableStripe.Reset();
    m_brushAccent.Reset();
    m_brushAccentText.Reset();
}

void PreviewRenderer::ApplyTextEffects(const LayoutEngine& layout) const {
    if (!HasResources()) return;

    auto apply = [this](IDWriteTextLayout* textLayout, const std::vector<EffectRange>& effects) {
        if (!textLayout) return;
        for (const auto& e : effects) {
            ID2D1Brush* brush = (e.effect == TextEffect::Link) ? static_cast<ID2D1Brush*>(m_brushLink.Get())
                                                               : static_cast<ID2D1Brush*>(m_brushInlineCodeText.Get());
            textLayout->SetDrawingEffect(brush, DWRITE_TEXT_RANGE{e.start, e.length});
        }
    };

    for (const auto& block : layout.GetBlocks()) {
        apply(block.textLayout.Get(), block.effects);
        for (const auto& row : block.tableRows) {
            for (const auto& cell : row) {
                apply(cell.textLayout.Get(), cell.effects);
            }
        }
    }
}

ID2D1Brush* PreviewRenderer::TextBrushFor(const LayoutBlock& block) const {
    if (block.type == Markdown::BlockType::Heading) {
        return block.level == 6 ? m_brushMuted.Get() : m_brushHeading.Get();
    }
    return block.quoteDepth > 0 ? m_brushQuoteText.Get() : m_brushText.Get();
}

void PreviewRenderer::DrawTextBlock(ID2D1RenderTarget* rt, const LayoutBlock& block) const {
    for (const auto& pill : block.inlineCodePills) {
        rt->FillRoundedRectangle(D2D1::RoundedRect(pill, 4.0f, 4.0f), m_brushInlineCodeBg.Get());
    }
    if (block.textLayout) {
        rt->DrawTextLayout(block.textOrigin, block.textLayout.Get(), TextBrushFor(block), kTextOptions);
    }
}

void PreviewRenderer::DrawListMarker(ID2D1RenderTarget* rt, const LayoutBlock& block) const {
    const D2D1_RECT_F& m = block.markerRect;
    if (block.isTask) {
        const D2D1_ROUNDED_RECT box = D2D1::RoundedRect(m, 3.5f, 3.5f);
        if (block.isTaskChecked) {
            rt->FillRoundedRectangle(box, m_brushAccent.Get());
            const float w = m.right - m.left;
            const float h = m.bottom - m.top;
            const D2D1_POINT_2F p1 = D2D1::Point2F(m.left + w * 0.24f, m.top + h * 0.52f);
            const D2D1_POINT_2F p2 = D2D1::Point2F(m.left + w * 0.43f, m.top + h * 0.72f);
            const D2D1_POINT_2F p3 = D2D1::Point2F(m.left + w * 0.77f, m.top + h * 0.30f);
            rt->DrawLine(p1, p2, m_brushAccentText.Get(), 1.9f);
            rt->DrawLine(p2, p3, m_brushAccentText.Get(), 1.9f);
        } else {
            const D2D1_ROUNDED_RECT inset = D2D1::RoundedRect(
                D2D1::RectF(m.left + 0.5f, m.top + 0.5f, m.right - 0.5f, m.bottom - 0.5f), 3.0f, 3.0f);
            rt->DrawRoundedRectangle(inset, m_brushMuted.Get(), 1.3f);
        }
        return;
    }

    if (block.isOrdered) {
        if (block.markerLayout) {
            rt->DrawTextLayout(block.markerOrigin, block.markerLayout.Get(), m_brushMuted.Get(), kTextOptions);
        }
        return;
    }

    // Bullet style alternates with nesting depth: disc, circle, square.
    ID2D1Brush* brush = block.quoteDepth > 0 ? m_brushQuoteText.Get() : m_brushText.Get();
    const float cx = (m.left + m.right) * 0.5f;
    const float cy = (m.top + m.bottom) * 0.5f;
    const float r = (m.right - m.left) * 0.5f;
    switch (block.listDepth % 3) {
    case 0:
        rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), brush);
        break;
    case 1:
        rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r - 0.5f, r - 0.5f), brush, 1.2f);
        break;
    default:
        rt->FillRectangle(D2D1::RectF(cx - r + 0.4f, cy - r + 0.4f, cx + r - 0.4f, cy + r - 0.4f), brush);
        break;
    }
}

void PreviewRenderer::DrawCodeBlock(ID2D1RenderTarget* rt, const LayoutBlock& block) const {
    const D2D1_RECT_F& b = block.bounds;
    const D2D1_ROUNDED_RECT box = D2D1::RoundedRect(
        D2D1::RectF(b.left + 0.5f, b.top + 0.5f, b.right - 0.5f, b.bottom - 0.5f), 6.0f, 6.0f);
    rt->FillRoundedRectangle(box, m_brushCodeBg.Get());
    rt->DrawRoundedRectangle(box, m_brushCodeBorder.Get(), 1.0f);

    // Code never paints outside its box, even for pathological lines.
    rt->PushAxisAlignedClip(b, D2D1_ANTIALIAS_MODE_ALIASED);
    if (block.labelLayout) {
        rt->DrawTextLayout(block.labelOrigin, block.labelLayout.Get(), m_brushMuted.Get(), kTextOptions);
    }
    if (block.textLayout) {
        rt->DrawTextLayout(block.textOrigin, block.textLayout.Get(), m_brushCodeText.Get(), kTextOptions);
    }
    rt->PopAxisAlignedClip();
}

void PreviewRenderer::DrawTable(ID2D1RenderTarget* rt, const LayoutBlock& block) const {
    // Backgrounds: header and zebra stripes.
    for (const auto& row : block.tableRows) {
        for (const auto& cell : row) {
            if (cell.isHeader) {
                rt->FillRectangle(cell.rect, m_brushTableHeaderBg.Get());
            } else if (cell.rowIndex % 2 == 1) {
                rt->FillRectangle(cell.rect, m_brushTableStripe.Get());
            }
        }
    }

    for (const auto& pill : block.inlineCodePills) {
        rt->FillRoundedRectangle(D2D1::RoundedRect(pill, 4.0f, 4.0f), m_brushInlineCodeBg.Get());
    }

    // Cell text, clipped to its cell so nothing can spill into neighbours.
    ID2D1Brush* textBrush = block.quoteDepth > 0 ? m_brushQuoteText.Get() : m_brushText.Get();
    for (const auto& row : block.tableRows) {
        for (const auto& cell : row) {
            if (!cell.textLayout) continue;
            rt->PushAxisAlignedClip(cell.rect, D2D1_ANTIALIAS_MODE_ALIASED);
            rt->DrawTextLayout(cell.textOrigin, cell.textLayout.Get(), textBrush, kTextOptions);
            rt->PopAxisAlignedClip();
        }
    }

    // Crisp 1px grid: aliased lines on pixel centres, shared edges drawn once per cell.
    const D2D1_ANTIALIAS_MODE previous = rt->GetAntialiasMode();
    rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    for (const auto& row : block.tableRows) {
        for (const auto& cell : row) {
            const D2D1_RECT_F r = D2D1::RectF(cell.rect.left + 0.5f, cell.rect.top + 0.5f,
                                              cell.rect.right - 0.5f, cell.rect.bottom + 0.5f);
            rt->DrawRectangle(r, m_brushTableBorder.Get(), 1.0f);
        }
    }
    rt->SetAntialiasMode(previous);
}

void PreviewRenderer::Draw(ID2D1RenderTarget* rt, const LayoutEngine& layout, float scrollY, float viewHeight) const {
    if (!rt || !HasResources()) return;

    rt->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, -scrollY));
    const float viewTop = scrollY;
    const float viewBottom = scrollY + viewHeight;

    for (const auto& block : layout.GetBlocks()) {
        // Viewport culling
        if (block.bounds.bottom < viewTop || block.bounds.top > viewBottom) {
            continue;
        }

        switch (block.type) {
        case Markdown::BlockType::Heading:
            DrawTextBlock(rt, block);
            if (block.level <= 2) {
                const float y = block.bounds.bottom - 1.0f;
                rt->FillRectangle(D2D1::RectF(block.bounds.left, y - 1.0f, block.bounds.right, y), m_brushRule.Get());
            }
            break;

        case Markdown::BlockType::Paragraph:
            DrawTextBlock(rt, block);
            break;

        case Markdown::BlockType::Blockquote:
            if (block.isQuoteBar) {
                rt->FillRoundedRectangle(D2D1::RoundedRect(block.bounds, 1.75f, 1.75f), m_brushQuoteBar.Get());
            } else {
                DrawTextBlock(rt, block);
            }
            break;

        case Markdown::BlockType::ListItem:
            DrawListMarker(rt, block);
            DrawTextBlock(rt, block);
            break;

        case Markdown::BlockType::CodeBlock:
            DrawCodeBlock(rt, block);
            break;

        case Markdown::BlockType::ThematicBreak:
            rt->FillRoundedRectangle(D2D1::RoundedRect(block.bounds, 1.0f, 1.0f), m_brushRule.Get());
            break;

        case Markdown::BlockType::Table:
            DrawTable(rt, block);
            break;

        default:
            break;
        }
    }

    rt->SetTransform(D2D1::Matrix3x2F::Identity());
}

} // namespace Pluma::Preview
