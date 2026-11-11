#include "../src/AnalysisWorkflow.h"
#include <iostream>
#include <stdexcept>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    const auto click = traitRequestScope(false, true);
    require(click == TraitRequestScope::SelectedGene && includesTraitGene(click, false, true) && !includesTraitGene(click, true, false),
        "A gene click must compute only that gene even if All genes was previously selected in the closed window");
    require(includesTraitGene(traitRequestScope(true, false), true, false) && !includesTraitGene(traitRequestScope(true, false), false, false),
        "An explicit listed-gene analysis must preserve its scope");
    require(includesTraitGene(traitRequestScope(true, true), false, false), "Explicit all-gene analysis must remain available");
    RequestGeneration clicks;
    clicks.advance(); const auto firstA = clicks.current();
    clicks.advance(); const auto b = clicks.current();
    clicks.advance(); const auto secondA = clicks.current();
    require(!clicks.matches(firstA) && !clicks.matches(b) && clicks.matches(secondA),
        "Rapid A/B/A clicks must reject older results even when gene and row match again");
    clicks.advance(); // clearing selection, Update or dataset invalidation
    require(!clicks.matches(secondA), "Invalidation must prevent a completed click from publishing");
    AnalysisWorkflow de, trait;
    require(!de.active && !trait.active && !de.busy(), "Analysis must start without demand");
    de.setActive(true);
    de.beginRead();
    auto old = de.cancellation();
    de.pending = true;
    de.setActive(false);
    require(*old && de.busy() && !de.pending, "Closing must cancel but keep the reader busy until it exits");
    require(!*trait.cancellation(), "Closing DE must not cancel trait analysis");
    de.setActive(true);
    require(*old && !*de.cancellation() && old != de.cancellation(), "Reopening must not revive an old result");
    de.finish();
    de.beginCompute();
    require(de.phase() == AnalysisWorkflow::Phase::Computing, "Statistics must use the computing phase");
    trait.setActive(true); trait.beginRead();
    auto current = de.cancellation(), traitCurrent = trait.cancellation();
    de.cancel(); trait.cancel(); // main Update or destruction
    require(*current && *traitCurrent && de.busy() && trait.busy(), "Update must cancel both generations without falsely finishing them");
    de.finish(); trait.finish();
    de.resume(); trait.resume();
    require(!*de.cancellation() && !*trait.cancellation() && *current && *traitCurrent, "Restart must preserve cancellation of superseded workers");
    require(de.active && trait.active, "An Update must preserve open-window demand");
    std::cout << "PASS: demand, phase transitions, close/reopen, independent cancellation, Update restart\n";
}
