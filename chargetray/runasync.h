#ifndef RUNASYNC_H
#define RUNASYNC_H

#include <QString>
#include <QStringList>

#include <functional>

// Runs prog without blocking the event loop and calls done exactly once, from the event loop.
// ok is true only for a normal exit with code 0. text is trimmed stdout on success; on failure it is
// trimmed stderr, or a short reason when stderr is empty. timeoutMs is milliseconds until the process is killed.
void runAsync(const QString &prog, const QStringList &args, std::function<void(bool, QString)> done, int timeoutMs = 5000);

#endif
