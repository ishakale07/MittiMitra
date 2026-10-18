#include "Plan.hpp"
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL line " << __LINE__ << ": " #c "\n"; return 1; } } while (0)

int main() {
    Catalog cat;
    const std::vector<double> ok = {7.5, 0.6, 400, 15, 200, 15, 1, 1, 6, 3, 1};     // everything adequate
    auto dose = [&](std::vector<double> v) { return FertilizerPlan(SoilCard(v, cat.thresholds), cat.crops.at("Wheat")).dose(); };

    SoilCard low(std::vector<double>{7.5, 0.6, 200, 15, 200, 15, 1, 1, 6, 3, 1}, cat.thresholds);
    CHECK(low["N"].level() == Level::Low && low["N"].doseFactor() == 1.25);          // polymorphic dose factor
    CHECK(SoilCard(ok, cat.thresholds)["N"].level() == Level::Adequate);

    auto highP = ok; highP[3] = 40;                                                  // high phosphorus -> 25% less DAP
    CHECK(std::abs(dose(highP).at("DAP") / dose(ok).at("DAP") - 0.75) < 1e-9);

    auto noZn = ok; noZn[6] = 0.3;                                                   // zinc deficiency adds a product
    CHECK(dose(noZn).count("Zinc sulphate") == 1 && dose(ok).count("Zinc sulphate") == 0);

    auto d = dose(ok);                                                               // following the plan saves nothing
    FertilizerPlan plan(SoilCard(ok, cat.thresholds), cat.crops.at("Wheat"));
    CHECK(std::abs(CostComparison(CurrentPractice(d["Urea"], d["DAP"], d["MOP"]), plan, cat.prices, 1).saving()) < 1e-6);
    std::cout << "all tests passed\n";
}
