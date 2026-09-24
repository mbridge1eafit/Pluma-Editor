#include "sync_scroll.h"

namespace Pluma::Sync {

SyncScrollController::SyncScrollController(Editor::EditorView* editor, Preview::PreviewView* preview)
    : m_editor(editor), m_preview(preview) {
    if (m_preview) {
        m_preview->SetOnScrollCallback([this](int line) {
            OnPreviewScrolled(line);
        });
    }
}

void SyncScrollController::OnEditorScrolled() {
    if (!m_enabled || !m_editor || !m_preview) return;
    if (m_activeSource == ScrollSource::Preview) return; // Prevent echo bounce

    const int docLine = m_editor->GetFirstVisibleDocLine();
    if (docLine == m_lastEditorLine) return;
    m_lastEditorLine = docLine;

    m_activeSource = ScrollSource::Editor;
    m_preview->ScrollToLine(docLine);
    m_lastPreviewLine = m_preview->GetLineForCurrentScroll();
    m_activeSource = ScrollSource::None;
}

void SyncScrollController::OnPreviewScrolled(int previewLine) {
    if (!m_enabled || !m_editor || !m_preview) return;
    if (m_activeSource == ScrollSource::Editor) return; // Prevent echo bounce

    if (previewLine == m_lastPreviewLine) return;
    m_lastPreviewLine = previewLine;

    m_activeSource = ScrollSource::Preview;
    m_editor->ScrollToDocLine(previewLine);
    // Scintilla reports the scroll later (SCN_UPDATEUI). Remember where it landed so that
    // notification is recognised as our own echo and does not drag the preview back.
    m_lastEditorLine = m_editor->GetFirstVisibleDocLine();
    m_activeSource = ScrollSource::None;
}

void SyncScrollController::Reset() noexcept {
    m_lastEditorLine = -1;
    m_lastPreviewLine = -1;
    m_activeSource = ScrollSource::None;
}

} // namespace Pluma::Sync
