#ifndef JIGGLER_H
#define JIGGLER_H

#include "constants.h"
#include "platform.h"

#include <QObject>
#include <QPoint>
#include <QTimer>

// Keeps the system from going idle by injecting mouse movement events at
// regular intervals.
//
// No Widgets dependency, so a different frontend (for example QSkinny)
// can drive it later.
class Jiggler : public QObject
{
    Q_OBJECT

public:
    enum class Mode
    {
        Linear, // Slide left and right without vertical movement.
        Diagonal, // Slide diagonally: right and down, then left and up.
        Circle, // Move around a small circle.
        Invisible // Send input events without moving the cursor at all.
    };
    Q_ENUM(Mode)

    explicit Jiggler(QObject* parent = nullptr);

    void start();

    bool isJiggling() const { return m_jiggling; }
    Mode mode() const { return m_mode; }
    int periodSeconds() const { return m_periodSeconds; }
    int distanceMultiplier() const { return m_distanceMultiplier; }
    bool randomInterval() const { return m_randomInterval; }
    bool showWindowOnStart() const { return m_showWindowOnStart; }

public slots:
    void setJiggling(bool jiggling);
    void setMode(Jiggler::Mode mode);
    void setPeriodSeconds(int seconds);
    void setDistanceMultiplier(int multiplier);
    void setRandomInterval(bool random);
    void setShowWindowOnStart(bool show);

signals:
    void jigglingChanged(bool jiggling);
    void modeChanged(Jiggler::Mode mode);
    void periodSecondsChanged(int seconds);
    void distanceMultiplierChanged(int multiplier);
    void randomIntervalChanged(bool random);
    // Seconds until the next jiggle; differs from the period while the
    // random interval is enabled.
    void nextIntervalChanged(int seconds);
    // Emitted after every tick with its outcome: true when the jiggle was
    // skipped because the user was moving the mouse, false when it was
    // injected.
    void userActivityChanged(bool active);

private slots:
    void tick();

private:
    void scheduleNextJiggle();

    bool m_jiggling = constants::DefaultJiggling;
    Mode m_mode = Mode::Linear;
    int m_periodSeconds = constants::DefaultPeriodSeconds;
    int m_distanceMultiplier = constants::DefaultDistanceMultiplier;
    bool m_randomInterval = constants::DefaultRandomInterval;
    bool m_showWindowOnStart = constants::DefaultShowWindowOnStart;

    QTimer m_timer;
    int m_step = 0;
    QPoint m_lastCursorPosition;
};

#endif // JIGGLER_H
