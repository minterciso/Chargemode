#include <QtTest>

#include "runasync.h"

// All cases in this file are suggested by Claude and need approval before they count as oracle.
class TstRunAsync : public QObject {
    Q_OBJECT

    struct Result {
        int calls = 0;
        bool ok = false;
        QString text;
    };

    // Waits past the call so a second invocation of done would be counted.
    static Result run(const QString &prog, const QStringList &args, int timeoutMs = 5000) {
        Result r;
        runAsync(prog, args, [&r](bool ok, QString text) {
            ++r.calls;
            r.ok = ok;
            r.text = std::move(text);
        }, timeoutMs);
        QElapsedTimer guard;
        guard.start();
        while (r.calls == 0 && guard.elapsed() < 10000) {
            QTest::qWait(10);
        }
        QTest::qWait(100);
        return r;
    }

private slots:
    void successReturnsStdout() {
        const Result r = run("/bin/sh", {"-c", "echo hello"});
        QCOMPARE(r.calls, 1);
        QVERIFY(r.ok);
        QCOMPARE(r.text, QString("hello"));
    }

    void failureReturnsStderrNotStdout() {
        const Result r = run("/bin/sh", {"-c", "echo out; echo err >&2; exit 3"});
        QCOMPARE(r.calls, 1);
        QVERIFY(!r.ok);
        QCOMPARE(r.text, QString("err"));
    }

    void failureWithEmptyStderrReportsExitCode() {
        const Result r = run("/bin/sh", {"-c", "exit 7"});
        QCOMPARE(r.calls, 1);
        QVERIFY(!r.ok);
        QCOMPARE(r.text, QString("exit code 7"));
    }

    void missingBinaryReportsStartFailure() {
        const Result r = run("/nonexistent/chargetray-test-binary", {});
        QCOMPARE(r.calls, 1);
        QVERIFY(!r.ok);
        QVERIFY(!r.text.isEmpty());
    }

    void timeoutKillsProcessAndReportsIt() {
        QElapsedTimer t;
        t.start();
        const Result r = run("/bin/sleep", {"30"}, 200);
        QCOMPARE(r.calls, 1);
        QVERIFY(!r.ok);
        QCOMPARE(r.text, QString("timed out or crashed"));
        QVERIFY(t.elapsed() < 5000);
    }

    void fastProcessIsNotKilledByTimer() {
        const Result r = run("/bin/sh", {"-c", "echo done"}, 5000);
        QVERIFY(r.ok);
        QCOMPARE(r.text, QString("done"));
    }
};

QTEST_GUILESS_MAIN(TstRunAsync)
#include "tst_runasync.moc"
