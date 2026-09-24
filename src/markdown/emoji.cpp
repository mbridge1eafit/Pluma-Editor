#include "emoji.h"

#include <algorithm>
#include <array>
#include <utility>

namespace Pluma::Markdown {

namespace {

using Entry = std::pair<std::string_view, std::string_view>;

// Most used GitHub shortcodes. Must stay sorted by name (checked in debug builds).
constexpr std::array kEmojiTable = {
    Entry{"+1", "👍"},
    Entry{"-1", "👎"},
    Entry{"100", "💯"},
    Entry{"alarm_clock", "⏰"},
    Entry{"arrow_down", "⬇️"},
    Entry{"arrow_left", "⬅️"},
    Entry{"arrow_right", "➡️"},
    Entry{"arrow_up", "⬆️"},
    Entry{"art", "🎨"},
    Entry{"beer", "🍺"},
    Entry{"bell", "🔔"},
    Entry{"blue_heart", "💙"},
    Entry{"boat", "⛵"},
    Entry{"book", "📖"},
    Entry{"bookmark", "🔖"},
    Entry{"books", "📚"},
    Entry{"boom", "💥"},
    Entry{"brain", "🧠"},
    Entry{"broken_heart", "💔"},
    Entry{"bug", "🐛"},
    Entry{"bulb", "💡"},
    Entry{"calendar", "📆"},
    Entry{"camera", "📷"},
    Entry{"chart_with_upwards_trend", "📈"},
    Entry{"check", "✔️"},
    Entry{"clap", "👏"},
    Entry{"clipboard", "📋"},
    Entry{"clock3", "🕒"},
    Entry{"cloud", "☁️"},
    Entry{"coffee", "☕"},
    Entry{"computer", "💻"},
    Entry{"construction", "🚧"},
    Entry{"cool", "🆒"},
    Entry{"cry", "😢"},
    Entry{"dart", "🎯"},
    Entry{"disappointed", "😞"},
    Entry{"dizzy", "💫"},
    Entry{"dog", "🐶"},
    Entry{"email", "📧"},
    Entry{"exclamation", "❗"},
    Entry{"eyes", "👀"},
    Entry{"file_folder", "📁"},
    Entry{"fire", "🔥"},
    Entry{"flag_es", "🇪🇸"},
    Entry{"gear", "⚙️"},
    Entry{"gem", "💎"},
    Entry{"gift", "🎁"},
    Entry{"globe_with_meridians", "🌐"},
    Entry{"green_heart", "💚"},
    Entry{"grin", "😁"},
    Entry{"grinning", "😀"},
    Entry{"hammer", "🔨"},
    Entry{"hammer_and_wrench", "🛠️"},
    Entry{"hand", "✋"},
    Entry{"heart", "❤️"},
    Entry{"heart_eyes", "😍"},
    Entry{"heavy_check_mark", "✔️"},
    Entry{"heavy_minus_sign", "➖"},
    Entry{"heavy_plus_sign", "➕"},
    Entry{"hourglass", "⌛"},
    Entry{"house", "🏠"},
    Entry{"information_source", "ℹ️"},
    Entry{"joy", "😂"},
    Entry{"key", "🔑"},
    Entry{"laughing", "😆"},
    Entry{"link", "🔗"},
    Entry{"lock", "🔒"},
    Entry{"loudspeaker", "📢"},
    Entry{"mag", "🔍"},
    Entry{"memo", "📝"},
    Entry{"moneybag", "💰"},
    Entry{"muscle", "💪"},
    Entry{"no_entry", "⛔"},
    Entry{"no_entry_sign", "🚫"},
    Entry{"ok", "🆗"},
    Entry{"ok_hand", "👌"},
    Entry{"package", "📦"},
    Entry{"paperclip", "📎"},
    Entry{"partying_face", "🥳"},
    Entry{"pencil", "📝"},
    Entry{"pencil2", "✏️"},
    Entry{"point_down", "👇"},
    Entry{"point_left", "👈"},
    Entry{"point_right", "👉"},
    Entry{"point_up", "☝️"},
    Entry{"pray", "🙏"},
    Entry{"pushpin", "📌"},
    Entry{"question", "❓"},
    Entry{"rage", "😡"},
    Entry{"rainbow", "🌈"},
    Entry{"recycle", "♻️"},
    Entry{"red_circle", "🔴"},
    Entry{"robot", "🤖"},
    Entry{"rocket", "🚀"},
    Entry{"rotating_light", "🚨"},
    Entry{"scream", "😱"},
    Entry{"see_no_evil", "🙈"},
    Entry{"shield", "🛡️"},
    Entry{"shrug", "🤷"},
    Entry{"slightly_smiling_face", "🙂"},
    Entry{"smile", "😄"},
    Entry{"smiley", "😃"},
    Entry{"sob", "😭"},
    Entry{"sparkles", "✨"},
    Entry{"speech_balloon", "💬"},
    Entry{"star", "⭐"},
    Entry{"star2", "🌟"},
    Entry{"stop_sign", "🛑"},
    Entry{"sunglasses", "😎"},
    Entry{"sunny", "☀️"},
    Entry{"tada", "🎉"},
    Entry{"thinking", "🤔"},
    Entry{"thumbsdown", "👎"},
    Entry{"thumbsup", "👍"},
    Entry{"trophy", "🏆"},
    Entry{"truck", "🚚"},
    Entry{"unlock", "🔓"},
    Entry{"v", "✌️"},
    Entry{"warning", "⚠️"},
    Entry{"wave", "👋"},
    Entry{"white_check_mark", "✅"},
    Entry{"wink", "😉"},
    Entry{"wrench", "🔧"},
    Entry{"x", "❌"},
    Entry{"yellow_heart", "💛"},
    Entry{"zap", "⚡"},
};

constexpr bool IsSorted() {
    for (size_t i = 1; i < kEmojiTable.size(); ++i) {
        if (!(kEmojiTable[i - 1].first < kEmojiTable[i].first)) {
            return false;
        }
    }
    return true;
}

bool IsShortcodeChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '-';
}

} // namespace

std::string_view LookupEmojiShortcode(std::string_view name) {
    static_assert(IsSorted(), "kEmojiTable must be sorted by shortcode");
    auto it = std::lower_bound(kEmojiTable.begin(), kEmojiTable.end(), name,
                               [](const Entry& e, std::string_view key) { return e.first < key; });
    if (it != kEmojiTable.end() && it->first == name) {
        return it->second;
    }
    return {};
}

std::string ReplaceEmojiShortcodes(std::string_view text) {
    std::string out;
    size_t pos = 0;
    size_t copied = 0;
    while ((pos = text.find(':', pos)) != std::string_view::npos) {
        size_t end = pos + 1;
        while (end < text.size() && IsShortcodeChar(text[end])) {
            ++end;
        }
        if (end < text.size() && text[end] == ':' && end > pos + 1) {
            std::string_view emoji = LookupEmojiShortcode(text.substr(pos + 1, end - pos - 1));
            if (!emoji.empty()) {
                if (out.empty()) {
                    out.reserve(text.size());
                }
                out.append(text.substr(copied, pos - copied));
                out.append(emoji);
                copied = end + 1;
                pos = end + 1;
                continue;
            }
            // Closing colon may open the next shortcode (e.g. "a:b:smile:").
            pos = end;
            continue;
        }
        pos = end;
    }
    if (copied == 0) {
        return std::string(text);
    }
    out.append(text.substr(copied));
    return out;
}

} // namespace Pluma::Markdown
