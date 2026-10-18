#pragma once
#include "SoilParameter.hpp"
#include <ctime>

struct Record {                                      // one saved analysis: inputs only, results are recomputed from them
    std::string date, farmer, village, crop;
    double acres = 1;
    std::vector<double> card;                        // values in kCardOrder
    double urea = 0, dap = 0, mop = 0;               // what the farmer applies today, kg per acre
};

class CardRepository {                               // CSV save / load
public:
    explicit CardRepository(std::string path) : path_(std::move(path)) {}

    void save(const Record& r) const {
        bool fresh;
        { std::ifstream in(path_); fresh = in.peek() == std::char_traits<char>::eof(); }
        std::ofstream f(path_, std::ios::app);
        if (!f) throw std::runtime_error("Cannot write " + path_);
        if (fresh) {
            f << "date,farmer,village,crop,acres";
            for (auto& k : kCardOrder) f << ',' << k;
            f << ",urea,dap,mop\n";
        }
        f << r.date << ',' << clean(r.farmer) << ',' << clean(r.village) << ',' << r.crop << ',' << r.acres;
        for (double v : r.card) f << ',' << v;
        f << ',' << r.urea << ',' << r.dap << ',' << r.mop << '\n';
    }

    std::vector<Record> load() const {
        std::vector<Record> out;
        const size_t j = 5 + kCardOrder.size();
        for (auto& r : readCsv(path_, j + 3, false)) {
            Record rec{r[0], r[1], r[2], r[3], std::stod(r[4]), {}, 0, 0, 0};
            for (size_t i = 0; i < kCardOrder.size(); ++i) rec.card.push_back(std::stod(r[5 + i]));
            rec.urea = std::stod(r[j]); rec.dap = std::stod(r[j + 1]); rec.mop = std::stod(r[j + 2]);
            out.push_back(std::move(rec));
        }
        return out;
    }

    std::vector<Record> history(const std::string& farmer) const {
        std::vector<Record> h;
        for (auto& r : load()) if (lower(r.farmer) == lower(farmer)) h.push_back(r);
        return h;
    }

    static std::string today() {
        char b[11]; std::time_t t = std::time(nullptr);
        std::strftime(b, sizeof b, "%Y-%m-%d", std::localtime(&t));
        return b;
    }
private:
    static std::string clean(std::string s) { for (auto& c : s) if (c == ',') c = ' '; return s; }
    std::string path_;
};
