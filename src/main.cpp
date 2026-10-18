#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "CardRepository.hpp"
#include "Plan.hpp"

struct Analysis {                                    // wires it together: Record -> SoilCard -> FertilizerPlan -> CostComparison
    SoilCard card; FertilizerPlan plan; CostComparison cost;
    Analysis(const Record& r, const Catalog& c)
        : card(r.card, c.thresholds), plan(card, c.crops.at(r.crop)),
          cost(CurrentPractice(r.urea, r.dap, r.mop), plan, c.prices, r.acres) {}
};

namespace {
std::string T(const std::string& en, const std::string& mr) { return g_marathi ? mr : en; }   // pick the active language

std::string askLine(const std::string& prompt) {
    std::cout << prompt; std::string s;
    if (!std::getline(std::cin, s)) throw std::runtime_error("input closed");
    return s;
}
double askNum(const std::string& prompt, double lo, double hi) {
    for (;;) {
        std::string s = askLine(prompt);
        try { size_t used; double v = std::stod(s, &used); if (used == s.size() && v >= lo && v <= hi) return v; } catch (...) {}
        std::cout << "  " << T("Please enter a number between ", "कृपया ") << lo << T(" and ", " ते ") << hi << T("", " मधील संख्या टाका") << "\n";
    }
}

Record demo() {                                      // typical black-soil farm: low N, high P and K, zinc deficient
    return {CardRepository::today(), "Demo Farmer", "Demo Village", "Wheat", 2,
            {8.2, 0.42, 245, 38, 310, 8, 0.4, 0.6, 5.2, 3.1, 0.4}, 130, 100, 50};
}

Record collect(const Catalog& cat, const CardRepository& repo) {
    Record r;
    r.date = CardRepository::today();
    r.farmer = askLine(T("Farmer name: ", "शेतकऱ्याचे नाव: "));
    r.village = askLine(T("Village: ", "गाव: "));
    std::vector<std::string> crops;
    for (auto& [name, c] : cat.crops) { crops.push_back(name); std::cout << "  " << crops.size() << ". " << c.name.get() << "\n"; }
    r.crop = crops[size_t(askNum(T("Crop number: ", "पिकाचा क्रमांक: "), 1, double(crops.size()))) - 1];
    r.acres = askNum(T("Land under this crop (acres): ", "या पिकाखालील क्षेत्र (एकर): "), 0.1, 1000);
    auto old = repo.history(r.farmer);
    if (!old.empty() && lower(askLine(T("Earlier card found. Reuse its soil values and fertilizer? (y/n): ",
                                        "आधीचे कार्ड सापडले. तीच जमिनीची मूल्ये व खते वापरायची? (y/n): "))) == "y") {
        r.card = old.back().card; r.urea = old.back().urea; r.dap = old.back().dap; r.mop = old.back().mop;
        return r;
    }
    std::cout << "\n" << T("Values from the Soil Health Card:", "मृदा आरोग्य पत्रिकेवरील मूल्ये:") << "\n";
    for (auto& k : kCardOrder) {
        const Threshold& t = cat.thresholds.at(k);
        r.card.push_back(askNum("  " + t.name.get() + (t.unit.empty() ? "" : " (" + t.unit + ")") + ": ", 0, k == "pH" ? 14 : 5000));
    }
    std::cout << "\n" << T("What does the farmer apply today (kg per acre)?", "शेतकरी आज प्रति एकर किती खत वापरतो (किलो)?") << "\n";
    r.urea = askNum(T("  Urea: ", "  युरिया: "), 0, 2000);
    r.dap = askNum(T("  DAP: ", "  डीएपी: "), 0, 2000);
    r.mop = askNum(T("  MOP (potash): ", "  पोटॅश (MOP): "), 0, 2000);
    return r;
}

void show(const Record& r, const Catalog& cat) {
    Analysis a(r, cat);
    std::cout << "\n===== " << r.farmer << ", " << r.village << " | " << cat.crops.at(r.crop).name.get() << " | " << num(r.acres)
              << T(" acres", " एकर") << " =====\n\n" << Text{"YOUR SOIL, IN PLAIN WORDS", "तुमची जमीन - सोप्या भाषेत"}.get() << "\n";
    for (auto& p : a.card.parameters())
        std::cout << "  [" << levelText(p->level()).get() << "] " << p->reading() << " - " << p->verdict().get() << "\n";
    a.cost.print();
    if (!a.plan.notes().empty()) std::cout << "\n" << Text{"ALSO DO", "हेही करा"}.get() << "\n";
    for (auto& n : a.plan.notes()) std::cout << "  - " << n.get() << "\n";
}

void history(const Catalog& cat, const CardRepository& repo) {
    auto h = repo.history(askLine(T("Farmer name: ", "शेतकऱ्याचे नाव: ")));
    if (h.empty()) { std::cout << "  " << T("No saved cards for this farmer.", "या शेतकऱ्याचे कोणतेही कार्ड जतन केलेले नाही.") << "\n"; return; }
    for (auto& r : h) {
        Analysis a(r, cat);
        std::cout << "  " << r.date << "  " << cat.crops.at(r.crop).name.get() << "  " << T("OC ", "सेंद्रिय कर्ब ") << num(a.card["OC"].value()) << "%  "
                  << T("N ", "नत्र ") << num(a.card["N"].value()) << "  " << T("plan saves Rs ", "बचत Rs ") << fmt(a.cost.saving()) << T("/acre", "/एकर") << "\n";
    }
    if (h.size() > 1) std::cout << "  " << T("Organic carbon: ", "सेंद्रिय कर्ब: ") << num(h.front().card[1]) << "% -> " << num(h.back().card[1]) << "%\n";
}

void villageImpact(const Catalog& cat, const CardRepository& repo) {
    std::map<std::string, Record> latest;            // newest card per farmer
    for (auto& r : repo.load()) latest[lower(r.farmer)] = r;
    if (latest.empty()) { std::cout << "  " << T("No saved cards yet.", "अजून कोणतेही कार्ड जतन केलेले नाही.") << "\n"; return; }
    double acres = 0, rupees = 0, urea = 0; int over = 0;
    for (auto& entry : latest) {
        const Record& r = entry.second; Analysis a(r, cat);
        double extra = r.urea - a.plan.dose().at("Urea");
        acres += r.acres; rupees += a.cost.saving() * r.acres; urea += extra * r.acres; over += extra > 0;
    }
    std::cout << "  " << T("Farmers: ", "शेतकरी: ") << latest.size() << " | " << T("land: ", "क्षेत्र: ") << num(acres) << T(" acres", " एकर") << "\n  "
              << T("Over-applying urea: ", "युरिया जास्त वापरणारे: ") << over << T(" of ", " / ") << latest.size()
              << T(" farmers | net urea saved: ", " शेतकरी | एकूण युरिया बचत: ") << fmt(urea) << T(" kg", " किलो") << "\n  "
              << (rupees >= 0 ? T("Fertilizer spend saved this season: Rs ", "या हंगामात खतावरील बचत: Rs ")
                              : T("Extra fertilizer spend this season: Rs ", "या हंगामात जास्त खर्च: Rs ")) << fmt(std::abs(rupees)) << "\n";
}
}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
#endif
    try {
        Catalog cat; CardRepository repo("data/history.csv");
        for (;;) {
            std::cout << "\n== " << T("MittiMitra: Soil Health Card Translator", "मातीमित्र: मृदा आरोग्य पत्रिका सोप्या भाषेत") << " ==\n"
                      << "1. " << T("New analysis", "नवीन विश्लेषण") << "\n2. " << T("Quick demo", "झटपट डेमो") << "\n3. "
                      << T("Farmer history", "शेतकऱ्याचा इतिहास") << "\n4. " << T("Village impact", "गावावरील परिणाम") << "\n5. "
                      << T("Language: English / Marathi", "भाषा बदला: English / मराठी") << "\n0. " << T("Exit", "बाहेर पडा") << "\n";
            switch (int(askNum(T("Choose: ", "निवडा: "), 0, 5))) {
                case 1: { Record r = collect(cat, repo); show(r, cat); repo.save(r); break; }
                case 2: show(demo(), cat); break;
                case 3: history(cat, repo); break;
                case 4: villageImpact(cat, repo); break;
                case 5: g_marathi = !g_marathi; break;
                default: return 0;
            }
        }
    } catch (const std::exception& e) { std::cerr << "\nStopped: " << e.what() << "\n"; return 1; }
}
