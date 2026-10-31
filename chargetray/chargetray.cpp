#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QProcess>
#include <QSystemTrayIcon>
#include <QTimer>

#include <functional>

#include "runasync.h"

namespace {

const QString kPowerSupply = "/sys/class/power_supply";
const QString kChargeMode = "/usr/local/bin/chargemode";
const QString kPpdBus = "org.freedesktop.UPower.PowerProfiles";
const QString kPpdPath = "/org/freedesktop/UPower/PowerProfiles";

QString readFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(f.readAll()).trimmed();
}

// "[Fast] Standard Long_Life" -> "Fast"
QString activeChargeType() {
    const QDir dir(kPowerSupply);
    for (const QString &name : dir.entryList({"BAT*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString raw = readFile(dir.filePath(name + "/charge_types"));
        const int a = raw.indexOf('[');
        const int b = raw.indexOf(']');
        if (a >= 0 && b > a) {
            return raw.mid(a + 1, b - a - 1);
        }
    }
    return {};
}

bool onAc() {
    const QDir dir(kPowerSupply);
    for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (readFile(dir.filePath(name + "/type")) == "Battery") {
            continue;
        }
        if (readFile(dir.filePath(name + "/online")) == "1") {
            return true;
        }
    }
    return false;
}

void getProfile(std::function<void(QString)> done) {
    runAsync("busctl", {"get-property", kPpdBus, kPpdPath, kPpdBus, "ActiveProfile"},
             [done](bool ok, const QString &out) {
                 done(ok ? out.section('"', 1, 1) : QString());  // s "balanced" -> balanced
             });
}

void setProfile(const QString &p, std::function<void(bool, QString)> done) {
    runAsync("busctl", {"set-property", kPpdBus, kPpdPath, kPpdBus, "ActiveProfile", "s", p}, std::move(done));
}

QIcon iconFor(const QString &type) {
    const char *name = "battery";
    if (type == "Long_Life") {
        name = "applications-games";
    } else if (type == "Fast") {
        name = "battery-full-charging";
    }
    return QIcon::fromTheme(name, QIcon::fromTheme("battery"));
}

}  // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("chargetray");
    QApplication::setApplicationVersion(CHARGETRAY_VERSION);
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    QSystemTrayIcon tray;
    QMenu menu;
    auto *group = new QActionGroup(&menu);
    QAction *gaming = menu.addAction("Gaming (performance + Long_Life)");
    QAction *fast = menu.addAction("Fast charge");
    QAction *slow = menu.addAction("Slow charge (Standard)");
    for (QAction *a : {gaming, fast, slow}) {
        a->setCheckable(true);
        group->addAction(a);
    }
    menu.addSeparator();
    QAction *quit = menu.addAction("Quit");

    auto refresh = [&] {
        const QString type = activeChargeType();
        const bool ac = onAc();
        gaming->setChecked(type == "Long_Life");
        fast->setChecked(type == "Fast");
        slow->setChecked(type == "Standard");
        tray.setIcon(iconFor(type));
        getProfile([&tray, type, ac](const QString &profile) {
            tray.setToolTip(QString("Charge: %1 | Profile: %2 | %3").arg(type, profile, ac ? "AC" : "battery"));
        });
    };

    auto finish = [&](const QString &charge, const QStringList &notes) {
        QString msg = charge;
        if (!notes.isEmpty()) {
            msg += "\n" + notes.join("\n");
        }
        tray.showMessage("Charge mode", msg, notes.isEmpty() ? QSystemTrayIcon::Information : QSystemTrayIcon::Warning);
        refresh();
    };

    auto apply = [&](const QString &charge, const QString &profile, bool needsAc) {
        runAsync("sudo", {"-n", kChargeMode, charge}, [&, charge, profile, needsAc](bool ok, const QString &err) {
            if (!ok) {
                tray.showMessage("Charge mode", "chargemode failed: " + err, QSystemTrayIcon::Warning);
                refresh();
                return;
            }
            if (needsAc && !onAc()) {
                finish(charge, {"no AC: performance profile skipped"});
                return;
            }
            setProfile(profile, [&, charge, profile](bool pok, const QString &perr) {
                finish(charge, pok ? QStringList{} : QStringList{"profile '" + profile + "' failed: " + perr});
            });
        });
    };

    QObject::connect(gaming, &QAction::triggered, [&] { apply("gaming", "performance", true); });
    QObject::connect(fast, &QAction::triggered, [&] { apply("fast", "balanced", false); });
    QObject::connect(slow, &QAction::triggered, [&] { apply("standard", "balanced", false); });
    QObject::connect(quit, &QAction::triggered, &app, &QApplication::quit);

    QTimer timer;
    timer.setInterval(10000);
    QObject::connect(&timer, &QTimer::timeout, refresh);
    timer.start();

    tray.setContextMenu(&menu);
    refresh();
    tray.show();
    return app.exec();
}
