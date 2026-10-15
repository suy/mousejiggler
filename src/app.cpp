#include "constants.h"
#include "jiggler.h"
#include "mainwindow.h"
#include "platform.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QIcon>
#include <QKeySequence>
#include <QLocale>
#include <QLockFile>
#include <QMessageBox>
#include <QMetaEnum>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTranslator>

#include <optional>

namespace {

std::optional<Jiggler::Mode> modeFromName(const QString& name)
{
    const QMetaEnum metaEnum = QMetaEnum::fromType<Jiggler::Mode>();
    for (int index = 0, last = metaEnum.keyCount(); index < last; ++index) {
        if (name == QString::fromLatin1(metaEnum.key(index)).toLower())
            return static_cast<Jiggler::Mode>(metaEnum.value(index));
    }

    return std::nullopt;
}

} // namespace

int main(int argc, char* argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Alejandro Exojo"));
    QCoreApplication::setApplicationName(QStringLiteral("Qt Mouse Jiggler"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/mousejiggler.ico")));

    // Tiny hack to have a command line option that doesn't show in the default help.
    // This is useful in order to clear the config on Windows, which by default
    // goes to the damn registry, and it's not so useful to clear.
    if (application.arguments().contains("--clear")) {
        QSettings settings;
        settings.clear();
        return 42;
    }

    QTranslator translator(&application);
    const QString language = QLocale::system().name().section(QLatin1Char('_'), 0, 0);
    if ((language == QLatin1String("ca") || language == QLatin1String("es"))
        && translator.load(QStringLiteral(":/i18n/") + language + QStringLiteral(".qm"))) {
        application.installTranslator(&translator);
    }

    QCommandLineParser commandLineParser;
    QCommandLineOption jiggleOption({ QStringLiteral("j"), QStringLiteral("jiggle") },
        QCoreApplication::translate("main", "Start with jiggling enabled (the default)."));
    QCommandLineOption noJiggleOption({ QStringLiteral("no-jiggle") },
        QCoreApplication::translate("main", "Start with jiggling disabled."));
    QCommandLineOption minimizedOption({ QStringLiteral("m"), QStringLiteral("minimized") },
        QCoreApplication::translate("main", "Start hidden in the system tray (the default)."));
    QCommandLineOption windowOption({ QStringLiteral("show-window") },
        QCoreApplication::translate("main", "Start with the settings window visible."));
    QCommandLineOption modeOption({ QStringLiteral("o"), QStringLiteral("mode") },
        QCoreApplication::translate("main", "Jiggle mode: linear (the default), diagonal, circle or invisible."),
        QStringLiteral("mode"));
    QCommandLineOption randomOption({ QStringLiteral("r"), QStringLiteral("random") },
        QCoreApplication::translate("main", "Randomize the jiggle interval."));
    QCommandLineOption secondsOption({ QStringLiteral("s"), QStringLiteral("seconds") },
        QCoreApplication::translate("main", "Jiggle interval in seconds (%1 to %2).")
            .arg(constants::MinimumPeriodSeconds)
            .arg(constants::MaximumPeriodSeconds),
        QStringLiteral("seconds"));
    QCommandLineOption distanceOption({ QStringLiteral("d"), QStringLiteral("distance") },
        QCoreApplication::translate("main", "Distance multiplier (%1 to %2).")
            .arg(constants::MinimumDistanceMultiplier)
            .arg(constants::MaximumDistanceMultiplier),
        QStringLiteral("distance"));
    QCommandLineOption shortcutOption(QStringLiteral("shortcut"),
        QCoreApplication::translate("main",
            "Global shortcut to raise the window (e.g. Meta+Ctrl+J, the default)."),
        QStringLiteral("shortcut"));
    commandLineParser.addHelpOption();
    commandLineParser.addVersionOption();
    commandLineParser.addOption(jiggleOption);
    commandLineParser.addOption(noJiggleOption);
    commandLineParser.addOption(minimizedOption);
    commandLineParser.addOption(windowOption);
    commandLineParser.addOption(modeOption);
    commandLineParser.addOption(randomOption);
    commandLineParser.addOption(secondsOption);
    commandLineParser.addOption(distanceOption);
    commandLineParser.addOption(shortcutOption);
    commandLineParser.process(application);

    QLockFile lockFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/mousejiggler-single-instance.lock"));
    if (!lockFile.tryLock(0)) {
        // Another instance owns the lock: terminate it and take over, so the
        // command line values are applied by this process and we don't need to
        // use IPC to apply those settings to the current instance. Last
        // invocation wins, and tight race conditions are not a big concern for
        // this app.
        qint64 otherProcessId = 0;
        lockFile.getLockInfo(&otherProcessId, nullptr, nullptr);
        if (otherProcessId > 0)
            platform::terminateProcess(otherProcessId);
        // Termination is not immediate: the retry loop inside tryLock() waits
        // for the other process to die and its lock to go stale.
        const int takeoverTimeoutMilliseconds = 5000;
        if (!lockFile.tryLock(takeoverTimeoutMilliseconds)) {
            QMessageBox::critical(nullptr, QCoreApplication::applicationName(),
                QCoreApplication::translate("main", "Qt Mouse Jiggler is already running and could not be stopped."));
            return 1;
        }
    }

    // The logic and the interface are independent objects; this is the only
    // place that knows about both. Values cross the boundary, never calls.
    Jiggler jiggler;
    MainWindow mainWindow;

    platform::GlobalShortcut globalShortcut(&application);
    QSettings settings;
    const QString shortcut = commandLineParser.isSet(shortcutOption)
        ? commandLineParser.value(shortcutOption)
        : settings.value(QStringLiteral("globalShortcut"),
            constants::DefaultGlobalShortcut).toString();
    if (commandLineParser.isSet(shortcutOption))
        settings.setValue(QStringLiteral("globalShortcut"), shortcut);

    if (shortcut.isEmpty()) {
        // An explicitly empty value disables the shortcut.
        qDebug() << "Global shortcut disabled";
    } else if (!globalShortcut.setShortcut(QKeySequence(shortcut))) {
        qWarning() << "Global shortcut" << shortcut << "is unavailable"
                   << "(invalid combination, taken by another application,"
                   << "or unsupported platform)";
    }
    QObject::connect(&globalShortcut, &platform::GlobalShortcut::activated,
        &mainWindow, &MainWindow::onGlobalShortcutActivated);

    // Interface events drive the logic.
    QObject::connect(&mainWindow, &MainWindow::jigglingToggled, &jiggler, &Jiggler::setJiggling);
    // The combo box lists modes in Jiggler::Mode declaration order.
    QObject::connect(&mainWindow, &MainWindow::modeSelected, &jiggler,
        [&jiggler](int mode) { jiggler.setMode(static_cast<Jiggler::Mode>(mode)); });
    QObject::connect(&mainWindow, &MainWindow::periodChanged, &jiggler, &Jiggler::setPeriodSeconds);
    QObject::connect(&mainWindow, &MainWindow::distanceChanged, &jiggler, &Jiggler::setDistanceMultiplier);
    QObject::connect(&mainWindow, &MainWindow::randomIntervalToggled, &jiggler, &Jiggler::setRandomInterval);
    QObject::connect(&mainWindow, &MainWindow::showWindowOnStartToggled, &jiggler,
        &Jiggler::setShowWindowOnStart);

    // Logic state flows back into the interface.
    QObject::connect(&jiggler, &Jiggler::jigglingChanged, &mainWindow, &MainWindow::setJiggling);
    QObject::connect(&jiggler, &Jiggler::modeChanged, &mainWindow,
        [&mainWindow](Jiggler::Mode mode) { mainWindow.setModeSelected(static_cast<int>(mode)); });
    QObject::connect(&jiggler, &Jiggler::periodSecondsChanged, &mainWindow, &MainWindow::setPeriodSeconds);
    QObject::connect(&jiggler, &Jiggler::distanceMultiplierChanged, &mainWindow,
        &MainWindow::setDistanceMultiplier);
    QObject::connect(&jiggler, &Jiggler::randomIntervalChanged, &mainWindow,
        &MainWindow::setRandomInterval);
    QObject::connect(&jiggler, &Jiggler::nextIntervalChanged, &mainWindow, &MainWindow::setNextInterval);
    QObject::connect(&jiggler, &Jiggler::userActivityChanged, &mainWindow, &MainWindow::setUserActivity);

    // Initialize the interface from the persisted state.
    mainWindow.setJiggling(jiggler.isJiggling());
    mainWindow.setModeSelected(static_cast<int>(jiggler.mode()));
    mainWindow.setPeriodSeconds(jiggler.periodSeconds());
    mainWindow.setDistanceMultiplier(jiggler.distanceMultiplier());
    mainWindow.setRandomInterval(jiggler.randomInterval());
    mainWindow.setShowWindowOnStart(jiggler.showWindowOnStart());

    // Command line values override the stored settings.
    if (commandLineParser.isSet(modeOption)) {
        const std::optional<Jiggler::Mode> mode = modeFromName(commandLineParser.value(modeOption));
        if (!mode) {
            QMessageBox::critical(nullptr, QCoreApplication::applicationName(),
                QCoreApplication::translate("main", "Unknown jiggle mode: %1")
                    .arg(commandLineParser.value(modeOption)));
            return 2;
        }
        jiggler.setMode(*mode);
    }

    if (commandLineParser.isSet(secondsOption)) {
        bool conversionSucceeded = false;
        const int seconds = commandLineParser.value(secondsOption).toInt(&conversionSucceeded);
        if (!conversionSucceeded) {
            QMessageBox::critical(nullptr, QCoreApplication::applicationName(),
                QCoreApplication::translate("main", "Invalid number of seconds: %1")
                    .arg(commandLineParser.value(secondsOption)));
            return 2;
        }
        jiggler.setPeriodSeconds(seconds);
    }

    if (commandLineParser.isSet(distanceOption)) {
        bool conversionSucceeded = false;
        const int distance = commandLineParser.value(distanceOption).toInt(&conversionSucceeded);
        if (!conversionSucceeded) {
            QMessageBox::critical(nullptr, QCoreApplication::applicationName(),
                QCoreApplication::translate("main", "Invalid distance multiplier: %1")
                    .arg(commandLineParser.value(distanceOption)));
            return 2;
        }
        jiggler.setDistanceMultiplier(distance);
    }

    if (commandLineParser.isSet(randomOption))
        jiggler.setRandomInterval(true);

    const bool jiggleOn = commandLineParser.isSet(jiggleOption);
    const bool jiggleOff = commandLineParser.isSet(noJiggleOption);
    if (jiggleOn || jiggleOff) {
        jiggler.setJiggling(jiggleOn);
    }

    const bool showWindow = commandLineParser.isSet(windowOption)
        || !QSystemTrayIcon::isSystemTrayAvailable()
        || (!commandLineParser.isSet(minimizedOption) && jiggler.showWindowOnStart());
    mainWindow.setVisible(showWindow);
    if (!showWindow)
        mainWindow.showStartedMessage();

    jiggler.start();

    return application.exec();
}
