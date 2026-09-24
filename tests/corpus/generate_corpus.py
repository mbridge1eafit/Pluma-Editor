import os

def create_medium(path):
    # ~100 KB Markdown document resembling a large README with tables and code
    section_template = """
## Section {i}: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration {i}

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {{
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, {i} * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}}
```

### Metrics Table {i}

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section {i}: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs
"""
    with open(path, "w", encoding="utf-8") as f:
        f.write("# Pluma Technical Specification & Benchmark Document (Medium)\n\n")
        # Target ~100 KB: each section is ~1 KB, so ~100 sections
        for i in range(1, 105):
            f.write(section_template.format(i=i))

def create_large(path, target_mb=10):
    chunk = (
        "## Generated Benchmark Block\n\n"
        "This paragraph is generated as part of the stress corpus for **Pluma**.\n"
        "We verify that loading, editing, and previewing high-volume Markdown documents remains stable.\n\n"
        "```python\ndef fib(n):\n    return n if n <= 1 else fib(n-1) + fib(n-2)\n```\n\n"
        "1. First list item\n2. Second list item\n3. Third list item with `inline code`\n\n"
    ).encode("utf-8")

    bytes_target = target_mb * 1024 * 1024
    written = 0
    with open(path, "wb") as f:
        f.write(b"# Pluma Large Document (10 MB)\n\n")
        written += 33
        while written < bytes_target:
            f.write(chunk)
            written += len(chunk)

def create_edge_cases(corpus_dir):
    # Empty file
    open(os.path.join(corpus_dir, "empty.md"), "w", encoding="utf-8").close()

    # Deeply nested list (20 levels)
    with open(os.path.join(corpus_dir, "deep_lists.md"), "w", encoding="utf-8") as f:
        f.write("# Deeply Nested Lists (20 Levels)\n\n")
        for lvl in range(1, 21):
            indent = "  " * (lvl - 1)
            f.write(f"{indent}- Level {lvl} list item\n")

    # Single long line (> 100,000 characters)
    with open(os.path.join(corpus_dir, "long_line.md"), "w", encoding="utf-8") as f:
        f.write("# Single Long Line Test\n\n")
        f.write("A" * 120000 + "\n")

    # Bidi & Emoji stress test
    with open(os.path.join(corpus_dir, "bidi_emoji.md"), "w", encoding="utf-8") as f:
        f.write("# Unicode, Emoji & BiDi Stress Test\n\n")
        f.write("🎉 🚀 ✨ 💻 📝 ⚡ 🔥 🌈 🦄 🎨 🏆\n\n")
        f.write("### Arabic (Right-to-Left):\nمرحبا بالعالم! هذا اختبار لمحرر النصوص السريع.\n\n")
        f.write("### Hebrew (Right-to-Left):\nשלום עולם! בדיקת ביצועים לעורך טקסט מהיר.\n\n")
        f.write("### Mixed Text & Math:\nComplex symbols: ∀x ∈ ℝ, ∃y : x + y = 0, ∑_{i=1}^n i = n(n+1)/2.\n")

    # Invalid UTF-8 byte sequences
    with open(os.path.join(corpus_dir, "invalid_utf8.md"), "wb") as f:
        f.write(b"# Invalid UTF-8 Stress Test\n\n")
        f.write(b"Valid prefix text. Invalid sequence follows: \xC0\xAF and \xED\xA0\x80 and \xFE\xFF.\n")

if __name__ == "__main__":
    base = os.path.dirname(os.path.abspath(__file__))
    create_medium(os.path.join(base, "medium.md"))
    create_large(os.path.join(base, "large.md"), 10)
    # Huge 50 MB can also be generated on demand
    create_edge_cases(base)
    print("Corpus generated successfully!")
