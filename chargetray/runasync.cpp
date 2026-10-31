#include "runasync.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTimer>

// done(ok, text): text is stdout on success, stderr (or a reason) on failure. Never blocks the GUI thread.
void runAsync(const QString &prog, const QStringList &args, std::function<void(bool, QString)> done, int timeoutMs) {
    auto *p = new QProcess(qApp);
    QObject::connect(p, &QProcess::finished, p, [p, done](int code, QProcess::ExitStatus st) {
        const bool ok = st == QProcess::NormalExit && code == 0;
        QString text = QString::fromUtf8(ok ? p->readAllStandardOutput() : p->readAllStandardError()).trimmed();
        if (!ok && text.isEmpty()) {
            text = st == QProcess::CrashExit ? "timed out or crashed" : QString("exit code %1").arg(code);
        }
        p->deleteLater();
        done(ok, text);
    });
    // FailedToStart never emits finished(), so it needs its own path.
    QObject::connect(p, &QProcess::errorOccurred, p, [p, done](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) {
            return;
        }
        const QString msg = p->errorString();
        p->deleteLater();
        done(false, msg);
    });
    QTimer::singleShot(timeoutMs, p, [p] {
        if (p->state() != QProcess::NotRunning) {
            p->kill();
        }
    });
    p->start(prog, args);
}
