#pragma once
#include <QDir>
#include <QString>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QUuid>
#include <QRegularExpression>
#include <memory>
#if defined(XSC_USE_MANIVAULT_TEMP)
#include <Application.h>
#endif

namespace EvoViewerTemp {
inline QDir rootDirectory() {
#if defined(XSC_USE_MANIVAULT_TEMP)
    // Capture ManiVault's configured root while the application is alive.
    // Destruction must not consult an already-destroyed application object.
    static const QString base = mv::Application::current()->getTemporaryDirRoot();
#else
    static const QString base = QDir::tempPath(); // isolated Qt-only regression executables
#endif
    const auto path = QDir(base).filePath(QStringLiteral("EvoViewer"));
    QDir().mkpath(path);
    return QDir(path);
}
inline void removeSessionFiles(const QDir& dir) {
    // Only files in this plugin's own keyed directory; never recurse through
    // directories/junctions and never remove a live process's lock.
    for (const auto& name : dir.entryList(QDir::Files | QDir::Hidden | QDir::NoSymLinks))
        if (name != QStringLiteral("session.lock")) QFile::remove(dir.filePath(name));
}
inline void cleanupUnlocked(const QDir& root) {
    const QRegularExpression ownedKey(QStringLiteral("^GeneDetect-[0-9]+-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    for (const auto& name : root.entryList({QStringLiteral("GeneDetect-*")}, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
        if (!ownedKey.match(name).hasMatch()) continue;
        const QDir dir(root.filePath(name));
        QLockFile lock(dir.filePath(QStringLiteral("session.lock")));
        lock.setStaleLockTime(0); // no age-based eviction of long-running processes
        if (!lock.tryLock(0)) continue; // Qt checks PID/host/process identity for stale locks
        removeSessionFiles(dir);
        lock.unlock();
        root.rmdir(name); // only if empty; no recursive deletion after unlocking
    }
}
inline void cleanupStaleSessions() {
    const auto root = rootDirectory();
    QLockFile registry(root.filePath(QStringLiteral("GeneDetect-registry.lock")));
    registry.setStaleLockTime(0);
    if (registry.tryLock(5000)) cleanupUnlocked(root);
}
class Session {
public:
    Session() {
        const auto root = rootDirectory();
        QLockFile registry(root.filePath(QStringLiteral("GeneDetect-registry.lock")));
        registry.setStaleLockTime(0);
        if (!registry.tryLock(5000)) {
            _directory = QDir(root.filePath(QStringLiteral("unavailable-%1").arg(QCoreApplication::applicationPid())));
            return;
        }
        cleanupUnlocked(root);
        const auto key = QStringLiteral("GeneDetect-%1-%2").arg(QCoreApplication::applicationPid())
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        _directory = QDir(root.filePath(key));
        if (!root.mkpath(key)) return;
        _lock = std::make_unique<QLockFile>(_directory.filePath(QStringLiteral("session.lock")));
        _lock->setStaleLockTime(0);
        _locked = _lock->tryLock(0);
        if (!_locked) {
            root.rmdir(key);
            // A nonexistent location makes optional files fail to open safely;
            // never fall back to loose, unlocked files in the temp root.
            _directory = QDir(root.filePath(QStringLiteral("unavailable-%1").arg(key)));
        }
    }
    ~Session() {
        if (!_locked) return;
        removeSessionFiles(_directory);
        _lock->unlock();
        rootDirectory().rmdir(_directory.dirName());
    }
    QDir directory() const { return _directory; }
private:
    QDir _directory;
    std::unique_ptr<QLockFile> _lock;
    bool _locked = false;
};
inline QDir directory() { static Session session; return session.directory(); }
inline QString fileTemplate(const QString& purpose) {
    return directory().filePath(purpose + QStringLiteral("-XXXXXX"));
}
}
