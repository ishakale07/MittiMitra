#pragma once
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

inline bool g_marathi = false;                       // language switch for farmer-facing text

struct Text {                                        // one message, two languages
    std::string en, mr;
    const std::string& get() const { return g_marathi ? mr : en; }
};

inline std::string fmt(double v, int d = 0) {        // fixed decimals
    std::ostringstream o; o << std::fixed << std::setprecision(d) << v; return o.str();
}
inline std::string num(double v) {                   // 0.40 -> "0.4", 245.00 -> "245"
    std::string s = fmt(v, 2);
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s;
}
inline std::string lower(std::string s) { for (auto& c : s) c = char(std::tolower((unsigned char)c)); return s; }

using Row = std::vector<std::string>;
inline Row split(const std::string& s) {
    Row out(1);
    for (char c : s) { if (c == ',') out.emplace_back(); else out.back() += c; }
    return out;
}
inline std::vector<Row> readCsv(const std::string& path, size_t cols, bool required = true) {
    std::ifstream f(path);
    if (!f) { if (required) throw std::runtime_error("Cannot open " + path + " (run from the project folder)"); return {}; }
    std::vector<Row> rows; std::string line;
    std::getline(f, line);                           // header
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        rows.push_back(split(line));
        if (rows.back().size() < cols) throw std::runtime_error("Bad row in " + path + ": " + line);
    }
    return rows;
}
