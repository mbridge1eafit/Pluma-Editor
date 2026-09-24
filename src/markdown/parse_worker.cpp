#include "parse_worker.h"
#include "md4c_adapter.h"

#include <algorithm>

namespace Pluma::Markdown {

ParseWorker::ParseWorker(HWND targetHwnd, UINT notifyMsg, std::chrono::milliseconds debounceMs)
    : m_targetHwnd(targetHwnd),
      m_notifyMsg(notifyMsg),
      m_debounceMs(debounceMs),
      m_thread([this](std::stop_token st) { WorkerLoop(st); }) {
}

ParseWorker::~ParseWorker() {
    Stop();
}

void ParseWorker::Stop() {
    if (m_thread.joinable()) {
        m_thread.request_stop();
        WakeConditionVariable(&m_cv);
        m_thread.join();
    }
}

void ParseWorker::RequestParse(std::string markdown, uint64_t version) {
    AcquireSRWLockExclusive(&m_lock);
    m_pendingText = std::move(markdown);
    m_pendingVersion = version;
    m_hasPending = true;
    m_requestTime = std::chrono::steady_clock::now();
    WakeConditionVariable(&m_cv);
    ReleaseSRWLockExclusive(&m_lock);
}

void ParseWorker::WorkerLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::string textToParse;
        uint64_t versionToParse = 0;

        AcquireSRWLockExclusive(&m_lock);

        // Wait until there is pending work or stop is requested
        while (!stopToken.stop_requested() && !m_hasPending) {
            SleepConditionVariableSRW(&m_cv, &m_lock, INFINITE, 0);
        }

        if (stopToken.stop_requested()) {
            ReleaseSRWLockExclusive(&m_lock);
            break;
        }

        // Check debounce timer (F-08)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_requestTime);
        if (elapsed < m_debounceMs) {
            auto remaining = m_debounceMs - elapsed;
            DWORD waitMs = static_cast<DWORD>(remaining.count());
            if (waitMs == 0) {
                waitMs = 1;
            }
            SleepConditionVariableSRW(&m_cv, &m_lock, waitMs, 0);
            ReleaseSRWLockExclusive(&m_lock);
            continue; // Re-evaluate condition and debounce
        }

        // Debounce passed: claim pending work
        textToParse = std::move(m_pendingText);
        versionToParse = m_pendingVersion;
        m_hasPending = false;

        ReleaseSRWLockExclusive(&m_lock);

        if (stopToken.stop_requested()) {
            break;
        }

        // Parse markdown on background thread outside lock (F-08)
        auto tree = Md4cAdapter::Parse(textToParse, versionToParse);

        // Check if obsolete before posting (F-08)
        bool isObsolete = false;
        AcquireSRWLockShared(&m_lock);
        if (m_hasPending && m_pendingVersion > versionToParse) {
            isObsolete = true;
        }
        ReleaseSRWLockShared(&m_lock);

        if (!isObsolete && !stopToken.stop_requested() && m_targetHwnd) {
            // Transfer ownership to UI message queue. Receiver is responsible for deletion.
            BlockTree* rawTree = tree.release();
            if (!PostMessageW(m_targetHwnd, m_notifyMsg,
                              static_cast<WPARAM>(versionToParse),
                              reinterpret_cast<LPARAM>(rawTree))) {
                // Window destroyed or message queue full: free memory to prevent leak
                delete rawTree;
            }
        }
    }
}

} // namespace Pluma::Markdown
