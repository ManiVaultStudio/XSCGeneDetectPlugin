#pragma once
#include <cassert>
#include <atomic>
#include <memory>
#include <cstdint>

enum class TraitRequestScope { SelectedGene, ListedGenes, AllGenes };
inline TraitRequestScope traitRequestScope(bool windowOpen, bool allGenes) {
    return !windowOpen ? TraitRequestScope::SelectedGene : (allGenes ? TraitRequestScope::AllGenes : TraitRequestScope::ListedGenes);
}
inline bool includesTraitGene(TraitRequestScope scope, bool listed, bool selected) {
    return selected || scope == TraitRequestScope::AllGenes || (scope == TraitRequestScope::ListedGenes && listed);
}

// GUI-owned request identity. Row/gene equality alone cannot distinguish an
// old A result from a new A request after the user clicked A, B, then A again.
class RequestGeneration {
public:
    using Token = std::uint64_t;
    void advance() { ++_current; }
    Token current() const { return _current; }
    bool matches(Token token) const { return token == _current; }
private:
    Token _current = 0;
};

// GUI-thread state only. Worker cancellation belongs to the captured atomic
// token; finishing a worker does not make its result current or publishable.
class AnalysisWorkflow {
public:
    enum class Phase { Idle, Reading, Computing };
    bool active = false; // explicit demand: a gene request or an analysis window
    bool pending = false; // coalesced request; inputs are re-snapshotted on restart
    bool busy() const { return _phase != Phase::Idle; }
    Phase phase() const { return _phase; }
    void beginRead() { assert(!busy()); _phase = Phase::Reading; }
    void beginCompute() { assert(!busy()); _phase = Phase::Computing; }
    void finish() { _phase = Phase::Idle; }
    void cancel() { *_cancel = true; pending = false; }
    void resume() { if (*_cancel) _cancel = std::make_shared<std::atomic_bool>(false); }
    std::shared_ptr<std::atomic_bool> cancellation() const { return _cancel; }
    void setActive(bool value) {
        active = value;
        if (value) resume(); else cancel();
    }
private:
    Phase _phase = Phase::Idle;
    std::shared_ptr<std::atomic_bool> _cancel = std::make_shared<std::atomic_bool>(false);
};
