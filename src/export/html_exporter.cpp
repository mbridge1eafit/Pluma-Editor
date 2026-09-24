#include "html_exporter.h"
#include "../io/document_io.h"
#include "../markdown/emoji.h"
#include "../markdown/slug.h"
#include <md4c-html.h>
#include <map>
#include <sstream>

namespace Pluma::Export {

namespace {

void HtmlOutputCallback(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(text, size);
}

std::string EscapeHtml(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&':  result += "&amp;"; break;
        case '<':  result += "&lt;"; break;
        case '>':  result += "&gt;"; break;
        case '"':  result += "&quot;"; break;
        case '\'': result += "&#39;"; break;
        default:   result += c; break;
        }
    }
    return result;
}

// Plain text of an HTML fragment: tags removed, the basic entities md4c emits decoded.
std::string HtmlToPlainText(std::string_view html) {
    std::string text;
    bool inTag = false;
    for (size_t i = 0; i < html.size(); ++i) {
        const char c = html[i];
        if (inTag) {
            inTag = (c != '>');
            continue;
        }
        if (c == '<') {
            inTag = true;
            continue;
        }
        if (c == '&') {
            static constexpr std::pair<std::string_view, char> kEntities[] = {
                {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&#39;", '\''}};
            bool decoded = false;
            for (const auto& [entity, ch] : kEntities) {
                if (html.compare(i, entity.size(), entity) == 0) {
                    text.push_back(ch);
                    i += entity.size() - 1;
                    decoded = true;
                    break;
                }
            }
            if (decoded) continue;
        }
        text.push_back(c);
    }
    return text;
}

// Adds GitHub-compatible id="" anchors to headings (so "#section" links work) and
// expands :emoji: shortcodes outside <pre>/<code>, matching the in-app preview.
std::string PostProcessBody(const std::string& body) {
    std::string out;
    out.reserve(body.size() + body.size() / 16);
    std::map<std::string, int> slugCounts;
    int codeDepth = 0;
    size_t copied = 0; // body[0, copied) is already in `out`; untouched spans are copied in bulk

    auto flushTo = [&](size_t pos) {
        out.append(body, copied, pos - copied);
        copied = pos;
    };
    auto processText = [&](size_t begin, size_t end) {
        if (codeDepth > 0 || begin >= end) return;
        const std::string_view text(body.data() + begin, end - begin);
        if (text.find(':') == std::string_view::npos) return;
        std::string replaced = Markdown::ReplaceEmojiShortcodes(text);
        if (replaced.size() == text.size() && replaced == text) return;
        flushTo(begin);
        out += replaced;
        copied = end;
    };

    size_t i = 0;
    while (i < body.size()) {
        const size_t lt = body.find('<', i);
        if (lt == std::string::npos) {
            processText(i, body.size());
            break;
        }
        processText(i, lt);

        const size_t tagEnd = body.find('>', lt);
        if (tagEnd == std::string::npos) {
            break;
        }
        const std::string_view tag(body.data() + lt, tagEnd - lt + 1);
        i = tagEnd + 1;

        if (tag.starts_with("<pre") || tag.starts_with("<code")) {
            ++codeDepth;
            continue;
        }
        if (tag.starts_with("</pre") || tag.starts_with("</code")) {
            if (codeDepth > 0) --codeDepth;
            continue;
        }

        const bool isHeadingOpen = tag.size() == 4 && tag[1] == 'h' && tag[2] >= '1' && tag[2] <= '6';
        if (!isHeadingOpen) {
            continue;
        }
        const char closeTag[] = {'<', '/', 'h', tag[2], '>', '\0'};
        const size_t close = body.find(closeTag, tagEnd);
        if (close == std::string::npos) {
            continue;
        }
        std::string slug = Markdown::Slugify(Markdown::ReplaceEmojiShortcodes(
            HtmlToPlainText(std::string_view(body).substr(tagEnd + 1, close - tagEnd - 1))));
        if (slug.empty()) {
            continue;
        }
        const int seen = slugCounts[slug]++;
        if (seen > 0) {
            slug += "-" + std::to_string(seen);
        }
        flushTo(lt);
        out += "<h";
        out += tag[2];
        out += " id=\"";
        out += slug;
        out += "\">";
        copied = tagEnd + 1;
    }
    flushTo(body.size());
    return out;
}

} // namespace

std::string HtmlExporter::GetEmbeddedCss(HtmlTheme theme) {
    std::ostringstream css;

    // Base variables & color schemes
    if (theme == HtmlTheme::Light) {
        css << R"CSS(
:root {
  --bg: #ffffff;
  --fg: #1e1e1e;
  --fg-muted: #57606a;
  --heading: #0f172a;
  --link: #0969da;
  --code-bg: #f6f8fa;
  --code-border: #d0d7de;
  --quote-border: #d0d7de;
  --rule: #d0d7de;
  --table-border: #d0d7de;
  --table-header-bg: #f6f8fa;
  --table-alt-bg: #fbfcfd;
}
)CSS";
    } else if (theme == HtmlTheme::Dark) {
        css << R"CSS(
:root {
  --bg: #1e1e1e;
  --fg: #e0e0e0;
  --fg-muted: #8b949e;
  --heading: #ffffff;
  --link: #58a6ff;
  --code-bg: #2d2d2d;
  --code-border: #3c3c3c;
  --quote-border: #4a5568;
  --rule: #3c3c3c;
  --table-border: #3c3c3c;
  --table-header-bg: #2d2d2d;
  --table-alt-bg: #242424;
}
)CSS";
    } else { // Auto / Media query
        css << R"CSS(
:root {
  --bg: #ffffff;
  --fg: #1e1e1e;
  --fg-muted: #57606a;
  --heading: #0f172a;
  --link: #0969da;
  --code-bg: #f6f8fa;
  --code-border: #d0d7de;
  --quote-border: #d0d7de;
  --rule: #d0d7de;
  --table-border: #d0d7de;
  --table-header-bg: #f6f8fa;
  --table-alt-bg: #fbfcfd;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #1e1e1e;
    --fg: #e0e0e0;
    --fg-muted: #8b949e;
    --heading: #ffffff;
    --link: #58a6ff;
    --code-bg: #2d2d2d;
    --code-border: #3c3c3c;
    --quote-border: #4a5568;
    --rule: #3c3c3c;
    --table-border: #3c3c3c;
    --table-header-bg: #2d2d2d;
    --table-alt-bg: #242424;
  }
}
)CSS";
    }

    // Core styles
    css << R"CSS(
* {
  box-sizing: border-box;
}
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji";
  line-height: 1.6;
  color: var(--fg);
  background-color: var(--bg);
  margin: 0;
  padding: 0;
}
.container {
  max-width: 860px;
  margin: 0 auto;
  padding: 40px 24px;
}
h1, h2, h3, h4, h5, h6 {
  color: var(--heading);
  line-height: 1.25;
  margin-top: 24px;
  margin-bottom: 16px;
  font-weight: 600;
}
h1 { font-size: 2em; padding-bottom: 0.3em; border-bottom: 1px solid var(--rule); }
h2 { font-size: 1.5em; padding-bottom: 0.3em; border-bottom: 1px solid var(--rule); }
h3 { font-size: 1.25em; }
h4 { font-size: 1em; }
h5 { font-size: 0.875em; }
h6 { font-size: 0.85em; color: var(--fg-muted); }
h1:target, h2:target, h3:target, h4:target, h5:target, h6:target {
  scroll-margin-top: 16px;
}
p, li, td, th {
  overflow-wrap: break-word;
}
p, ul, ol {
  margin-top: 0;
  margin-bottom: 16px;
}
ul, ol {
  padding-left: 2em;
}
li + li {
  margin-top: 0.25em;
}
ul ul, ul ol, ol ol, ol ul {
  margin-top: 0.25em;
  margin-bottom: 0;
}
li.task-list-item {
  list-style-type: none;
}
li.task-list-item > input[type="checkbox"] {
  margin: 0 0.4em 0.2em -1.5em;
  vertical-align: middle;
}
img {
  max-width: 100%;
  height: auto;
}
kbd {
  font-family: Consolas, "Cascadia Code", monospace;
  font-size: 85%;
  padding: 0.15em 0.4em;
  border: 1px solid var(--code-border);
  border-bottom-width: 2px;
  border-radius: 4px;
  background-color: var(--code-bg);
}
blockquote {
  margin: 0 0 16px;
  padding: 0 1em;
  color: var(--fg-muted);
  border-left: 0.25em solid var(--quote-border);
}
pre {
  padding: 16px;
  overflow: auto;
  font-size: 85%;
  line-height: 1.45;
  background-color: var(--code-bg);
  border: 1px solid var(--code-border);
  border-radius: 6px;
}
code {
  font-family: Consolas, "Cascadia Code", "Courier New", monospace;
  font-size: 85%;
  padding: 0.2em 0.4em;
  margin: 0;
  background-color: var(--code-bg);
  border-radius: 4px;
}
pre code {
  padding: 0;
  background-color: transparent;
  border-radius: 0;
}
table {
  display: block;
  width: max-content;
  max-width: 100%;
  overflow-x: auto;
  border-spacing: 0;
  border-collapse: collapse;
  margin-bottom: 16px;
}
table th, table td {
  padding: 8px 13px;
  border: 1px solid var(--table-border);
  overflow-wrap: anywhere;
  min-width: 3em;
}
table th {
  background-color: var(--table-header-bg);
  font-weight: 600;
}
table tr:nth-child(2n) {
  background-color: var(--table-alt-bg);
}
hr {
  height: 0.25em;
  padding: 0;
  margin: 24px 0;
  background-color: var(--rule);
  border: 0;
}
a {
  color: var(--link);
  text-decoration: none;
}
a:hover {
  text-decoration: underline;
}
input[type="checkbox"] {
  margin-right: 0.5em;
}
@media print {
  body {
    background: white !important;
    color: black !important;
  }
  .container {
    max-width: 100%;
    padding: 0;
    margin: 0;
  }
  pre, blockquote, table, tr, img {
    page-break-inside: avoid;
  }
  table {
    display: table;
    width: 100%;
  }
  pre {
    white-space: pre-wrap;
    overflow-wrap: anywhere;
  }
  h1, h2, h3, h4, h5, h6 {
    page-break-after: avoid;
  }
}
)CSS";

    return css.str();
}

std::string HtmlExporter::ExportToString(std::string_view markdown, const HtmlExportOptions& options) {
    std::string htmlBody;
    htmlBody.reserve(markdown.size() * 2);

    // MD_DIALECT_GITHUB already includes tables, task lists, strikethrough and autolinks.
    unsigned parserFlags = MD_DIALECT_GITHUB;
    unsigned rendererFlags = 0;

    int res = md_html(
        markdown.data(),
        static_cast<MD_SIZE>(markdown.size()),
        HtmlOutputCallback,
        &htmlBody,
        parserFlags,
        rendererFlags
    );

    if (res != 0) {
        return {};
    }
    htmlBody = PostProcessBody(htmlBody);

    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n";
    doc << "<html lang=\"" << EscapeHtml(options.language) << "\">\n";
    doc << "<head>\n";
    doc << "  <meta charset=\"utf-8\">\n";
    doc << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    doc << "  <meta name=\"generator\" content=\"Pluma\">\n";
    if (options.theme == HtmlTheme::Auto) {
        doc << "  <meta name=\"color-scheme\" content=\"light dark\">\n";
    }
    doc << "  <title>" << EscapeHtml(options.title) << "</title>\n";

    if (options.embedStyles) {
        doc << "  <style>\n";
        doc << GetEmbeddedCss(options.theme);
        doc << "  </style>\n";
    }

    doc << "</head>\n";
    doc << "<body>\n";
    doc << "  <div class=\"container\">\n";
    doc << htmlBody;
    doc << "  </div>\n";
    doc << "</body>\n";
    doc << "</html>\n";

    return doc.str();
}

bool HtmlExporter::ExportToFile(const std::filesystem::path& path, std::string_view markdown, const HtmlExportOptions& options) {
    std::string html = ExportToString(markdown, options);
    if (html.empty()) {
        return false;
    }

    try {
        IO::WriteDocumentAtomic(path, html, IO::Encoding::Utf8, IO::LineEnding::CRLF);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Pluma::Export
