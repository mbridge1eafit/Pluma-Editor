# Pluma Technical Specification & Benchmark Document (Medium)


## Section 1: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 1

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 1 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 1

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 1: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 2: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 2

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 2 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 2

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 2: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 3: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 3

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 3 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 3

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 3: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 4: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 4

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 4 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 4

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 4: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 5: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 5

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 5 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 5

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 5: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 6: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 6

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 6 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 6

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 6: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 7: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 7

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 7 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 7

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 7: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 8: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 8

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 8 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 8

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 8: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 9: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 9

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 9 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 9

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 9: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 10: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 10

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 10 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 10

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 10: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 11: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 11

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 11 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 11

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 11: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 12: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 12

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 12 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 12

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 12: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 13: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 13

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 13 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 13

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 13: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 14: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 14

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 14 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 14

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 14: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 15: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 15

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 15 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 15

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 15: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 16: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 16

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 16 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 16

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 16: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 17: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 17

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 17 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 17

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 17: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 18: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 18

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 18 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 18

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 18: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 19: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 19

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 19 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 19

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 19: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 20: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 20

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 20 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 20

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 20: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 21: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 21

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 21 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 21

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 21: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 22: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 22

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 22 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 22

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 22: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 23: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 23

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 23 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 23

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 23: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 24: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 24

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 24 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 24

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 24: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 25: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 25

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 25 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 25

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 25: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 26: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 26

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 26 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 26

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 26: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 27: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 27

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 27 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 27

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 27: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 28: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 28

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 28 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 28

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 28: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 29: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 29

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 29 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 29

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 29: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 30: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 30

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 30 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 30

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 30: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 31: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 31

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 31 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 31

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 31: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 32: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 32

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 32 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 32

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 32: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 33: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 33

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 33 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 33

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 33: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 34: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 34

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 34 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 34

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 34: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 35: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 35

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 35 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 35

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 35: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 36: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 36

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 36 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 36

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 36: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 37: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 37

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 37 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 37

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 37: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 38: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 38

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 38 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 38

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 38: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 39: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 39

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 39 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 39

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 39: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 40: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 40

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 40 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 40

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 40: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 41: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 41

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 41 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 41

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 41: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 42: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 42

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 42 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 42

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 42: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 43: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 43

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 43 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 43

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 43: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 44: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 44

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 44 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 44

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 44: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 45: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 45

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 45 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 45

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 45: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 46: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 46

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 46 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 46

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 46: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 47: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 47

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 47 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 47

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 47: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 48: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 48

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 48 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 48

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 48: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 49: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 49

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 49 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 49

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 49: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 50: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 50

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 50 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 50

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 50: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 51: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 51

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 51 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 51

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 51: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 52: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 52

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 52 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 52

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 52: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 53: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 53

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 53 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 53

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 53: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 54: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 54

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 54 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 54

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 54: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 55: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 55

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 55 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 55

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 55: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 56: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 56

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 56 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 56

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 56: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 57: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 57

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 57 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 57

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 57: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 58: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 58

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 58 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 58

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 58: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 59: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 59

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 59 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 59

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 59: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 60: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 60

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 60 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 60

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 60: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 61: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 61

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 61 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 61

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 61: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 62: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 62

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 62 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 62

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 62: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 63: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 63

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 63 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 63

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 63: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 64: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 64

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 64 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 64

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 64: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 65: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 65

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 65 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 65

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 65: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 66: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 66

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 66 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 66

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 66: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 67: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 67

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 67 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 67

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 67: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 68: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 68

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 68 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 68

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 68: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 69: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 69

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 69 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 69

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 69: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 70: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 70

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 70 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 70

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 70: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 71: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 71

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 71 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 71

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 71: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 72: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 72

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 72 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 72

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 72: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 73: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 73

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 73 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 73

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 73: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 74: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 74

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 74 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 74

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 74: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 75: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 75

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 75 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 75

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 75: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 76: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 76

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 76 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 76

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 76: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 77: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 77

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 77 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 77

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 77: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 78: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 78

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 78 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 78

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 78: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 79: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 79

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 79 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 79

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 79: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 80: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 80

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 80 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 80

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 80: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 81: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 81

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 81 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 81

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 81: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 82: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 82

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 82 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 82

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 82: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 83: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 83

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 83 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 83

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 83: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 84: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 84

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 84 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 84

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 84: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 85: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 85

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 85 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 85

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 85: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 86: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 86

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 86 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 86

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 86: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 87: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 87

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 87 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 87

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 87: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 88: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 88

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 88 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 88

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 88: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 89: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 89

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 89 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 89

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 89: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 90: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 90

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 90 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 90

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 90: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 91: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 91

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 91 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 91

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 91: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 92: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 92

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 92 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 92

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 92: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 93: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 93

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 93 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 93

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 93: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 94: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 94

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 94 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 94

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 94: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 95: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 95

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 95 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 95

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 95: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 96: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 96

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 96 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 96

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 96: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 97: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 97

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 97 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 97

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 97: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 98: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 98

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 98 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 98

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 98: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 99: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 99

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 99 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 99

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 99: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 100: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 100

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 100 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 100

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 100: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 101: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 101

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 101 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 101

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 101: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 102: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 102

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 102 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 102

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 102: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 103: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 103

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 103 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 103

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 103: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs

## Section 104: Architecture and Implementation Details

Pluma is a **native Windows Markdown editor** built from the ground up using Win32, Direct2D, and DirectWrite.
It features *instantaneous startup*, smooth scrolling, and synchronization.

### Code Demonstration 104

```cpp
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

void RenderTextLayout(ID2D1RenderTarget* pRenderTarget, IDWriteTextLayout* pLayout) {
    D2D1_POINT_2F origin = D2D1::Point2F(10.0f, 104 * 20.0f);
    pRenderTarget->DrawTextLayout(origin, pLayout, nullptr);
}
```

### Metrics Table 104

| Metric | Target | Hard Limit | Current Value |
| :--- | :---: | :---: | ---: |
| Startup Latency | <= 60 ms | <= 100 ms | 42 ms |
| Memory Footprint | <= 15 MB | <= 30 MB | 12 MB |
| Binary Size | <= 2.0 MB | <= 4.0 MB | 1.8 MB |
| Keystroke Latency | <= 8 ms | <= 16 ms | 4 ms |

> Blockquote note for section 104: Always optimize for zero memory allocation on critical paths.
> Nested quote: "Fast software feels like magic."

- Task 1: Initialize Direct2D factory
- Task 2: Create hardware render target
- Task 3: Tokenize CommonMark AST with md4c
- Task 4: Layout block paragraphs
