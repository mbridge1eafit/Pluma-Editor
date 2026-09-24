# ==============================================================================
# md4c 0.5.2 (CommonMark + GFM SAX Parser)
# ==============================================================================
add_library(md4c STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/src/md4c.c
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/src/entity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/src/md4c-html.c
)
target_include_directories(md4c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/md4c/src
)
if(MSVC)
    target_compile_options(md4c PRIVATE /W3 /wd4996)
endif()

# ==============================================================================
# Scintilla 5.5.3 (Win32 / Direct2D Static Library)
# ==============================================================================
set(SCINTILLA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/scintilla)

set(SCINTILLA_SOURCES
    ${SCINTILLA_DIR}/win32/ScintillaWin.cxx
    ${SCINTILLA_DIR}/win32/PlatWin.cxx
    ${SCINTILLA_DIR}/win32/HanjaDic.cxx
    ${SCINTILLA_DIR}/src/AutoComplete.cxx
    ${SCINTILLA_DIR}/src/CallTip.cxx
    ${SCINTILLA_DIR}/src/CaseConvert.cxx
    ${SCINTILLA_DIR}/src/CaseFolder.cxx
    ${SCINTILLA_DIR}/src/CellBuffer.cxx
    ${SCINTILLA_DIR}/src/ChangeHistory.cxx
    ${SCINTILLA_DIR}/src/CharClassify.cxx
    ${SCINTILLA_DIR}/src/CharacterCategoryMap.cxx
    ${SCINTILLA_DIR}/src/CharacterType.cxx
    ${SCINTILLA_DIR}/src/ContractionState.cxx
    ${SCINTILLA_DIR}/src/DBCS.cxx
    ${SCINTILLA_DIR}/src/Decoration.cxx
    ${SCINTILLA_DIR}/src/Document.cxx
    ${SCINTILLA_DIR}/src/EditModel.cxx
    ${SCINTILLA_DIR}/src/Editor.cxx
    ${SCINTILLA_DIR}/src/EditView.cxx
    ${SCINTILLA_DIR}/src/Geometry.cxx
    ${SCINTILLA_DIR}/src/Indicator.cxx
    ${SCINTILLA_DIR}/src/KeyMap.cxx
    ${SCINTILLA_DIR}/src/LineMarker.cxx
    ${SCINTILLA_DIR}/src/MarginView.cxx
    ${SCINTILLA_DIR}/src/PerLine.cxx
    ${SCINTILLA_DIR}/src/PositionCache.cxx
    ${SCINTILLA_DIR}/src/RESearch.cxx
    ${SCINTILLA_DIR}/src/RunStyles.cxx
    ${SCINTILLA_DIR}/src/ScintillaBase.cxx
    ${SCINTILLA_DIR}/src/Selection.cxx
    ${SCINTILLA_DIR}/src/Style.cxx
    ${SCINTILLA_DIR}/src/UndoHistory.cxx
    ${SCINTILLA_DIR}/src/UniConversion.cxx
    ${SCINTILLA_DIR}/src/UniqueString.cxx
    ${SCINTILLA_DIR}/src/ViewStyle.cxx
    ${SCINTILLA_DIR}/src/XPM.cxx
)

add_library(scintilla STATIC ${SCINTILLA_SOURCES})

target_include_directories(scintilla PUBLIC
    ${SCINTILLA_DIR}/include
)
target_include_directories(scintilla PRIVATE
    ${SCINTILLA_DIR}/src
    ${SCINTILLA_DIR}/win32
)

target_compile_definitions(scintilla PRIVATE
    STATIC_BUILD
)

if(MSVC)
    target_compile_options(scintilla PRIVATE /W3 /wd4100 /wd4702 /wd4267 /wd4244)
endif()

target_link_libraries(scintilla PRIVATE
    user32
    gdi32
    d2d1
    dwrite
    imm32
    ole32
    uuid
)

# ==============================================================================
# Lexilla 5.4.3 (Markdown Lexer Static Library)
# ==============================================================================
set(LEXILLA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lexilla)

set(LEXILLA_SOURCES
    ${LEXILLA_DIR}/lexers/LexMarkdown.cxx
    ${LEXILLA_DIR}/lexlib/Accessor.cxx
    ${LEXILLA_DIR}/lexlib/CharacterCategory.cxx
    ${LEXILLA_DIR}/lexlib/CharacterSet.cxx
    ${LEXILLA_DIR}/lexlib/DefaultLexer.cxx
    ${LEXILLA_DIR}/lexlib/LexAccessor.cxx
    ${LEXILLA_DIR}/lexlib/LexerBase.cxx
    ${LEXILLA_DIR}/lexlib/LexerModule.cxx
    ${LEXILLA_DIR}/lexlib/LexerSimple.cxx
    ${LEXILLA_DIR}/lexlib/PropSetSimple.cxx
    ${LEXILLA_DIR}/lexlib/StyleContext.cxx
    ${LEXILLA_DIR}/lexlib/WordList.cxx
)

add_library(lexilla STATIC ${LEXILLA_SOURCES})

target_include_directories(lexilla PUBLIC
    ${LEXILLA_DIR}/include
    ${SCINTILLA_DIR}/include
)
target_include_directories(lexilla PRIVATE
    ${LEXILLA_DIR}/lexlib
)

target_compile_definitions(lexilla PRIVATE
    STATIC_BUILD
)

if(MSVC)
    target_compile_options(lexilla PRIVATE /W3 /wd4100 /wd4267 /wd4244)
endif()
