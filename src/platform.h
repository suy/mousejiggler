#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>
#include <QPoint>

namespace platform {

// Injects a relative mouse movement event. Passing (0, 0) resets the idle
// timer without moving the cursor (used by the Invisible mode).
void moveMouseBy(QPoint delta);

// Keeps the display and system awake while awake is true; restores the
// default power behavior otherwise.
void setStayAwake(bool stayAwake);

// Terminates the process with the given identifier. Failure is silent by
// design: the process may already be gone, and the caller observes the
// outcome through the single instance lock instead.
void terminateProcess(qint64 processId);

// A global (system-wide) keyboard shortcut, delivered while any application has
// the focus. Qt offers no cross-platform API for this, so the Windows build
// implements it with RegisterHotKey(). Other platforms get no registration, so
// isRegistered() always returns false.
class GlobalShortcut : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalShortcut(QObject* parent = nullptr);
    ~GlobalShortcut() override;

    // Registers the key combination globally. Returns `false` when the platform
    // has no implementation or the combination is taken by another application.
    bool setShortcut(QKeySequence shortcut);

    bool isRegistered() const { return m_registered; }

    // From QAbstractNativeEventFilter. Receives `WM_HOTKEY` on Windows.
    bool nativeEventFilter(const QByteArray& eventType, void* message,
                           qintptr* result) override;

signals:
    void activated();

private:
    QKeySequence m_shortcut;
    bool m_registered = false;
};

} // namespace platform

