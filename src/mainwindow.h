#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLabel;
class QMenu;
class QSpinBox;

// Qt Widgets frontend. Knows nothing about the jiggling logic: it emits
// value-only signals for user actions, and receives state updates through
// its public slots. The integration point (app.cpp) wires it to the logic.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Shows the tray notification that the application has started.
    void showStartedMessage();

signals:
    // The user changed something in the interface. Values only.
    void jigglingToggled(bool jiggling);
    void modeSelected(int mode);
    void periodChanged(int seconds);
    void distanceChanged(int multiplier);
    void randomIntervalToggled(bool random);
    void showWindowOnStartToggled(bool show);

public slots:
    // State pushed from the logic back into the interface.
    void setJiggling(bool jiggling);
    void setModeSelected(int mode);
    void setPeriodSeconds(int seconds);
    void setDistanceMultiplier(int multiplier);
    void setRandomInterval(bool random);
    void setShowWindowOnStart(bool show);
    void setNextInterval(int seconds);
    // Outcome of the last tick: true when the jiggle was skipped because
    // the user was moving the mouse.
    void setUserActivity(bool active);

    void onGlobalShortcutActivated();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void countdownTick();
    void trayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUi();
    void updateTrayToolTip();
    void updateNextIntervalLabel();

    QCheckBox* m_jigglingCheckBox = nullptr;
    QLabel* m_nextIntervalLabel = nullptr;
    QComboBox* m_modeComboBox = nullptr;
    QSpinBox* m_periodSpinBox = nullptr;
    QSpinBox* m_distanceSpinBox = nullptr;
    QCheckBox* m_randomIntervalCheckBox = nullptr;
    QCheckBox* m_showWindowOnStartCheckBox = nullptr;

    QSystemTrayIcon* m_trayIcon = nullptr;
    QMenu* m_trayMenu = nullptr;
    QAction* m_trayJigglingAction = nullptr;

    // Own one-second timer for the countdown display; approximate, and
    // re-synchronized every time the logic reports the next interval.
    QTimer m_countdownTimer;
    int m_secondsToNextJiggle = 0;

    // Whether the last tick was skipped due to user activity; cleared when
    // jiggling stops, so a later restart does not show a stale message.
    bool m_userActivity = false;
};

#endif // MAINWINDOW_H
