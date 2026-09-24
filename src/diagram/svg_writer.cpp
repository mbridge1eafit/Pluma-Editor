#include "svg_writer.h"

#include <cmath>
#include <cstdio>

namespace Pluma::Diagram {

namespace {

constexpr const char* kRoleNames[kRoleCount] = {
    "none", "custom", "bg", "text", "muted", "line", "node-fill", "node-stroke", "cluster-fill", "cluster-stroke",
    "label-bg", "note-fill", "note-stroke", "actor-fill", "actor-stroke", "activation", "block-stroke",
    "block-label", "marker", "pie-text", "pie-0", "pie-1", "pie-2", "pie-3", "pie-4", "pie-5", "pie-6", "pie-7",
    "pie-8", "pie-9", "pie-10", "pie-11"};

std::string Hex(uint32_t rgb) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%06x", rgb & 0xFFFFFFu);
    return buffer;
}

std::string Num(float value) {
    if (!std::isfinite(value)) value = 0.0f;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(value));
    std::string s(buffer);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s == "-0" ? "0" : s;
}

std::string PaintCss(const Paint& paint, const Palette& fallback) {
    if (paint.role == Role::None) return "none";
    if (paint.role == Role::Custom) return Hex(paint.rgb);
    return std::string("var(--pluma-mm-") + kRoleNames[static_cast<int>(paint.role)] + "," +
           Hex(fallback.Resolve(paint)) + ")";
}

void AppendEscaped(std::string& out, std::string_view text) {
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default:  out += c; break;
        }
    }
}

std::string ShapeStyle(const Primitive& p, const Palette& fallback) {
    std::string style = "fill:" + PaintCss(p.fill, fallback);
    style += ";stroke:" + PaintCss(p.stroke, fallback);
    if (!p.stroke.IsNone()) {
        style += ";stroke-width:" + Num(p.strokeWidth);
        if (p.lineStyle == LineStyle::Dashed) style += ";stroke-dasharray:5 4";
        if (p.lineStyle == LineStyle::Dotted) style += ";stroke-dasharray:2 3";
    }
    return style;
}

} // namespace

std::string SvgPaletteVariables(const Palette& palette) {
    std::string css;
    for (int r = static_cast<int>(Role::Background); r < kRoleCount; ++r) {
        css += "--pluma-mm-";
        css += kRoleNames[r];
        css += ':';
        css += Hex(palette.colors[r]);
        css += ';';
    }
    return css;
}

std::string WriteSvg(const Scene& scene, std::string_view accessibleTitle) {
    const Palette fallback = Palette::Light();
    std::string out;
    out.reserve(scene.items.size() * 120 + 256);
    out += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + Num(scene.width) + " " + Num(scene.height) +
           "\" width=\"" + Num(scene.width) + "\" height=\"" + Num(scene.height) +
           "\" role=\"img\" font-family=\"Helvetica, Arial, sans-serif\" style=\"max-width:100%;height:auto\">";
    if (!accessibleTitle.empty()) {
        out += "<title>";
        AppendEscaped(out, accessibleTitle);
        out += "</title>";
    }

    for (const auto& p : scene.items) {
        switch (p.kind) {
        case PrimitiveKind::Rect:
            out += "<rect x=\"" + Num(p.x) + "\" y=\"" + Num(p.y) + "\" width=\"" + Num(p.w) + "\" height=\"" +
                   Num(p.h) + "\"";
            if (p.radius > 0.0f) out += " rx=\"" + Num(p.radius) + "\"";
            out += " style=\"" + ShapeStyle(p, fallback) + "\"/>";
            break;
        case PrimitiveKind::Ellipse:
            out += "<ellipse cx=\"" + Num(p.x) + "\" cy=\"" + Num(p.y) + "\" rx=\"" + Num(p.w) + "\" ry=\"" +
                   Num(p.h) + "\" style=\"" + ShapeStyle(p, fallback) + "\"/>";
            break;
        case PrimitiveKind::Path: {
            std::string d = "M" + Num(p.start.x) + " " + Num(p.start.y);
            Point prev = p.start;
            for (const auto& s : p.segments) {
                const bool straight = s.c1.x == prev.x && s.c1.y == prev.y && s.c2.x == s.end.x && s.c2.y == s.end.y;
                if (straight) {
                    d += "L" + Num(s.end.x) + " " + Num(s.end.y);
                } else {
                    d += "C" + Num(s.c1.x) + " " + Num(s.c1.y) + " " + Num(s.c2.x) + " " + Num(s.c2.y) + " " +
                         Num(s.end.x) + " " + Num(s.end.y);
                }
                prev = s.end;
            }
            if (p.closed) d += "Z";
            out += "<path d=\"" + d + "\" style=\"" + ShapeStyle(p, fallback) + ";stroke-linejoin:round\"/>";
            break;
        }
        case PrimitiveKind::Text: {
            const char* anchor = p.align == TextAlign::Left ? "start" : (p.align == TextAlign::Right ? "end" : "middle");
            out += "<text x=\"" + Num(p.x) + "\" y=\"" + Num(p.y) + "\" text-anchor=\"" + anchor +
                   "\" dominant-baseline=\"central\" font-size=\"" + Num(p.fontSize) + "\"";
            if (p.bold) out += " font-weight=\"bold\"";
            out += " style=\"fill:" + PaintCss(p.fill, fallback) + "\">";
            AppendEscaped(out, p.text);
            out += "</text>";
            break;
        }
        }
    }
    out += "</svg>";
    return out;
}

} // namespace Pluma::Diagram
