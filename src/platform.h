#ifndef PLATFORM_H
#define PLATFORM_H

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

} // namespace platform

#endif // PLATFORM_H
