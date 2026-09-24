#include <gtest/gtest.h>

#include <cmath>

#include "diagram/mermaid.h"
#include "diagram/svg_writer.h"

using namespace Pluma::Diagram;

namespace {

MeasureText Measure() {
    return [](std::string_view text, float size, bool bold) { return EstimateTextWidth(text, size, bold); };
}

MermaidResult Render(std::string_view source) {
    return RenderMermaid(source, Measure());
}

const Primitive* FindText(const Scene& scene, std::string_view text) {
    for (const auto& item : scene.items) {
        if (item.kind == PrimitiveKind::Text && item.text == text) return &item;
    }
    return nullptr;
}

bool AllFinite(const Scene& scene) {
    auto ok = [](Point p) { return std::isfinite(p.x) && std::isfinite(p.y); };
    for (const auto& item : scene.items) {
        if (!std::isfinite(item.x) || !std::isfinite(item.y) || !std::isfinite(item.w) || !std::isfinite(item.h)) {
            return false;
        }
        if (!ok(item.start)) return false;
        for (const auto& s : item.segments) {
            if (!ok(s.c1) || !ok(s.c2) || !ok(s.end)) return false;
        }
    }
    return std::isfinite(scene.width) && std::isfinite(scene.height);
}

// Every primitive lies inside the scene bounds.
bool InsideBounds(const Scene& scene) {
    for (const auto& item : scene.items) {
        if (item.kind == PrimitiveKind::Rect &&
            (item.x < -0.5f || item.y < -0.5f || item.x + item.w > scene.width + 0.5f || item.y + item.h > scene.height + 0.5f)) {
            return false;
        }
        if (item.kind == PrimitiveKind::Text && (item.y < 0.0f || item.y > scene.height || item.x < 0.0f || item.x > scene.width)) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(MermaidTest, DetectsMermaidInfoString) {
    EXPECT_TRUE(IsMermaidLanguage("mermaid"));
    EXPECT_TRUE(IsMermaidLanguage("Mermaid"));
    EXPECT_TRUE(IsMermaidLanguage(" mermaid {theme: dark}"));
    EXPECT_FALSE(IsMermaidLanguage("mermaidx"));
    EXPECT_FALSE(IsMermaidLanguage("js"));
    EXPECT_FALSE(IsMermaidLanguage(""));
}

TEST(MermaidTest, FlowchartTopDownStacksNodes) {
    const auto result = Render("graph TD\n  A --> B\n  B --> C\n");
    ASSERT_TRUE(result.scene) << result.error;
    const Scene& s = *result.scene;
    const auto* a = FindText(s, "A");
    const auto* b = FindText(s, "B");
    const auto* c = FindText(s, "C");
    ASSERT_TRUE(a && b && c);
    EXPECT_LT(a->y, b->y);
    EXPECT_LT(b->y, c->y);
    EXPECT_NEAR(a->x, c->x, 1.0f);
    EXPECT_TRUE(AllFinite(s));
    EXPECT_TRUE(InsideBounds(s));
}

TEST(MermaidTest, FlowchartLeftToRightAndReversedDirections) {
    const auto lr = Render("flowchart LR\n  A --> B --> C");
    ASSERT_TRUE(lr.scene) << lr.error;
    EXPECT_LT(FindText(*lr.scene, "A")->x, FindText(*lr.scene, "B")->x);
    EXPECT_LT(FindText(*lr.scene, "B")->x, FindText(*lr.scene, "C")->x);

    const auto bt = Render("graph BT\n  A --> B");
    ASSERT_TRUE(bt.scene) << bt.error;
    EXPECT_GT(FindText(*bt.scene, "A")->y, FindText(*bt.scene, "B")->y);

    const auto rl = Render("graph RL\n  A --> B");
    ASSERT_TRUE(rl.scene) << rl.error;
    EXPECT_GT(FindText(*rl.scene, "A")->x, FindText(*rl.scene, "B")->x);
}

TEST(MermaidTest, FlowchartShapesLabelsAndEntities) {
    const auto result = Render(
        "flowchart TD\n"
        "  A[Inicio] --> B{¿Válido?}\n"
        "  B -->|Sí| C([Fin])\n"
        "  B -- No --> D[(Base de datos)]\n"
        "  D --> E((Círculo)) & F>Bandera] & G{{Hexágono}}\n"
        "  G --> H[/Entrada/] --> I[\\Salida\\] --> J[/Trapecio\\] --> K[[Subrutina]]\n"
        "  K --> L[\"Texto con #quot;comillas#quot; y &amp;\"]\n");
    ASSERT_TRUE(result.scene) << result.error;
    for (const char* label : {"Inicio", "¿Válido?", "Sí", "No", "Fin", "Base de datos", "Círculo", "Bandera",
                              "Hexágono", "Entrada", "Salida", "Trapecio", "Subrutina", "Texto con \"comillas\" y &"}) {
        EXPECT_NE(FindText(*result.scene, label), nullptr) << label;
    }
    EXPECT_TRUE(AllFinite(*result.scene));
    EXPECT_TRUE(InsideBounds(*result.scene));
}

TEST(MermaidTest, FlowchartSiblingsDoNotOverlap) {
    const auto result = Render("graph TD\n  R --> A1[Nodo uno] & A2[Nodo dos] & A3[Nodo tres] & A4[Nodo cuatro]");
    ASSERT_TRUE(result.scene) << result.error;
    const Scene& s = *result.scene;
    std::vector<const Primitive*> row;
    for (const char* label : {"Nodo uno", "Nodo dos", "Nodo tres", "Nodo cuatro"}) {
        const auto* t = FindText(s, label);
        ASSERT_NE(t, nullptr);
        row.push_back(t);
    }
    for (size_t i = 0; i < row.size(); ++i) {
        for (size_t j = i + 1; j < row.size(); ++j) {
            EXPECT_NEAR(row[i]->y, row[j]->y, 0.5f);
            const float minDistance = (EstimateTextWidth(row[i]->text, 14.0f, false) +
                                       EstimateTextWidth(row[j]->text, 14.0f, false)) * 0.5f + 20.0f;
            EXPECT_GE(std::fabs(row[i]->x - row[j]->x), minDistance);
        }
    }
    // The parent is centred over its children.
    const float childrenMid = (row.front()->x + row.back()->x) * 0.5f;
    EXPECT_NEAR(FindText(s, "R")->x, childrenMid, 30.0f);
}

TEST(MermaidTest, FlowchartCyclesSelfLoopsAndLinkStyles) {
    const auto result = Render(
        "graph LR\n"
        "  A --> B --> C --> A\n"
        "  B --> B\n"
        "  C -.-> D\n"
        "  D ==> E\n"
        "  E --- F\n"
        "  F ~~~ G\n"
        "  G <--> H\n"
        "  H --o I\n"
        "  I --x J\n"
        "  A ---> J\n"
        "  linkStyle 0 stroke:#ff3,stroke-width:4px\n"
        "  classDef rojo fill:#f96,stroke:#333\n"
        "  class A,B rojo\n"
        "  C:::rojo --> K\n"
        "  style D fill:#bbf,color:#fff\n");
    ASSERT_TRUE(result.scene) << result.error;
    EXPECT_TRUE(AllFinite(*result.scene));
    EXPECT_TRUE(InsideBounds(*result.scene));

    bool customFill = false;
    bool customEdge = false;
    for (const auto& item : result.scene->items) {
        if (item.fill.role == Role::Custom && item.fill.rgb == 0xFF9966) customFill = true;
        if (item.stroke.role == Role::Custom && item.stroke.rgb == 0xFFFF33 && item.strokeWidth == 4.0f) customEdge = true;
    }
    EXPECT_TRUE(customFill);
    EXPECT_TRUE(customEdge);

    // Text on a custom fill contrasts with the fill in both themes; explicit colours win.
    const auto* a = FindText(*result.scene, "A");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->fill.role, Role::Custom);
    EXPECT_EQ(a->fill.rgb, 0x1F2328u);
    const auto* d = FindText(*result.scene, "D");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->fill.rgb, 0xFFFFFFu);
}

TEST(MermaidTest, FlowchartSubgraphsEncloseTheirNodes) {
    const auto result = Render(
        "flowchart TB\n"
        "  c1 --> a2\n"
        "  subgraph uno [Primero]\n"
        "    a1 --> a2\n"
        "  end\n"
        "  subgraph dos [Segundo]\n"
        "    b1 --> b2\n"
        "    subgraph tres [Tercero]\n"
        "      t1\n"
        "    end\n"
        "  end\n"
        "  uno --> dos\n"
        "  b2 --> t1\n");
    ASSERT_TRUE(result.scene) << result.error;
    const Scene& s = *result.scene;
    const auto* title = FindText(s, "Primero");
    const auto* a1 = FindText(s, "a1");
    ASSERT_TRUE(title && a1 && FindText(s, "Segundo") && FindText(s, "Tercero"));
    EXPECT_EQ(FindText(s, "uno"), nullptr); // Edges to a subgraph do not create a stray node

    // A frame surrounds a1 and sits above it with the title.
    bool enclosed = false;
    for (const auto& item : s.items) {
        if (item.kind == PrimitiveKind::Rect && item.fill.role == Role::ClusterFill && item.x < a1->x &&
            item.x + item.w > a1->x && item.y < title->y && item.y + item.h > a1->y) {
            enclosed = true;
        }
    }
    EXPECT_TRUE(enclosed);
    EXPECT_TRUE(InsideBounds(s));
}

TEST(MermaidTest, FlowchartReportsSyntaxErrors) {
    const auto unclosed = Render("graph TD\n  A[sin cerrar --> B\n");
    EXPECT_FALSE(unclosed.scene);
    EXPECT_FALSE(unclosed.error.empty());

    const auto garbage = Render("graph TD\n  A --> \n");
    EXPECT_FALSE(garbage.scene);
    EXPECT_FALSE(garbage.error.empty());
}

TEST(MermaidTest, UnsupportedDiagramTypeIsReported) {
    const auto result = Render("classDiagram\n  Animal <|-- Pato\n");
    EXPECT_FALSE(result.scene);
    EXPECT_NE(result.error.find("classDiagram"), std::string::npos);

    EXPECT_FALSE(Render("").scene);
    EXPECT_FALSE(Render("%% solo un comentario\n").scene);
}

TEST(MermaidTest, FrontMatterTitleAndDirectivesAreAccepted) {
    const auto result = Render("---\ntitle: Mi flujo\n---\n%%{init: {'theme':'dark'}}%%\ngraph LR; A-->B; B-->C\n");
    ASSERT_TRUE(result.scene) << result.error;
    EXPECT_NE(FindText(*result.scene, "Mi flujo"), nullptr);
    EXPECT_NE(FindText(*result.scene, "C"), nullptr);
}

TEST(MermaidTest, SequenceDiagram) {
    const auto result = Render(
        "sequenceDiagram\n"
        "  autonumber\n"
        "  title Saludo\n"
        "  participant A as Alice\n"
        "  actor B as Bob\n"
        "  A->>+B: Hola Bob, ¿qué tal?\n"
        "  loop Cada minuto\n"
        "    B-->>A: ¡Bien!\n"
        "  end\n"
        "  alt es de día\n"
        "    A-)B: Café\n"
        "  else es de noche\n"
        "    A-xB: Té\n"
        "  end\n"
        "  Note right of B: Bob piensa\n"
        "  Note over A,B: Una nota larga que cruza\n"
        "  B->>B: Pensar\n"
        "  B-->>-A: Adiós\n"
        "  rect rgb(200, 220, 255)\n"
        "    A->B: Sin flecha\n"
        "  end\n");
    ASSERT_TRUE(result.scene) << result.error;
    const Scene& s = *result.scene;
    for (const char* label : {"Alice", "Bob", "Hola Bob, ¿qué tal?", "loop", "[Cada minuto]", "alt", "[es de día]",
                              "[es de noche]", "Bob piensa", "Una nota larga que cruza", "Pensar", "Saludo", "1"}) {
        EXPECT_NE(FindText(s, label), nullptr) << label;
    }
    EXPECT_LT(FindText(s, "Alice")->x, FindText(s, "Bob")->x);
    EXPECT_TRUE(AllFinite(s));
    EXPECT_TRUE(InsideBounds(s));
}

TEST(MermaidTest, SequenceDiagramErrors) {
    EXPECT_FALSE(Render("sequenceDiagram\n").scene);
    const auto bad = Render("sequenceDiagram\n  Alice envía a Bob\n");
    EXPECT_FALSE(bad.scene);
    EXPECT_FALSE(bad.error.empty());
}

TEST(MermaidTest, StateDiagram) {
    const auto result = Render(
        "stateDiagram-v2\n"
        "  [*] --> Quieto\n"
        "  Quieto --> Moviendo : empujar\n"
        "  Moviendo --> Quieto : frenar\n"
        "  Moviendo --> Choque\n"
        "  state Choque {\n"
        "    [*] --> Dañado\n"
        "    Dañado --> [*]\n"
        "  }\n"
        "  Choque --> [*]\n"
        "  state bifurca <<fork>>\n"
        "  Quieto --> bifurca\n"
        "  note right of Quieto : Estado inicial\n");
    ASSERT_TRUE(result.scene) << result.error;
    const Scene& s = *result.scene;
    for (const char* label : {"Quieto", "Moviendo", "empujar", "frenar", "Choque", "Dañado", "Estado inicial"}) {
        EXPECT_NE(FindText(s, label), nullptr) << label;
    }
    EXPECT_LT(FindText(s, "Quieto")->y, FindText(s, "Moviendo")->y);
    EXPECT_TRUE(AllFinite(s));
    EXPECT_TRUE(InsideBounds(s));
}

TEST(MermaidTest, PieChart) {
    const auto result = Render(
        "pie showData\n"
        "  title Mascotas\n"
        "  \"Perros\" : 386\n"
        "  \"Gatos\" : 85.5\n"
        "  \"Ratas\" : 15\n");
    ASSERT_TRUE(result.scene) << result.error;
    const Scene& s = *result.scene;
    EXPECT_NE(FindText(s, "Mascotas"), nullptr);
    EXPECT_NE(FindText(s, "Perros [386]"), nullptr);
    EXPECT_NE(FindText(s, "Gatos [85.5]"), nullptr);
    EXPECT_NE(FindText(s, "79%"), nullptr);
    EXPECT_TRUE(AllFinite(s));

    EXPECT_FALSE(Render("pie\n  \"Nada\" : 0\n").scene);
}

TEST(MermaidTest, SvgOutputIsSelfContainedAndEscaped) {
    const auto result = Render("graph LR\n  A[\"a < b & c\"] --> B");
    ASSERT_TRUE(result.scene) << result.error;
    const std::string svg = WriteSvg(*result.scene, "Diagrama");
    EXPECT_EQ(svg.rfind("<svg", 0), 0u);
    EXPECT_NE(svg.find("</svg>"), std::string::npos);
    EXPECT_NE(svg.find("a &lt; b &amp; c"), std::string::npos);
    EXPECT_NE(svg.find("var(--pluma-mm-node-fill,#"), std::string::npos);
    EXPECT_NE(svg.find("<title>Diagrama</title>"), std::string::npos);
    EXPECT_EQ(svg.find("\"nan"), std::string::npos);
    EXPECT_EQ(svg.find(" nan"), std::string::npos);

    const std::string vars = SvgPaletteVariables(Palette::Dark());
    EXPECT_NE(vars.find("--pluma-mm-text:#d4d4d4;"), std::string::npos);
}

TEST(MermaidTest, LargeInputIsRejectedGracefully) {
    std::string source = "graph TD\n";
    for (int i = 0; i < 600; ++i) source += "  N" + std::to_string(i) + " --> N" + std::to_string(i + 1) + "\n";
    const auto result = Render(source);
    EXPECT_FALSE(result.scene);
    EXPECT_FALSE(result.error.empty());
}
