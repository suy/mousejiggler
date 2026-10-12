#include "mainwindow.h"
#include "constants.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

namespace {

// Keep tray icon tooltips compact as there are allegedly some Windows' limits
// for them. QFontMetrics::elidedText() takes pixels, so use that.
const int MaximumToolTipWidth = 500;

// The combo box lists the modes in declaration order of the logic's mode
// enumeration, so the current index maps directly to the enum value.
QStringList modeDisplayNames()
{
    return { MainWindow::tr("Linear"), MainWindow::tr("Diagonal"), MainWindow::tr("Circle"),
        MainWindow::tr("Invisible") };
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("Qt Mouse Jiggler"));

    auto jigglingGroupBox = new QGroupBox(tr("Jiggling"), this);
    m_jigglingCheckBox = new QCheckBox(tr("Jiggling the mouse"), jigglingGroupBox);
    m_nextIntervalLabel = new QLabel(tr("Not jiggling."), jigglingGroupBox);
    auto jigglingLayout = new QVBoxLayout(jigglingGroupBox);
    jigglingLayout->addWidget(m_jigglingCheckBox);
    jigglingLayout->addWidget(m_nextIntervalLabel);

    auto settingsGroupBox = new QGroupBox(tr("Settings"), this);
    m_modeComboBox = new QComboBox(settingsGroupBox);
    m_modeComboBox->addItems(modeDisplayNames());
    m_periodSpinBox = new QSpinBox(settingsGroupBox);
    m_periodSpinBox->setRange(constants::MinimumPeriodSeconds, constants::MaximumPeriodSeconds);
    const auto updatePeriodSuffix = [this](int seconds) {
        m_periodSpinBox->setSuffix(tr(" second(s)", "period unit suffix", seconds));
    };
    updatePeriodSuffix(m_periodSpinBox->value());
    m_distanceSpinBox = new QSpinBox(settingsGroupBox);
    m_distanceSpinBox->setRange(constants::MinimumDistanceMultiplier,
        constants::MaximumDistanceMultiplier);
    m_randomIntervalCheckBox = new QCheckBox(tr("Randomize interval"), settingsGroupBox);
    m_showWindowOnStartCheckBox = new QCheckBox(tr("Show window on start"), settingsGroupBox);
    auto settingsLayout = new QFormLayout(settingsGroupBox);
    settingsLayout->addRow(tr("Mode:"), m_modeComboBox);
    settingsLayout->addRow(tr("Period:"), m_periodSpinBox);
    settingsLayout->addRow(tr("Distance:"), m_distanceSpinBox);
    settingsLayout->addRow(m_randomIntervalCheckBox);
    settingsLayout->addRow(m_showWindowOnStartCheckBox);

    auto layout = new QVBoxLayout;
    layout->addWidget(jigglingGroupBox);
    layout->addWidget(settingsGroupBox);
    auto centralWidget = new QWidget(this);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    m_trayMenu = new QMenu(this);
    m_trayJigglingAction = m_trayMenu->addAction(tr("Jiggle"));
    m_trayJigglingAction->setCheckable(true);
    connect(m_trayJigglingAction, &QAction::toggled, this, [this](bool jiggling) {
        m_jigglingCheckBox->setChecked(jiggling);
    });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Open"), this, &QWidget::showNormal);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Quit"), QCoreApplication::instance(), &QCoreApplication::quit);

    m_trayIcon = new QSystemTrayIcon(QIcon(QStringLiteral(":/mousejiggler.ico")), this);
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayActivated);
    connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, &QWidget::showNormal);
    m_trayIcon->show();

    // Widget changes leave as plain values; the integration point forwards
    // them to the logic.
    connect(m_jigglingCheckBox, &QCheckBox::toggled, this, &MainWindow::jigglingToggled);
    connect(m_modeComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this,
        &MainWindow::modeSelected);
    connect(m_periodSpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
        &MainWindow::periodChanged);
    connect(m_periodSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, updatePeriodSuffix);
    connect(m_distanceSpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
        &MainWindow::distanceChanged);
    connect(m_randomIntervalCheckBox, &QCheckBox::toggled, this, &MainWindow::randomIntervalToggled);
    connect(m_showWindowOnStartCheckBox, &QCheckBox::toggled, this,
        &MainWindow::showWindowOnStartToggled);

    // Keyboard shortcuts, all hardcoded. The standard keys are
    // platform-themed: Windows binds QKeySequence::Quit to nothing at all
    // and QKeySequence::Close to Ctrl+F4, so neither of those can be used
    // here. Qt::CTRL maps to the Command key on macOS, so these hardcoded
    // sequences keep their native meaning there too. Ctrl+T drives the
    // jiggling checkbox, so it takes the same path as a click. Ctrl+W goes
    // through closeEvent, so it hides to the tray like any other close, and
    // quits when there is no system tray. Ctrl+Q quits; closing only hides,
    // so quitting needs its own shortcut.
    m_jigglingCheckBox->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    auto closeAction = new QAction(this);
    closeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_W));
    connect(closeAction, &QAction::triggered, this, &QWidget::close);
    addAction(closeAction);
    auto quitAction = new QAction(this);
    quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(quitAction, &QAction::triggered, QCoreApplication::instance(), &QCoreApplication::quit);
    addAction(quitAction);

    // The countdown is a display approximation driven by an own timer,
    // independent from the logic's jiggle timer.
    m_countdownTimer.setInterval(1000);
    connect(&m_countdownTimer, &QTimer::timeout, this, &MainWindow::countdownTick);
}

void MainWindow::setJiggling(bool jiggling)
{
    m_jigglingCheckBox->setChecked(jiggling);
    m_trayJigglingAction->setChecked(jiggling);
    if (!jiggling) {
        m_countdownTimer.stop();
        m_userActivity = false;
    }
    updateNextIntervalLabel();
    updateTrayToolTip();
}

void MainWindow::setModeSelected(int mode)
{
    m_modeComboBox->setCurrentIndex(mode);
    updateTrayToolTip();
}

void MainWindow::setPeriodSeconds(int seconds)
{
    m_periodSpinBox->setValue(seconds);
    updateTrayToolTip();
}

void MainWindow::setDistanceMultiplier(int multiplier)
{
    m_distanceSpinBox->setValue(multiplier);
    updateTrayToolTip();
}

void MainWindow::setRandomInterval(bool random)
{
    m_randomIntervalCheckBox->setChecked(random);
    updateTrayToolTip();
}

void MainWindow::setShowWindowOnStart(bool show)
{
    m_showWindowOnStartCheckBox->setChecked(show);
    updateTrayToolTip();
}

void MainWindow::setNextInterval(int seconds)
{
    m_secondsToNextJiggle = seconds;
    updateNextIntervalLabel();
    if (m_jigglingCheckBox->isChecked())
        m_countdownTimer.start();
}

void MainWindow::setUserActivity(bool active)
{
    m_userActivity = active;
    updateNextIntervalLabel();
}

void MainWindow::showStartedMessage()
{
    const QString text = m_jigglingCheckBox->isChecked()
        ? tr("Qt Mouse Jiggler started (jiggling enabled).")
        : tr("Qt Mouse Jiggler started (jiggling disabled).");
    m_trayIcon->showMessage(QCoreApplication::applicationName(), text);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Closing the window hides it back to the tray; quitting happens through
    // the tray menu. Without a system tray, closing quits as usual.
    if (m_trayIcon->isVisible() && QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::countdownTick()
{
    if (m_secondsToNextJiggle > 0) {
        --m_secondsToNextJiggle;
        updateNextIntervalLabel();
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        setVisible(true);
    } else if (reason == QSystemTrayIcon::Trigger) {
        setVisible(!isVisible());
    }
}

void MainWindow::updateTrayToolTip()
{
    QString text;
    if (m_jigglingCheckBox->isChecked()) {
        const int period = m_periodSpinBox->value();
        if (m_randomIntervalCheckBox->isChecked()) {
            text = tr("Jiggling between 1 and %n second(s). Mode: %1", "", period)
                          .arg(m_modeComboBox->currentText());
        } else {
            text = tr("Jiggling every %n second(s). Mode: %1", "", period)
                          .arg(m_modeComboBox->currentText());
        }
    } else {
        text = tr("Not jiggling the mouse.");
    }

    text = QFontMetricsF(QApplication::font()).elidedText(text, Qt::ElideRight,
        MaximumToolTipWidth);

    m_trayIcon->setToolTip(text);
}

void MainWindow::updateNextIntervalLabel()
{
    if (!m_jigglingCheckBox->isChecked()) {
        m_nextIntervalLabel->setText(tr("Not jiggling."));
        return;
    }

    const QString text = m_userActivity
        ? tr("Cursor moved. Next jiggle attempt in approximately %n second(s).", "",
              m_secondsToNextJiggle)
        : tr("Next jiggle in approximately %n second(s).", "", m_secondsToNextJiggle);

    m_nextIntervalLabel->setText(text);
}
