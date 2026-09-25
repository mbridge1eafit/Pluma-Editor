#include "icon_font.h"

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

} // namespace Pluma::Platform
