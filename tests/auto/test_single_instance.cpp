// Process level tests for the single instance behavior: a running instance
// owns the lock file, a second instance takes over by terminating the first
// one, and a lock left behind by a killed instance is removed as stale.
//
// The tests spawn the real application binary and observe it from the
// outside: which process owns the lock file, which processes are alive, and
// which values were persisted. No application code is linked into the test.

#include <QtTest>

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTemporaryDir>

#include <functional>

// Must match the lock file name assembled in app.cpp.
static const QString lockFileName = QStringLiteral("mousejiggler-single-instance.lock");

class TestSingleInstance : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

private slots:
    void initTestCase();
    void lockHoldsProcessId();
    void secondInstanceTakesOver();
    void staleLockIsRemoved();

private:
    static bool waitFor(const std::function<bool()>& condition, int timeoutMilliseconds);
    QString lockPath() const;
    QString lockOwnerProcessId() const;
    void startApplication(QProcess& process, const QStringList& arguments) const;
    void resetLockAndSettings();

    QTemporaryDir m_isolationDirectory;
    QSettings* m_settings = nullptr;
};

void TestSingleInstance::initTestCase()
{
    QVERIFY2(m_isolationDirectory.isValid(), "Could not create the isolation directory");
    QVERIFY(QDir(m_isolationDirectory.path()).mkpath(QStringLiteral("config")));
    QVERIFY(QDir(m_isolationDirectory.path()).mkpath(QStringLiteral("tmp")));

    // Redirect everything the application persists into the isolation
    // directory: XDG_CONFIG_HOME moves the settings file on Unix. The
    // application process gets the same variables plus TMPDIR, TEMP and TMP
    // for the lock file (the Unix and Windows environment variables of
    // QStandardPaths::TempLocation).
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_isolationDirectory.filePath(QStringLiteral("config"))));

    // Must match the organization and application names set in app.cpp: on
    // Unix they select the settings file the application writes, on Windows
    // the registry keys.
    m_settings = new QSettings(QStringLiteral("Alejandro Exojo"), QStringLiteral("Qt Mouse Jiggler"), this);
}

bool TestSingleInstance::waitFor(const std::function<bool()>& condition, int timeoutMilliseconds)
{
    // Poll with a deadline instead of sleeping for a fixed time: the loop
    // returns as soon as the condition holds, which keeps passing runs fast
    // and only failing runs slow.
    const QDeadlineTimer deadline(timeoutMilliseconds);
    while (!deadline.hasExpired()) {
        if (condition())
            return true;
        QTest::qWait(20);
    }
    return condition();
}

QString TestSingleInstance::lockPath() const
{
    return m_isolationDirectory.filePath(QStringLiteral("tmp") + QLatin1Char('/') + lockFileName);
}

QString TestSingleInstance::lockOwnerProcessId() const
{
    // The first line of the lock file is the process identifier of its
    // owner; an empty result means "no lock" (missing or unreadable).
    QFile lockFile(lockPath());
    if (!lockFile.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(lockFile.readLine()).simplified();
}

void TestSingleInstance::startApplication(QProcess& process, const QStringList& arguments) const
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("XDG_CONFIG_HOME"), m_isolationDirectory.filePath(QStringLiteral("config")));
    environment.insert(QStringLiteral("TMPDIR"), m_isolationDirectory.filePath(QStringLiteral("tmp")));
    environment.insert(QStringLiteral("TEMP"), m_isolationDirectory.filePath(QStringLiteral("tmp")));
    environment.insert(QStringLiteral("TMP"), m_isolationDirectory.filePath(QStringLiteral("tmp")));
    process.setProcessEnvironment(environment);
    // Keep the application output away from the test output: jiggling
    // instances write a debug trace on every tick.
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(QStringLiteral(APPLICATION_BINARY_PATH), arguments);
}

void TestSingleInstance::resetLockAndSettings()
{
    // Start each case from a clean slate: remove the lock file and the
    // settings persisted by whatever ran before. All cases share the
    // isolation directory because QSettings resolves its path only once per
    // process.
    if (QFile::exists(lockPath()))
        QVERIFY(QFile::remove(lockPath()));
    m_settings->clear();
    m_settings->sync();
}

void TestSingleInstance::lockHoldsProcessId()
{
    resetLockAndSettings();

    QProcess application;
    startApplication(application, {});
    QVERIFY(application.waitForStarted());

    const QString processId = QString::number(application.processId());
    QVERIFY(waitFor([this, &processId] { return lockOwnerProcessId() == processId; }, 10000));
    QCOMPARE(application.state(), QProcess::Running);

    application.kill();
    QVERIFY(application.waitForFinished(10000));
}

void TestSingleInstance::secondInstanceTakesOver()
{
    resetLockAndSettings();

    QProcess first;
    startApplication(first, {});
    QVERIFY(first.waitForStarted());
    const QString firstProcessId = QString::number(first.processId());
    QVERIFY(waitFor([this, &firstProcessId] { return lockOwnerProcessId() == firstProcessId; }, 10000));

    // The second instance terminates the first one, takes over the lock,
    // and persists the values of its own command line.
    QProcess second;
    startApplication(second, { QStringLiteral("--no-jiggle") });
    QVERIFY(second.waitForStarted());
    const QString secondProcessId = QString::number(second.processId());

    QVERIFY(first.waitForFinished(10000));
    QVERIFY(waitFor([this, &secondProcessId] { return lockOwnerProcessId() == secondProcessId; }, 10000));
    QCOMPARE(second.state(), QProcess::Running);
    QVERIFY(waitFor([this] {
        m_settings->sync();
        return m_settings->contains(QStringLiteral("jiggle"))
            && m_settings->value(QStringLiteral("jiggle")).toBool() == false;
    }, 10000));

    second.kill();
    QVERIFY(second.waitForFinished(10000));
}

void TestSingleInstance::staleLockIsRemoved()
{
    resetLockAndSettings();

    // Simulate a crash: hard kill a running instance, which leaves its lock
    // file behind with no chance to clean up. The next start must treat the
    // leftover lock as stale, remove it, run, and persist the values of its
    // own command line.
    QProcess killed;
    startApplication(killed, {});
    QVERIFY(killed.waitForStarted());
    const QString killedProcessId = QString::number(killed.processId());
    QVERIFY(waitFor([this, &killedProcessId] { return lockOwnerProcessId() == killedProcessId; }, 10000));
    killed.kill();
    QVERIFY(killed.waitForFinished(10000));
    QVERIFY(QFile::exists(lockPath()));
    QProcess application;
    startApplication(application, { QStringLiteral("-s"), QStringLiteral("55") });
    QVERIFY(application.waitForStarted());
    const QString processId = QString::number(application.processId());
    QVERIFY(waitFor([this, &processId] { return lockOwnerProcessId() == processId; }, 10000));
    QCOMPARE(application.state(), QProcess::Running);
    QVERIFY(waitFor([this] {
        m_settings->sync();
        return m_settings->contains(QStringLiteral("periodSeconds"))
            && m_settings->value(QStringLiteral("periodSeconds")).toInt() == 55;
    }, 10000));

    application.kill();
    QVERIFY(application.waitForFinished(10000));
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "test_single_instance.moc"
