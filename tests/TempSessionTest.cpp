#include "../src/EvoViewerTemp.h"
#include <QCoreApplication>
#include <QProcess>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--hold"))) {
        const auto dir = EvoViewerTemp::directory();
        QFile marker(dir.filePath(QStringLiteral("cache-marker")));
        if (!marker.open(QIODevice::WriteOnly)) return 2;
        marker.write("owned cache"); marker.close();
        std::cout << dir.absolutePath().toStdString() << std::endl;
        std::cin.get(); // parent either releases us normally or terminates us
        return 0;
    }
    const auto own = EvoViewerTemp::directory();
    require(QFileInfo(own.absolutePath()).dir().dirName() == QStringLiteral("EvoViewer"), "Session must stay below the dedicated EvoViewer folder");
    require(own.dirName().startsWith(QStringLiteral("GeneDetect-%1-").arg(QCoreApplication::applicationPid())), "Session key must include the actual PID");
    QLockFile duplicate(own.filePath(QStringLiteral("session.lock")));
    duplicate.setStaleLockTime(0);
    require(!duplicate.tryLock(0), "Running session must hold an exclusive lock");
    for (const bool crash : {false, true}) {
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--hold")});
        require(child.waitForStarted(5000) && child.waitForReadyRead(5000), "Child session must start");
        const auto path = QString::fromUtf8(child.readLine()).trimmed();
        require(!path.isEmpty() && QDir(path).exists(), "Child session must publish its own path");
        EvoViewerTemp::cleanupStaleSessions();
        require(QFileInfo::exists(QDir(path).filePath(QStringLiteral("cache-marker"))), "Launch cleanup must preserve another live process's cache");
        if (crash) child.kill(); else { child.write("\n"); child.closeWriteChannel(); }
        require(child.waitForFinished(5000), "Child session must finish");
        if (crash) {
            require(QDir(path).exists(), "Forced exit should leave a stale session to recover");
            EvoViewerTemp::cleanupStaleSessions();
        }
        require(!QDir(path).exists(), "Normal shutdown and next-launch crash recovery must remove owned session files and directory");
    }
    require(QDir(own.absolutePath()).exists(), "Concurrent cleanup must preserve our live session");
    std::cout << "PASS: keyed PID folders, exclusive live locks, concurrent launch safety, clean shutdown and crash recovery\n";
}
