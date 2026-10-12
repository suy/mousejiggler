// Platform abstraction: the Windows implementation does the real work
// (input injection, power management); every other platform gets stubs so
// the application still builds and runs for development on Linux/WSL. The
// variant is selected per function with the preprocessor, so both
// implementations read in one place and shared code (e.g. debug traces)
// can sit on top of the split.

#include "platform.h"

#include <QtDebug>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

namespace platform {

void moveMouseBy(QPoint delta)
{
    qDebug() << "platform::moveMouseBy()" << delta;
#ifdef Q_OS_WIN
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = delta.x();
    input.mi.dy = delta.y();
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    const UINT injectedEvents = SendInput(1, &input, sizeof(INPUT));
    if (injectedEvents != 1)
        qWarning() << Q_FUNC_INFO << "SendInput failed with error" << GetLastError();
#else
    Q_UNUSED(delta);
#endif
}

void setStayAwake(bool stayAwake)
{
#ifdef Q_OS_WIN
    EXECUTION_STATE flags = ES_CONTINUOUS;
    if (stayAwake)
        flags |= ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED;

    SetThreadExecutionState(flags);
#else
    Q_UNUSED(stayAwake);
#endif
}

void terminateProcess(qint64 processId)
{
    qDebug() << Q_FUNC_INFO << processId;
#ifdef Q_OS_WIN
    // PROCESS_TERMINATE only: the minimum access required to terminate.
    const HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, DWORD(processId));
    if (process) {
        // The exit code is unobservable: nothing waits for the terminated
        // process, which is a previous instance of this application.
        TerminateProcess(process, 0);
        CloseHandle(process);
    }
#else
    kill(pid_t(processId), SIGTERM);
#endif
}

} // namespace platform
