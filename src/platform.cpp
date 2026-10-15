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
#include <optional>
#include <utility>
#include <windows.h>
#include <QGuiApplication>
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

namespace {

#ifdef Q_OS_WIN

// `RegisterHotKey()` requires an arbitrary number. Thankfully we just need one.
constexpr int hotkeyIdentifier = 1;

// Translates from QKeySequence to the virtual-key code and modifiers that
// `RegisterHotKey()` expects. Handles the key families that make sense for a
// global combination: letters, digits, Space, and the function keys.
std::optional<std::pair<quint32, quint32>> toNativeShortcut(const QKeySequence& shortcut)
{
    if (shortcut.isEmpty())
        return std::nullopt;

    const int combined = shortcut[0].toCombined();
    const auto modifier
        = static_cast<Qt::KeyboardModifiers>(combined & Qt::KeyboardModifierMask);
    const int key = combined & ~Qt::KeyboardModifierMask;

    quint32 nativeModifiers = 0;
    if (modifier & Qt::ControlModifier)
        nativeModifiers |= MOD_CONTROL;
    if (modifier & Qt::AltModifier)
        nativeModifiers |= MOD_ALT;
    if (modifier & Qt::ShiftModifier)
        nativeModifiers |= MOD_SHIFT;
    if (modifier & Qt::MetaModifier)
        nativeModifiers |= MOD_WIN;

    quint32 virtualKey = 0;
    // The ASCII range maps directly: 'A' == VK_A, '0' == VK_0.
    if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9)) {
        virtualKey = static_cast<quint32>(key);
    } else if (key == Qt::Key_Space) {
        virtualKey = VK_SPACE;
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        // Qt::Key_F1 (0x01000004) to Qt::Key_F24 is a contiguous range, and
        // thankfully VK_F1 (0x70) to VK_F24 (0x87) is as well.
        virtualKey = VK_F1 + static_cast<quint32>(key - Qt::Key_F1);
    } else {
        return std::nullopt;
    }

    return std::make_pair(virtualKey, nativeModifiers);
}

#endif // Q_OS_WIN

} // namespace

GlobalShortcut::GlobalShortcut(QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    // The hotkey message is delivered to the thread queue, so no window is
    // needed: RegisterHotKey() with a null HWND, and the filter installed on
    // the application object.
    QGuiApplication::instance()->installNativeEventFilter(this);
#endif
}

GlobalShortcut::~GlobalShortcut()
{
#ifdef Q_OS_WIN
    QGuiApplication::instance()->removeNativeEventFilter(this);
    if (m_registered)
        UnregisterHotKey(nullptr, hotkeyIdentifier);
#endif
}

bool GlobalShortcut::setShortcut(QKeySequence shortcut)
{
#ifdef Q_OS_WIN
    if (m_registered) {
        UnregisterHotKey(nullptr, hotkeyIdentifier);
        m_registered = false;
    }

    m_shortcut = shortcut;
    const auto nativeShortcut = toNativeShortcut(shortcut);
    if (!nativeShortcut)
        return false;

    const auto [virtualKey, modifiers] = *nativeShortcut;

    // Null-window registration: WM_HOTKEY is posted to the thread's message
    // queue, which the native event filter below receives.
    m_registered = RegisterHotKey(nullptr, hotkeyIdentifier, modifiers, virtualKey);
    return m_registered;
#else
    Q_UNUSED(shortcut);
    return false;
#endif
}

bool GlobalShortcut::nativeEventFilter(const QByteArray& eventType,
                                       void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(result);

    if (eventType != QByteArrayLiteral("windows_generic_MSG"))
        return false;

    const MSG* nativeMessage = static_cast<const MSG*>(message);
    if (nativeMessage->message != WM_HOTKEY)
        return false;

    if (nativeMessage->wParam != hotkeyIdentifier)
        return false;

    emit activated();
    return true;
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
#endif
}

} // namespace platform
