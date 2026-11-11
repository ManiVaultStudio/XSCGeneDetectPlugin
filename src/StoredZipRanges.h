#pragma once
#include "XSCDebug.h"
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <QFileInfo>
#include <QHash>
#include <QFile>
#include <optional>
#include <memory>
#include <mutex>
#include <limits>
#include <stdexcept>

// Plugin-local fast path for ZIP_STORED entries containing already-compressed
// packed tiles. Each worker reads only its frame directly from the project file;
// no staging/extraction files and no archive-wide lock during the actual reads.
class StoredZipRanges {
    struct Range { quint64 offset = 0, size = 0; };
    using Index = QHash<QString, Range>;
public:
    static std::optional<QByteArray> read(const QString& archive, const QString& entry, quint64 offset, quint64 size) {
        XSC_TRACE("packed.zip.range-read");
        const QFileInfo info(archive);
        if (!info.isFile()) return {};
        const QString key = info.absoluteFilePath() + QChar(0x1f) + QString::number(info.size())
            + QChar(0x1f) + QString::number(info.lastModified().toMSecsSinceEpoch());
        static std::mutex mutex;
        static QHash<QString, std::shared_ptr<const Index>> indices;
        std::shared_ptr<const Index> index;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            index = indices.value(key);
            if (!index) {
                auto built = std::make_shared<Index>();
                QuaZip zip(archive);
                if (!zip.open(QuaZip::mdUnzip)) throw std::runtime_error("Cannot index project archive");
                if (zip.goToFirstFile()) do {
                    QuaZipFileInfo64 entryInfo;
                    if (!zip.getCurrentFileInfo(&entryInfo)) throw std::runtime_error("Cannot inspect project archive entry");
                    if (entryInfo.method != 0 || (entryInfo.flags & 1) || entryInfo.compressedSize != entryInfo.uncompressedSize) continue;
                    QuaZipFile file(&zip);
                    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot open stored project entry");
                    const auto start = unzGetCurrentFileZStreamPos64(zip.getUnzFile());
                    if (start > static_cast<quint64>(info.size()) || entryInfo.uncompressedSize > static_cast<quint64>(info.size()) - start)
                        throw std::runtime_error("Stored archive entry exceeds project size");
                    built->insert(zip.getCurrentFileName(), {start, entryInfo.uncompressedSize});
                    file.close();
                } while (zip.goToNextFile());
                zip.close();
                if (indices.size() >= 4) indices.erase(indices.begin());
                indices.insert(key, built); index = built;
            }
        }
        const auto found = index->constFind(entry);
        if (found == index->cend()) return {}; // compressed/encrypted ZIP entries use the existing public codec fallback
        if (offset > found->size || size > found->size - offset || size > static_cast<quint64>(std::numeric_limits<qsizetype>::max()))
            throw std::out_of_range("Packed frame exceeds archive entry");
        QFile file(archive);
        if (!file.open(QIODevice::ReadOnly) || !file.seek(static_cast<qint64>(found->offset + offset)))
            throw std::runtime_error("Cannot seek packed project frame");
        auto bytes = file.read(static_cast<qint64>(size));
        if (static_cast<quint64>(bytes.size()) != size) throw std::runtime_error("Incomplete packed project frame");
        return bytes;
    }
};
