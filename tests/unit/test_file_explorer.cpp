#include <gtest/gtest.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "explorer/file_explorer_panel.h"

namespace fs = std::filesystem;
using Pluma::Explorer::FileExplorerPanel;
using Pluma::Explorer::FirstVisibleSegment;
using Pluma::Explorer::FolderChain;
using Pluma::Explorer::FolderEntry;
using Pluma::Explorer::IsMarkdownFile;
using Pluma::Explorer::IsWithinFolder;
using Pluma::Explorer::ListFolder;
using Items = std::vector<std::wstring>;

class FileExplorerTest : public ::testing::Test {
protected:
    fs::path tempDir;

    void SetUp() override {
        tempDir = fs::temp_directory_path() / ("pluma_explorer_test_" + std::to_string(GetTickCount64()));
        fs::create_directories(tempDir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tempDir, ec);
    }

    void MakeFile(const fs::path& name) { std::ofstream(tempDir / name) << "# Título\n"; }
    void MakeFolder(const fs::path& name) { fs::create_directories(tempDir / name); }
    void Hide(const fs::path& name) {
        const fs::path path = tempDir / name;
        SetFileAttributesW(path.c_str(), GetFileAttributesW(path.c_str()) | FILE_ATTRIBUTE_HIDDEN);
    }

    static std::vector<std::wstring> Names(const std::vector<FolderEntry>& entries) {
        std::vector<std::wstring> names;
        for (const FolderEntry& entry : entries) names.push_back(entry.path.filename().wstring());
        return names;
    }
};

namespace {

// gtest cannot print std::filesystem::path (a range of paths): compare the strings.
std::vector<std::wstring> Chain(const fs::path& folder) {
    std::vector<std::wstring> chain;
    for (const fs::path& path : FolderChain(folder)) chain.push_back(path.wstring());
    return chain;
}

} // namespace

TEST_F(FileExplorerTest, RecognizesMarkdownExtensionsInAnyCase) {
    EXPECT_TRUE(IsMarkdownFile(L"notas.md"));
    EXPECT_TRUE(IsMarkdownFile(L"C:\\docs\\LEEME.MD"));
    EXPECT_TRUE(IsMarkdownFile(L"guía.Markdown"));
    EXPECT_TRUE(IsMarkdownFile(L"x.mdown"));
    EXPECT_FALSE(IsMarkdownFile(L"notas.txt"));
    EXPECT_FALSE(IsMarkdownFile(L"notas.md.bak"));
    EXPECT_FALSE(IsMarkdownFile(L"md"));
}

TEST_F(FileExplorerTest, ListsFoldersFirstThenMarkdownFilesInNaturalOrder) {
    MakeFile(L"b.md");
    MakeFile(L"A.md");
    MakeFile(L"Capítulo 10.md");
    MakeFile(L"Capítulo 2.md");
    MakeFile(L"notas.txt");
    MakeFile(L"imagen.png");
    MakeFolder(L"zeta");
    MakeFolder(L"Anexos");
    MakeFolder(L"carpeta.md"); // A folder, whatever its name

    const auto entries = ListFolder(tempDir);
    ASSERT_TRUE(entries.has_value());
    const std::vector<std::wstring> expected = {L"Anexos",        L"carpeta.md",     L"zeta", L"A.md",
                                                L"b.md",          L"Capítulo 2.md", L"Capítulo 10.md"};
    EXPECT_EQ(Names(*entries), expected);
    EXPECT_TRUE((*entries)[0].isFolder);
    EXPECT_TRUE((*entries)[2].isFolder);
    EXPECT_FALSE((*entries)[3].isFolder);
    EXPECT_EQ((*entries)[3].path.wstring(), (tempDir / L"A.md").wstring());
}

TEST_F(FileExplorerTest, SkipsHiddenEntriesAndVersionControlFolders) {
    MakeFolder(L".git");
    MakeFolder(L".svn");
    MakeFolder(L".github"); // Not hidden: it often holds Markdown templates
    MakeFolder(L"oculta");
    Hide(L"oculta");
    MakeFile(L"secreto.md");
    Hide(L"secreto.md");
    MakeFile(L"visible.md");

    const auto entries = ListFolder(tempDir);
    ASSERT_TRUE(entries.has_value());
    const std::vector<std::wstring> expected = {L".github", L"visible.md"};
    EXPECT_EQ(Names(*entries), expected);
}

TEST_F(FileExplorerTest, EmptyAndMissingFolders) {
    MakeFolder(L"vacía");
    const auto empty = ListFolder(tempDir / L"vacía");
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->empty());

    EXPECT_FALSE(ListFolder(tempDir / L"no_existe").has_value());
    MakeFile(L"archivo.md");
    EXPECT_FALSE(ListFolder(tempDir / L"archivo.md").has_value());
    EXPECT_FALSE(ListFolder({}).has_value());
}

TEST(FileExplorerPathTest, IsWithinFolderComparesWholeNamesIgnoringCase) {
    EXPECT_TRUE(IsWithinFolder(L"C:\\Docs\\notas\\a.md", L"C:\\Docs"));
    EXPECT_TRUE(IsWithinFolder(L"c:\\docs\\NOTAS\\a.md", L"C:\\Docs\\Notas\\"));
    EXPECT_TRUE(IsWithinFolder(L"C:\\Docs", L"C:\\Docs"));
    EXPECT_TRUE(IsWithinFolder(L"C:\\Docs\\a.md", L"C:\\"));
    EXPECT_TRUE(IsWithinFolder(L"C:/Docs/a.md", L"C:\\Docs"));
    EXPECT_FALSE(IsWithinFolder(L"C:\\Docs2\\a.md", L"C:\\Docs")); // Not a name prefix
    EXPECT_FALSE(IsWithinFolder(L"C:\\Docs", L"C:\\Docs\\notas"));
    EXPECT_FALSE(IsWithinFolder(L"D:\\Docs\\a.md", L"C:\\Docs"));
    EXPECT_FALSE(IsWithinFolder(L"C:\\Docs\\a.md", L""));
    EXPECT_TRUE(IsWithinFolder(L"\\\\servidor\\equipo\\docs\\a.md", L"\\\\SERVIDOR\\equipo"));
}

TEST(FileExplorerPathTest, FolderChainGoesFromTheTopOfTheDriveOrShare) {
    const std::vector<std::wstring> drive = {L"C:\\", L"C:\\Users", L"C:\\Users\\ana"};
    EXPECT_EQ(Chain(L"C:\\Users\\ana"), drive);
    EXPECT_EQ(Chain(L"C:\\Users\\ana\\"), drive);
    EXPECT_EQ(Chain(L"C:\\Users\\.\\ana"), drive);
    EXPECT_EQ(Chain(L"C:\\"), std::vector<std::wstring>{L"C:\\"});

    // A server is not a folder: the chain starts at the share.
    const std::vector<std::wstring> share = {L"\\\\servidor\\equipo", L"\\\\servidor\\equipo\\docs"};
    EXPECT_EQ(Chain(L"\\\\servidor\\equipo\\docs"), share);

    EXPECT_TRUE(FolderChain({}).empty());
}

TEST(FileExplorerPathTest, BreadcrumbKeepsTheLastSegmentsThatFit) {
    const std::vector<int> widths = {30, 60, 50, 40};
    // Everything fits: 30 + 60 + 50 + 40 + 3 separators of 10 = 210.
    EXPECT_EQ(FirstVisibleSegment(widths, 10, 20, 210), 0u);
    // Overflow (20) + 10 + 50 + 10 + 40 = 130: the last two segments.
    EXPECT_EQ(FirstVisibleSegment(widths, 10, 20, 130), 2u);
    EXPECT_EQ(FirstVisibleSegment(widths, 10, 20, 199), 2u);
    // Overflow + segments 1 to 3 = 200.
    EXPECT_EQ(FirstVisibleSegment(widths, 10, 20, 200), 1u);
    // Too narrow even for the last one: it is still shown (the panel cuts it with an ellipsis).
    EXPECT_EQ(FirstVisibleSegment(widths, 10, 20, 5), 3u);
    // The first segment only comes back with all the others.
    EXPECT_EQ(FirstVisibleSegment(widths, 10, 20, 209), 1u);

    const std::vector<int> one = {500};
    EXPECT_EQ(FirstVisibleSegment(one, 10, 20, 100), 0u);
    EXPECT_EQ(FirstVisibleSegment({}, 10, 20, 100), 0u);
}

// The panel itself, in a host window that is never shown (the panel counts as shown while its own
// window is visible).
class FileExplorerPanelTest : public FileExplorerTest {
protected:
    HWND host = nullptr;
    HWND tree = nullptr;
    FileExplorerPanel panel;

    void SetUp() override {
        FileExplorerTest::SetUp();
        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_TREEVIEW_CLASSES};
        InitCommonControlsEx(&icc);
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"PlumaExplorerTestHost";
        RegisterClassExW(&wc);
        host = CreateWindowExW(0, wc.lpszClassName, L"Host", WS_OVERLAPPEDWINDOW, 0, 0, 400, 600, nullptr, nullptr,
                               wc.hInstance, nullptr);
        ASSERT_NE(host, nullptr);
        ASSERT_TRUE(panel.Create(host, wc.hInstance));
        panel.SetBounds(0, 0, 300, 600);
        ShowWindow(panel.GetHwnd(), SW_SHOW);
        tree = FindWindowExW(panel.GetHwnd(), nullptr, WC_TREEVIEWW, nullptr);
        ASSERT_NE(tree, nullptr);
    }

    void TearDown() override {
        if (host) DestroyWindow(host);
        FileExplorerTest::TearDown();
    }

    // The expanded rows, indented two spaces per level; " *" marks the open document (bold) and
    // " <" the selection.
    Items Rows() const {
        Items rows;
        for (HTREEITEM item = TreeView_GetRoot(tree); item; item = TreeView_GetNextVisible(tree, item)) {
            wchar_t text[MAX_PATH]{};
            TVITEMW tvi{};
            tvi.mask = TVIF_TEXT | TVIF_STATE;
            tvi.stateMask = TVIS_BOLD;
            tvi.hItem = item;
            tvi.pszText = text;
            tvi.cchTextMax = MAX_PATH;
            TreeView_GetItem(tree, &tvi);
            std::wstring row;
            for (HTREEITEM parent = TreeView_GetParent(tree, item); parent; parent = TreeView_GetParent(tree, parent)) {
                row += L"  ";
            }
            row += text;
            if (tvi.state & TVIS_BOLD) row += L" *";
            if (item == TreeView_GetSelection(tree)) row += L" <";
            rows.push_back(row);
        }
        return rows;
    }
};

TEST_F(FileExplorerPanelTest, ShowsTheRootAndRevealsTheOpenDocument) {
    EXPECT_EQ(Rows(), Items{L"Abrir una carpeta..."});
    MakeFolder(L"a\\b");
    MakeFolder(L"z");
    MakeFile(L"a\\b\\c.md");
    MakeFile(L"a\\x.md");
    MakeFile(L"top.md");

    panel.SetRootFolder(tempDir);
    EXPECT_EQ(Rows(), (Items{L"a", L"z", L"top.md"}));

    panel.SetCurrentFile(tempDir / L"a\\b\\c.md");
    EXPECT_EQ(panel.GetRootFolder().wstring(), tempDir.wstring());
    EXPECT_EQ(Rows(), (Items{L"a", L"  b", L"    c.md * <", L"  x.md", L"z", L"top.md"}));

    // A new document keeps the folder on screen.
    panel.SetCurrentFile({});
    EXPECT_EQ(Rows(), (Items{L"a", L"  b", L"    c.md <", L"  x.md", L"z", L"top.md"}));
}

TEST_F(FileExplorerPanelTest, AFileSavedInAListedFolderAppears) {
    MakeFolder(L"a");
    MakeFile(L"a\\old.md");
    panel.SetRootFolder(tempDir);
    panel.SetCurrentFile(tempDir / L"a\\old.md");
    EXPECT_EQ(Rows(), (Items{L"a", L"  old.md * <"}));

    MakeFile(L"a\\new.md"); // "Save as" in the same folder
    panel.SetCurrentFile(tempDir / L"a\\new.md");
    EXPECT_EQ(Rows(), (Items{L"a", L"  new.md * <", L"  old.md"}));
}

TEST_F(FileExplorerPanelTest, ADocumentOutsideTheRootShowsItsFolder) {
    MakeFolder(L"uno");
    MakeFolder(L"dos");
    MakeFile(L"uno\\a.md");
    MakeFile(L"dos\\b.md");
    panel.SetRootFolder(tempDir / L"uno");
    panel.SetCurrentFile(tempDir / L"dos\\b.md");
    EXPECT_EQ(panel.GetRootFolder().wstring(), (tempDir / L"dos").wstring());
    EXPECT_EQ(Rows(), Items{L"b.md * <"});
}

TEST_F(FileExplorerPanelTest, GoingUpKeepsThePreviousRootOpenAndSelected) {
    MakeFolder(L"raíz\\sub");
    MakeFile(L"raíz\\sub\\doc.md");
    MakeFile(L"otro.md");
    panel.SetCurrentFile(tempDir / L"raíz\\sub\\doc.md");
    EXPECT_EQ(panel.GetRootFolder().wstring(), (tempDir / L"raíz\\sub").wstring());

    panel.SetRootFolder(tempDir);
    EXPECT_EQ(Rows(), (Items{L"raíz", L"  sub <", L"    doc.md *", L"otro.md"}));
}

TEST_F(FileExplorerPanelTest, NothingIsReadWhileHidden) {
    MakeFolder(L"a");
    MakeFile(L"a\\doc.md");
    ShowWindow(panel.GetHwnd(), SW_HIDE);
    panel.SetCurrentFile(tempDir / L"a\\doc.md");
    EXPECT_EQ(panel.GetRootFolder().wstring(), (tempDir / L"a").wstring());
    EXPECT_EQ(Rows(), Items{L"Abrir una carpeta..."});

    ShowWindow(panel.GetHwnd(), SW_SHOW);
    EXPECT_EQ(Rows(), Items{L"doc.md * <"});
}
