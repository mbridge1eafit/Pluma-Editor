#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Pluma::Platform {

// Glyphs shared by Segoe Fluent Icons (Windows 11) and Segoe MDL2 Assets (Windows 10).
namespace Glyph {
constexpr wchar_t kNew[] = L"";
constexpr wchar_t kOpen[] = L"";
constexpr wchar_t kSave[] = L"";
constexpr wchar_t kUndo[] = L"";
constexpr wchar_t kRedo[] = L"";
constexpr wchar_t kCut[] = L"";
constexpr wchar_t kCopy[] = L"";
constexpr wchar_t kPaste[] = L"";
constexpr wchar_t kFind[] = L"";
constexpr wchar_t kSettings[] = L"";
constexpr wchar_t kFolder[] = L"";
constexpr wchar_t kList[] = L"";
constexpr wchar_t kRefresh[] = L"";
constexpr wchar_t kClose[] = L"";
constexpr wchar_t kChevronDown[] = L"";
constexpr wchar_t kMore[] = L"";
// Format bar
constexpr wchar_t kBold[] = L"";
constexpr wchar_t kItalic[] = L"";
constexpr wchar_t kStrikethrough[] = L"";
constexpr wchar_t kCode[] = L"";
constexpr wchar_t kBulletList[] = L"";
constexpr wchar_t kTaskList[] = L"";
constexpr wchar_t kQuote[] = L"";
constexpr wchar_t kLink[] = L"";
constexpr wchar_t kImage[] = L"";
constexpr wchar_t kTable[] = L"";
constexpr wchar_t kCodeBlock[] = L"";
constexpr wchar_t kHorizontalRule[] = L"";
constexpr wchar_t kDiagram[] = L"";
} // namespace Glyph

// Creates the system icon font at `pixelHeight` (Segoe Fluent Icons, falling back to Segoe MDL2 Assets).
HFONT CreateIconFont(int pixelHeight);

// Draws `glyph` centered in `rc` with the currently selected icon font.
void DrawGlyph(HDC hdc, const wchar_t* glyph, const RECT& rc, COLORREF color);

} // namespace Pluma::Platform
