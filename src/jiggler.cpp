#include "jiggler.h"
#include "constants.h"

#include <QCursor>
#include <QDebug>
#include <QMetaEnum>
#include <QRandomGenerator>
#include <QSettings>

namespace {

const QVector<QPoint>& patternFor(Jiggler::Mode mode)
{
    // Linear: slide left and right, without vertical movement.
    static const QVector<QPoint> linearPattern = {
        { 1, 0 },
        { -1, 0 },
    };

    // Diagonal: move right and down, then left and up.
    static const QVector<QPoint> diagonalPattern = {
        { 1, 1 },
        { -1, -1 },
    };

    // Circle: go around a small circle.
    static const QVector<QPoint> circlePattern = {
        { 3, 2 },
        { 2, 3 },
        { -2, 3 },
        { -3, 2 },
        { -3, -2 },
        { -2, -3 },
        { 2, -3 },
        { 3, -2 },
    };

    // Invisible: no movement; the injected (0, 0) event only resets the idle timer.
    static const QVector<QPoint> invisiblePattern = {
        { 0, 0 },
    };

    switch (mode) {
    case Jiggler::Mode::Linear:
        break;
    case Jiggler::Mode::Diagonal:
        return diagonalPattern;
    case Jiggler::Mode::Circle:
        return circlePattern;
    case Jiggler::Mode::Invisible:
        return invisiblePattern;
    }

    return linearPattern;
}

Jiggler::Mode modeFromString(const QString& name)
{
    const QMetaEnum metaEnum = QMetaEnum::fromType<Jiggler::Mode>();
    bool conversionSucceeded = false;
    const int value = metaEnum.keyToValue(name.toUtf8().constData(), &conversionSucceeded);
    if (conversionSucceeded)
        return static_cast<Jiggler::Mode>(value);

    return Jiggler::Mode::Linear;
}

int clamped(int value, int minimum, int maximum)
{
    return qBound(minimum, value, maximum);
}

} // namespace

Jiggler::Jiggler(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    m_jiggling = settings.value(QStringLiteral("jiggle"), constants::DefaultJiggling).toBool();
    m_mode = modeFromString(settings.value(QStringLiteral("mode"), QStringLiteral("Linear")).toString());
    m_periodSeconds = clamped(settings.value(QStringLiteral("periodSeconds"),
        constants::DefaultPeriodSeconds).toInt(), constants::MinimumPeriodSeconds,
        constants::MaximumPeriodSeconds);
    m_distanceMultiplier = clamped(settings.value(QStringLiteral("distanceMultiplier"),
        constants::DefaultDistanceMultiplier).toInt(), constants::MinimumDistanceMultiplier,
        constants::MaximumDistanceMultiplier);
    m_randomInterval = settings.value(QStringLiteral("randomInterval"),
        constants::DefaultRandomInterval).toBool();
    m_showWindowOnStart = settings.value(QStringLiteral("showWindowOnStart"),
        constants::DefaultShowWindowOnStart).toBool();

    // Single shot: each tick schedules the next run itself, so the interval
    // can change on every jiggle while randomization is enabled.
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Jiggler::tick);
}

void Jiggler::start()
{
    if (m_jiggling) {
        platform::setStayAwake(true);
        scheduleNextJiggle();
    }
}

void Jiggler::setJiggling(bool jiggling)
{
    if (m_jiggling == jiggling)
        return;

    m_jiggling = jiggling;
    platform::setStayAwake(jiggling);
    QSettings settings;
    settings.setValue(QStringLiteral("jiggle"), m_jiggling);

    m_step = 0;

    // Re-sample the cursor so the movement detection does not compare
    // against a position from before the pause.
    m_lastCursorPosition = QCursor::pos();

    // Announce the state change before its consequences, so receivers
    // observe a consistent sequence (jiggling first, next interval after).
    emit jigglingChanged(m_jiggling);

    if (m_jiggling)
        scheduleNextJiggle();
    else
        m_timer.stop();
}

void Jiggler::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    m_step = 0;

    QSettings settings;
    settings.setValue(QStringLiteral("mode"), QMetaEnum::fromType<Mode>().valueToKey(static_cast<int>(m_mode)));

    emit modeChanged(m_mode);
}

void Jiggler::setPeriodSeconds(int seconds)
{
    const int clampedSeconds = clamped(seconds, constants::MinimumPeriodSeconds,
        constants::MaximumPeriodSeconds);
    if (m_periodSeconds == clampedSeconds)
        return;

    m_periodSeconds = clampedSeconds;

    QSettings settings;
    settings.setValue(QStringLiteral("periodSeconds"), m_periodSeconds);

    if (m_jiggling)
        scheduleNextJiggle();

    emit periodSecondsChanged(m_periodSeconds);
}

void Jiggler::setDistanceMultiplier(int multiplier)
{
    const int clampedMultiplier = clamped(multiplier, constants::MinimumDistanceMultiplier,
        constants::MaximumDistanceMultiplier);
    if (m_distanceMultiplier == clampedMultiplier)
        return;

    m_distanceMultiplier = clampedMultiplier;

    QSettings settings;
    settings.setValue(QStringLiteral("distanceMultiplier"), m_distanceMultiplier);

    emit distanceMultiplierChanged(m_distanceMultiplier);
}

void Jiggler::setRandomInterval(bool random)
{
    if (m_randomInterval == random)
        return;

    m_randomInterval = random;

    QSettings settings;
    settings.setValue(QStringLiteral("randomInterval"), m_randomInterval);

    if (m_jiggling)
        scheduleNextJiggle();

    emit randomIntervalChanged(m_randomInterval);
}

void Jiggler::setShowWindowOnStart(bool show)
{
    if (m_showWindowOnStart == show)
        return;

    m_showWindowOnStart = show;

    QSettings settings;
    qDebug() << Q_FUNC_INFO << show;
    settings.setValue(QStringLiteral("showWindowOnStart"), m_showWindowOnStart);
}

void Jiggler::tick()
{
    qDebug("Jiggler::tick()");
    // Do not interfere while the user is moving the mouse: skip this jiggle
    // if the cursor moved since the last check. The next jiggle is still
    // scheduled: the timer is single shot, so every path out of tick() must
    // reschedule it or the chain dies.
    const QPoint position = QCursor::pos();
    if (position != m_lastCursorPosition) {
        m_lastCursorPosition = position;
        scheduleNextJiggle();
        qDebug() << Q_FUNC_INFO << "skipped; user activity";
        emit userActivityChanged(true);
        return;
    }

    const QVector<QPoint>& pattern = patternFor(m_mode);
    const QPoint delta = pattern.at(m_step) * m_distanceMultiplier;
    m_step = (m_step + 1) % pattern.size();

    // Injected movement, not an absolute move: a relative SendInput both
    // moves the cursor and resets the system idle timer, which absolute
    // moves do not (they leave the machine free to lock itself).
    platform::moveMouseBy(delta);

    // Account for our own injected movement, so the next tick does not
    // mistake it for user activity.
    m_lastCursorPosition = QCursor::pos();

    scheduleNextJiggle();
    emit userActivityChanged(false);
}

void Jiggler::scheduleNextJiggle()
{
    int seconds = m_periodSeconds;
    if (m_randomInterval && m_periodSeconds > 1)
        seconds = QRandomGenerator::global()->bounded(constants::MinimumPeriodSeconds,
            m_periodSeconds + 1);

    m_timer.start(seconds * 1000);
    emit nextIntervalChanged(seconds);
}
