#include "html_exporter.h"
#include "../io/document_io.h"
#include <md4c-html.h>
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
  border-spacing: 0;
  border-collapse: collapse;
  width: 100%;
  margin-bottom: 16px;
}
table th, table td {
  padding: 8px 13px;
  border: 1px solid var(--table-border);
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
  pre, blockquote, table, tr {
    page-break-inside: avoid;
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

    unsigned parserFlags = MD_DIALECT_GITHUB | MD_FLAG_TASKLISTS;
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

    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n";
    doc << "<html lang=\"en\">\n";
    doc << "<head>\n";
    doc << "  <meta charset=\"utf-8\">\n";
    doc << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
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
