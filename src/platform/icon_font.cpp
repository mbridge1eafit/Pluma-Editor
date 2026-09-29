#include "icon_font.h"

#include <vector>

namespace Pluma::Platform {

namespace {

int CALLBACK FontExistsProc(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM found) {
    *reinterpret_cast<bool*>(found) = true;
    return 0;
}

const wchar_t* IconFontFace() {
    static const wchar_t* s_face = [] {
        LOGFONTW lf{};
        lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, L"Segoe Fluent Icons");
        bool found = false;
        HDC hdc = GetDC(nullptr);
        EnumFontFamiliesExW(hdc, &lf, FontExistsProc, reinterpret_cast<LPARAM>(&found), 0);
        ReleaseDC(nullptr, hdc);
        return found ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
    }();
    return s_face;
}

} // namespace

HFONT CreateIconFont(int pixelHeight) {
    return CreateFontW(-pixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                       IconFontFace());
}

void DrawGlyph(HDC hdc, const wchar_t* glyph, const RECT& rc, COLORREF color) {
    RECT r = rc;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, glyph, -1, &r, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP);
}

HBITMAP CreateGlyphStrip(std::span<const GlyphColor> glyphs, int size, HBITMAP* mask) {
    *mask = nullptr;
    const int count = static_cast<int>(glyphs.size());
    if (size <= 0 || count == 0) return nullptr;
    const int width = size * count;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -size; // Top-down rows
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP strip = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!strip) return nullptr;

    // White glyphs on black with grayscale antialiasing: each pixel's gray level is its coverage.
    // One font and one pass for all of them: creating the font is the costly part.
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ oldBitmap = SelectObject(dc, strip);
    HFONT font = CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, IconFontFace());
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    for (int i = 0; i < count; ++i) {
        RECT cell{i * size, 0, (i + 1) * size, size};
        DrawTextW(dc, glyphs[i].glyph, -1, &cell, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
    }
    GdiFlush();
    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBitmap);
    DeleteDC(dc);

    // Premultiplied colors, as AlphaBlend takes them. The mask marks the transparent pixels (1): an
    // image list uses it for an image without alpha values, such as a blank one. Monochrome rows
    // are padded to 16 bits.
    auto* pixels = static_cast<DWORD*>(bits);
    const int stride = (width + 15) / 16 * 2;
    std::vector<BYTE> maskBits(static_cast<size_t>(stride) * size, 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < width; ++x) {
            DWORD& pixel = pixels[y * width + x];
            const DWORD alpha = pixel & 0xFF;
            const COLORREF color = glyphs[x / size].color;
            pixel = (alpha << 24) | ((GetRValue(color) * alpha / 255) << 16) |
                    ((GetGValue(color) * alpha / 255) << 8) | (GetBValue(color) * alpha / 255);
            if (alpha == 0) maskBits[y * stride + x / 8] |= static_cast<BYTE>(0x80 >> (x % 8));
        }
    }
    *mask = CreateBitmap(width, size, 1, 1, maskBits.data());
    if (!*mask) {
        DeleteObject(strip);
        return nullptr;
    }
    return strip;
}

} // namespace Pluma::Platform
