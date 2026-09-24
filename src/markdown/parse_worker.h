#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include "block_tree.h"

namespace Pluma::Markdown {

constexpr UINT WM_USER_PARSE_COMPLETE = WM_USER + 101;

class ParseWorker {
public:
    ParseWorker(HWND targetHwnd, UINT notifyMsg = WM_USER_PARSE_COMPLETE,
                std::chrono::milliseconds debounceMs = std::chrono::milliseconds(50));
    ~ParseWorker();

    ParseWorker(const ParseWorker&) = delete;
    ParseWorker& operator=(const ParseWorker&) = delete;

    // Requests asynchronous background parsing of a snapshot with debounce (F-08)
    void RequestParse(std::string markdown, uint64_t version);

    // Stops background worker thread
    void Stop();

private:
    void WorkerLoop(std::stop_token stopToken);

    HWND m_targetHwnd = nullptr;
    UINT m_notifyMsg = WM_USER_PARSE_COMPLETE;
    std::chrono::milliseconds m_debounceMs{50};

    SRWLOCK m_lock = SRWLOCK_INIT;
    CONDITION_VARIABLE m_cv = CONDITION_VARIABLE_INIT;

    std::string m_pendingText;
    uint64_t m_pendingVersion = 0;
    bool m_hasPending = false;
    std::chrono::steady_clock::time_point m_requestTime;

    std::jthread m_thread;
};

} // namespace Pluma::Markdown
