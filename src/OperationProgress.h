#pragma once
#include <Task.h>
#include <QPointer>
#include <QTimer>
#include <atomic>
#include <memory>
#include <mutex>
#include "ProgressState.h"

// Worker callbacks update atomics, not widgets. One GUI timer publishes bounded
// updates through Task::setProgress; 100% is reserved for actual view publication.
class OperationProgress {
public:
    using State = std::shared_ptr<OperationProgressState>;
    ~OperationProgress() { if (_task) delete _task; }
    State begin(QObject* parent, const QString& name) {
        if (!_task) {
            _task = new mv::Task(parent, name, {mv::Task::GuiScope::Foreground});
            _task->setProgressMode(mv::Task::ProgressMode::Manual);
            _timer = new QTimer(_task);
            _timer->setInterval(100);
            QObject::connect(_timer, &QTimer::timeout, _task, [this] { publish(); });
        }
        _state = std::make_shared<OperationProgressState>();
        _task->setName(name); _task->setProgress(0, "Queued"); _task->setRunning(); _timer->start();
        return _state;
    }
    State state() const { return _state; }
    bool running() const { return _timer && _timer->isActive(); }
    void finish() {
        if (!_task) return;
        _timer->stop(); _task->setProgress(1, "Ready"); _task->setFinished();
    }
    void cancel() {
        if (!_task || !running()) return;
        _timer->stop(); _task->setStatus(mv::Task::Status::Aborted);
    }
private:
    void publish() {
        if (!_task || !_state) return;
        QString text;
        { const std::lock_guard<std::mutex> lock(_state->mutex); text = _state->stage; }
        const auto total = _state->total.load();
        if (total) text += QString(" (%1 / %2)").arg(_state->done.load()).arg(total);
        _task->setProgress(_state->value.load(), text);
    }
    QPointer<mv::Task> _task;
    QPointer<QTimer> _timer;
    State _state;
};
