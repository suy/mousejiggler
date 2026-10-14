#pragma once

#include <QString>

namespace constants {

inline constexpr int MinimumPeriodSeconds = 1;
inline constexpr int MaximumPeriodSeconds = 3600;
inline constexpr int DefaultPeriodSeconds = 60;

inline constexpr int MinimumDistanceMultiplier = 1;
inline constexpr int MaximumDistanceMultiplier = 100;
inline constexpr int DefaultDistanceMultiplier = 1;

inline constexpr bool DefaultJiggling = true;
inline constexpr bool DefaultRandomInterval = false;
inline constexpr bool DefaultShowWindowOnStart = false;

inline const QString DefaultGlobalShortcut = QStringLiteral("Meta+Ctrl+J");

}

