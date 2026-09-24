#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>

#include "../config/settings.h"

namespace Pluma::App {

struct SettingsDialogCallbacks {
    // Applies the edited settings to the running application (Aceptar / Aplicar).
    std::function<void(const Config::Settings&)> apply;
    // Starts the "set as default Markdown editor" flow.
    std::function<void()> makeDefaultEditor;
    // True when Pluma currently opens .md files.
    std::function<bool()> isDefaultEditor;
};

// Modal "Preferencias" dialog. `settings` is the current configuration; runtime-only values
// (window placement, last file...) are preserved. Returns true when changes were applied.
bool ShowSettingsDialog(HWND owner, HINSTANCE hInstance, const Config::Settings& settings,
                        const SettingsDialogCallbacks& callbacks);

} // namespace Pluma::App
