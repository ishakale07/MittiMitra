#pragma once
#include "SoilParameter.hpp"
#include <cmath>
#include <iostream>

using Dose = std::map<std::string, double>;          // product -> kg per acre
constexpr double kHaToAcre = 0.4047, kUreaN = 0.46, kDapN = 0.18, kDapP2O5 = 0.46, kMopK2O = 0.60;

struct Crop { Text name; double n, p, k; };          // reference dose, kg/ha of N, P2O5, K2O

class PriceList {                                    // data/products.csv: product, marathi name, rupees per kg
public:
    explicit PriceList(const std::string& path) {
        for (auto& r : readCsv(path, 3)) items_[r[0]] = Item{{r[0], r[1]}, std::stod(r[2])};
    }
    double cost(const Dose& d) const { double c = 0; for (auto& [p, kg] : d) c += kg * items_.at(p).rupees; return c; }
    const Text& name(const std::string& p) const { return items_.at(p).name; }
private:
    struct Item { Text name; double rupees; };
    std::map<std::string, Item> items_;
};

struct Catalog {                                     // everything editable lives in data/*.csv
    ThresholdTable thresholds = loadThresholds("data/thresholds.csv");
    PriceList prices{"data/products.csv"};
    std::map<std::string, Crop> crops;
    Catalog() { for (auto& r : readCsv("data/crops.csv", 5)) crops[r[0]] = Crop{{r[0], r[1]}, std::stod(r[2]), std::stod(r[3]), std::stod(r[4])}; }
};

class CurrentPractice {                              // what the farmer applies today, kg per acre
public:
    CurrentPractice(double urea, double dap, double mop) : dose_{{"Urea", urea}, {"DAP", dap}, {"MOP", mop}} {}
    const Dose& dose() const { return dose_; }
private:
    Dose dose_;
};

struct Need { Text name; double standard, adjusted; };   // kg of nutrient per acre: crop's standard dose vs adjusted for this soil

class FertilizerPlan {                               // crop's reference dose x what the soil already has
public:
    FertilizerPlan(const SoilCard& card, const Crop& crop) {
        auto need = [&](const char* key, const char* nutrient, double kgPerHa) {
            double base = kgPerHa * kHaToAcre, adj = base * card[key].doseFactor();
            const Text& nm = card[key].name();
            needs_.push_back({{nm.en + " (" + nutrient + ")", nm.mr + " (" + nutrient + ")"}, base, adj});
            return adj;
        };
        double n = need("N", "N", crop.n), p = need("P", "P2O5", crop.p), k = need("K", "K2O", crop.k);
        dose_["DAP"] = p / kDapP2O5;                                          // DAP also carries some nitrogen
        dose_["Urea"] = std::max(0.0, (n - dose_["DAP"] * kDapN) / kUreaN);
        dose_["MOP"] = k / kMopK2O;
        for (auto& param : card.parameters())
            if (auto fix = param->deficiencyRule()) {
                if (fix->product.empty()) notes_.push_back(fix->advice);
                else dose_[fix->product] += fix->kgPerAcre;
            }
    }
    const Dose& dose() const { return dose_; }
    const std::vector<Text>& notes() const { return notes_; }
    const std::vector<Need>& needs() const { return needs_; }
private:
    Dose dose_; std::vector<Text> notes_; std::vector<Need> needs_;
};

class CostComparison {                               // before / after spend projection
public:
    CostComparison(const CurrentPractice& now, const FertilizerPlan& plan, const PriceList& prices, double acres)
        : now_(now.dose()), plan_(plan.dose()), prices_(prices), acres_(acres) {}
    double before() const { return prices_.cost(now_); }
    double after() const { return prices_.cost(plan_); }
    double saving() const { return before() - after(); }                      // rupees per acre

    void print() const {
        auto kg = [](const Dose& d, const std::string& p) { auto i = d.find(p); return i == d.end() ? 0.0 : i->second; };
        std::vector<std::string> products = {"Urea", "DAP", "MOP"};
        for (auto& e : plan_) if (!now_.count(e.first)) products.push_back(e.first);   // micronutrients the soil needs
        std::cout << "\n" << Text{"FERTILIZER PLAN (kg per acre)", "खत नियोजन (किलो प्रति एकर)"}.get() << "\n";
        for (auto& p : products) {
            double a = kg(now_, p), b = kg(plan_, p);
            std::cout << "  " << prices_.name(p).get() << ": " << fmt(a) << " -> " << fmt(b) << "  (" << advice(a, b).get() << ")\n";
        }
        const Text per{"per acre", "प्रति एकर"};
        const double s = std::abs(saving());
        std::cout << "\n  " << Text{"Cost now", "सध्याचा खर्च"}.get() << ": Rs " << fmt(before()) << " " << per.get()
                  << "\n  " << Text{"Cost with plan", "शिफारस केलेला खर्च"}.get() << ": Rs " << fmt(after()) << " " << per.get()
                  << "\n  " << (saving() >= 0 ? Text{"YOU SAVE", "तुमची बचत"} : Text{"Extra spend (soil needs it)", "जास्त खर्च (जमिनीला गरज आहे)"}).get()
                  << ": Rs " << fmt(s) << " " << per.get() << "  (Rs " << fmt(s * acres_) << " "
                  << Text{"for " + num(acres_) + " acres", num(acres_) + " एकरसाठी"}.get() << ")\n";
    }
private:
    static Text advice(double a, double b) {
        if (a < 0.5 && b < 0.5) return {"not needed", "गरज नाही"};
        if (a < 0.5) return {"add " + fmt(b) + " kg", fmt(b) + " किलो घाला"};
        double p = (b - a) / a * 100;
        if (p <= -1) return {"reduce by " + fmt(-p) + "%", fmt(-p) + "% कमी करा"};
        if (p >= 1) return {"increase by " + fmt(p) + "%", fmt(p) + "% वाढवा"};
        return {"keep the same", "तेवढेच ठेवा"};
    }
    Dose now_, plan_; const PriceList& prices_; double acres_;
};