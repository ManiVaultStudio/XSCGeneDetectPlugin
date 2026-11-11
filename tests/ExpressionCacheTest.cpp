#include "../src/GeneExpressionCache.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <future>
#include <thread>
#include <iostream>

void require(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QString path;
    {
        GeneExpressionCache ramOnly(32, 0, true);
        ramOnly.put("data", "gene", std::make_shared<const std::vector<float>>(4, 7));
        require(ramOnly.fileName().isEmpty() && ramOnly.read("data", "gene", 4), "Completed-vector RAM cache must not create a second file beside the gene-block file");
    }
    {
        GeneExpressionCache cache(32); // deliberately tiny RAM budget: exercise disk and eviction
        path = cache.fileName();
        require(QFileInfo(path).absolutePath() == EvoViewerTemp::directory().absolutePath(), "Expression cache files must stay in the dedicated EvoViewer temp folder");
        auto old = std::make_shared<const std::vector<float>>(1024, 42.0f);
        cache.put("data", "old", old);

        std::promise<void> wroteChunk, resume;
        auto resumeFuture = resume.get_future();
        auto large = std::make_shared<const std::vector<float>>(2 * 1024 * 1024, 7.0f);
        std::exception_ptr workerError;
        std::thread writer([&] {
            try {
                int checks = 0;
                cache.put("data", "large", large, [&] {
                    if (++checks == 2) { wroteChunk.set_value(); resumeFuture.wait(); }
                    return false;
                });
            } catch (...) { workerError = std::current_exception(); }
        });
        wroteChunk.get_future().wait();
        const auto readable = cache.read("data", "old", 1024);
        const bool incompleteHidden = !cache.read("data", "large", large->size());
        resume.set_value(); writer.join();
        if (workerError) std::rethrow_exception(workerError);
        require(readable && *readable == *old, "Completed vectors must remain readable during another write");
        require(incompleteHidden, "A concurrent partial write became visible");
        require(*cache.read("data", "large", large->size()) == *large, "Disk readback corrupted a vector");
        bool cancelled = false;
        try { cache.put("data", "cancelled", large, [] { return true; }); }
        catch (const std::runtime_error&) { cancelled = true; }
        require(cancelled && !cache.read("data", "cancelled", large->size()), "Cancelled writes must not publish a result");
        cache.put("data", "cancelled", large);
        require(*cache.read("data", "cancelled", large->size()) == *large, "A cancelled write must be retryable");
        for (int i = 0; i < 10; ++i) {
            cache.put("data", QString::number(i), std::make_shared<const std::vector<float>>(4, float(i)));
            require(cache.ramBytes() <= 32, "RAM cache exceeded its budget");
        }
        require(cache.fileName() == path && QFileInfo::exists(path), "All vectors must use the same backing file");
        require(!cache.read("data", "old", 999), "Changed point counts must miss the cache");
    }
    require(!QFileInfo::exists(path), "Backing file must be cleaned up with its last owner");
    {
        GeneExpressionCache capped(32, 64);
        for (int i = 0; i < 10; ++i)
            capped.put("data", QString::number(i), std::make_shared<const std::vector<float>>(8, float(i)));
        require(QFileInfo(capped.fileName()).size() <= 64, "Disk cache exceeded its budget");
        require(capped.read("data", "9", 8) != nullptr, "Recent interactive values must remain available in RAM when disk is full");
    }
    std::cout << "PASS: one file, concurrent reads during writes, incomplete publication, RAM budget, cleanup\n";
}
