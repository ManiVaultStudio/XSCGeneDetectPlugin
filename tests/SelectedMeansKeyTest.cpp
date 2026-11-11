#include "../src/SelectedMeansKey.h"
#include <iostream>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    const std::vector<unsigned> unsorted{ 9, 2, 7, 3 };
    const std::vector<int> memberships{ 1, 0, 1, -1 };
    const std::vector<unsigned> sorted{ 2, 3, 7, 9 };
    const std::vector<int> sortedMemberships{ 0, -1, 1, 1 };
    const auto background = selectedMeansKey("dataset", unsorted, memberships, 7754, 25);
    const auto publication = selectedMeansKey("dataset", sorted, sortedMemberships, 7754, 25);
    require(background == publication, "Publication must reuse background means for an unsorted selection");
    require(background != selectedMeansKey("other", sorted, sortedMemberships, 7754, 25), "Dataset replacement must invalidate means");
    require(background != selectedMeansKey("dataset", sorted, sortedMemberships, 7753, 25), "Gene count must invalidate means");
    require(background != selectedMeansKey("dataset", sorted, std::vector<int>{1,-1,0,1}, 7754, 25), "Species reassignment must invalidate means");
    require(background != selectedMeansKey("dataset", sorted, sortedMemberships, 7754, 24), "Species count must invalidate means");
    require(background != selectedMeansKey("dataset", std::vector<unsigned>{2,3,7,8}, sortedMemberships, 7754, 25), "Changed cells must invalidate means");
    bool rejected = false;
    try { selectedMeansKey("dataset", sorted, std::vector<int>{0}, 7754, 25); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Mismatched membership input must fail");
    std::cout << "PASS: order-independent reuse and all cache invalidation checks\n";
}
