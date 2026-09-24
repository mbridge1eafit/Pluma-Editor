#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "../editor/editor_view.h"
#include "../preview/preview_view.h"

namespace Pluma::Sync {

enum class ScrollSource {
    None,
    Editor,
    Preview
};

class SyncScrollController {
public:
    SyncScrollController(Editor::EditorView* editor, Preview::PreviewView* preview);
    ~SyncScrollController() = default;

    SyncScrollController(const SyncScrollController&) = delete;
    SyncScrollController& operator=(const SyncScrollController&) = delete;

    void SetEnabled(bool enabled) noexcept {
        m_enabled = enabled;
        Reset();
    }

    // Forgets the last synchronised positions (new document, view mode change)
    void Reset() noexcept;
    bool IsEnabled() const noexcept { return m_enabled; }

    // Called when Scintilla scrolls (e.g. from SCN_UPDATEUI with SC_UPDATE_V_SCROLL)
    void OnEditorScrolled();

    // Called when Preview scrolls (e.g. from mouse wheel or scrollbar)
    void OnPreviewScrolled(int previewLine);

private:
    Editor::EditorView* m_editor = nullptr;
    Preview::PreviewView* m_preview = nullptr;
    bool m_enabled = true;
    ScrollSource m_activeSource = ScrollSource::None;
    int m_lastEditorLine = -1;
    int m_lastPreviewLine = -1;
};

} // namespace Pluma::Sync
