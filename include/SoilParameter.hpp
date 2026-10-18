#pragma once
#include "Util.hpp"
#include <map>
#include <memory>
#include <optional>

enum class Level { Low, Adequate, High };
inline Text levelText(Level l) {
    static const Text t[] = {{"LOW", "कमी"}, {"OK", "ठीक"}, {"HIGH", "जास्त"}};
    return t[int(l)];
}

struct Threshold {                                   // one row of data/thresholds.csv
    Text name; std::string unit; double low, high; std::string product; double kgPerAcre;
};
using ThresholdTable = std::map<std::string, Threshold>;
inline const std::vector<std::string> kCardOrder = {"pH", "OC", "N", "P", "K", "S", "Zn", "B", "Fe", "Mn", "Cu"};

inline ThresholdTable loadThresholds(const std::string& path) {
    ThresholdTable t;
    for (auto& r : readCsv(path, 8))
        t[r[0]] = Threshold{{r[1], r[2]}, r[3], std::stod(r[4]), std::stod(r[5]), r[6], r[7].empty() ? 0.0 : std::stod(r[7])};
    return t;
}

struct Remedy { Text advice; std::string product; double kgPerAcre = 0; };   // product empty => advice only

class SoilParameter {                                // abstract: every card entry judges itself
public:
    SoilParameter(std::string key, double value, Threshold t) : key_(std::move(key)), value_(value), t_(std::move(t)) {}
    virtual ~SoilParameter() = default;
    virtual Level level() const { return value_ < t_.low ? Level::Low : value_ > t_.high ? Level::High : Level::Adequate; }
    virtual Text verdict() const = 0;                                              // one line a farmer can act on
    virtual std::optional<Remedy> deficiencyRule() const { return std::nullopt; }  // what to add when it is off
    virtual double doseFactor() const { return 1.0; }                              // scales the crop's N-P-K dose
    const std::string& key() const { return key_; }
    double value() const { return value_; }
    std::string reading() const { return num(value_) + (t_.unit.empty() ? "" : " " + t_.unit); }
protected:
    std::string key_; double value_; Threshold t_;
};

class MacroNutrient : public SoilParameter {         // N, P, K: low -> add 25%, high -> cut 25%
public:
    using SoilParameter::SoilParameter;
    Text verdict() const override {
        static const Text say[] = {{"is LOW - apply a little extra", "कमी आहे - थोडे जास्त खत द्या"},
                                   {"is enough - apply the normal dose", "पुरेशा प्रमाणात आहे - नेहमीइतकेच खत द्या"},
                                   {"is HIGH - apply less, extra is wasted money", "जास्त आहे - कमी खत द्या, पैसे वाया जातात"}};
        const Text& s = say[int(level())];
        return {t_.name.en + " " + s.en, t_.name.mr + " " + s.mr};
    }
    double doseFactor() const override { return level() == Level::Low ? 1.25 : level() == Level::High ? 0.75 : 1.0; }
};

class PH : public SoilParameter {                    // acidic < low, strongly alkaline > high
public:
    using SoilParameter::SoilParameter;
    Text verdict() const override {
        if (level() == Level::Low) return {"Soil is acidic - crops cannot take up nutrients well", "जमीन आम्लयुक्त आहे - पिकांना अन्नद्रव्ये नीट मिळत नाहीत"};
        if (level() == Level::High) return {"Soil is strongly alkaline - nutrients get locked up", "जमीन खूप क्षारयुक्त आहे - अन्नद्रव्ये पिकांना मिळत नाहीत"};
        return {"Soil pH is fine for most crops", "जमिनीचा सामू बहुतेक पिकांसाठी योग्य आहे"};
    }
    std::optional<Remedy> deficiencyRule() const override {
        if (level() == Level::Low) return Remedy{{"Add agricultural lime (ask your KVK for the quantity)", "कृषी चुना वापरा - प्रमाण KVK कडून विचारा"}, "", 0};
        if (level() == Level::High) return Remedy{{"Add gypsum and farmyard manure; avoid excess urea", "जिप्सम व शेणखत वापरा; युरिया जास्त वापरू नका"}, "", 0};
        return std::nullopt;
    }
};

class OrganicCarbon : public SoilParameter {
public:
    using SoilParameter::SoilParameter;
    Text verdict() const override {
        if (level() == Level::Low) return {"Organic carbon is LOW - soil holds little water and nutrients", "सेंद्रिय कर्ब कमी आहे - जमीन पाणी व अन्नद्रव्ये कमी धरते"};
        return {"Organic carbon is good", "सेंद्रिय कर्ब चांगल्या प्रमाणात आहे"};
    }
    std::optional<Remedy> deficiencyRule() const override {
        if (level() != Level::Low) return std::nullopt;
        return Remedy{{"Add 2 tonnes of farmyard manure or compost per acre", "प्रति एकर 2 टन शेणखत किंवा कंपोस्ट घाला"}, "", 0};
    }
};

class Micronutrient : public SoilParameter {         // S, Zn, B, Fe, Mn, Cu: only a deficiency matters
public:
    using SoilParameter::SoilParameter;
    Level level() const override { return value_ < t_.low ? Level::Low : Level::Adequate; }
    Text verdict() const override {
        if (level() == Level::Adequate) return {t_.name.en + " is sufficient", t_.name.mr + " पुरेशा प्रमाणात आहे"};
        return {t_.name.en + " is deficient (needs at least " + num(t_.low) + " " + t_.unit + ")",
                t_.name.mr + " कमी आहे (किमान " + num(t_.low) + " " + t_.unit + " हवे)"};
    }
    std::optional<Remedy> deficiencyRule() const override {
        if (level() == Level::Low) return Remedy{{}, t_.product, t_.kgPerAcre};
        return std::nullopt;
    }
};

class SoilCard {                                     // composition: the card owns its parameters
public:
    SoilCard(const std::vector<double>& values, const ThresholdTable& t) {
        for (size_t i = 0; i < kCardOrder.size(); ++i) params_.push_back(make(kCardOrder[i], values.at(i), t.at(kCardOrder[i])));
    }
    const SoilParameter& operator[](const std::string& key) const {
        for (auto& p : params_) if (p->key() == key) return *p;
        throw std::out_of_range("no soil parameter " + key);
    }
    const std::vector<std::unique_ptr<SoilParameter>>& parameters() const { return params_; }
private:
    static std::unique_ptr<SoilParameter> make(const std::string& k, double v, const Threshold& t) {   // factory
        if (k == "pH") return std::make_unique<PH>(k, v, t);
        if (k == "OC") return std::make_unique<OrganicCarbon>(k, v, t);
        if (k == "N" || k == "P" || k == "K") return std::make_unique<MacroNutrient>(k, v, t);
        return std::make_unique<Micronutrient>(k, v, t);
    }
    std::vector<std::unique_ptr<SoilParameter>> params_;
};
