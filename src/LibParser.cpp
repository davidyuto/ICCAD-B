#include "LibParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <cctype>
#include <algorithm>

static inline std::string trim(const std::string& s) {
    size_t i = 0, j = s.size();
    while (i < j && std::isspace((unsigned char)s[i])) ++i;
    while (j > i && std::isspace((unsigned char)s[j - 1])) --j;
    return s.substr(i, j - i);
}
static inline std::string tolower_str(std::string s){
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

bool LibParser::isClockName(const std::string& pinName){
    std::string p = tolower_str(pinName);
    return (p == "ck" || p == "clk" || p == "cp" || p == "c");
}

bool LibParser::load(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "[LibParser] Cannot open file: " << filename << "\n";
        return false;
    }

    std::string raw;
    std::string curCell, curPin, relatedPin;
    bool inCell = false, inPin = false, inIntPw = false, inRiseFall = false;
    bool sawIdx1_in_this_RF = false, sawIdx2_in_this_RF = false;
    double curWorst_accum_for_this_IP = 0.0;

    while (std::getline(fin, raw)) {
        std::string line = trim(raw);
        if (line.empty()) continue;

        // 單行 // 註解
        auto cpos = line.find("//");
        if (cpos != std::string::npos) line = trim(line.substr(0, cpos));
        if (line.empty()) continue;

        parseLine(line, curCell, curPin, inCell, inPin, inIntPw, inRiseFall,
                  sawIdx1_in_this_RF, sawIdx2_in_this_RF, relatedPin, curWorst_accum_for_this_IP);
    }
    return true;
}

void LibParser::parseLine(const std::string& line,
                          std::string& curCell,
                          std::string& curPin,
                          bool& inCell,
                          bool& inPin,
                          bool& inIntPw,
                          bool& inRiseFall,
                          bool& sawIdx1_in_this_RF,
                          bool& sawIdx2_in_this_RF,
                          std::string& relatedPin,
                          double& curWorst_accum_for_this_IP) {
    std::smatch m;

    static const std::regex reCell    (R"(^\s*cell\s*\(\s*([^\s\)]+)\s*\)\s*\{)");
    static const std::regex reArea    (R"(^\s*area\s*:\s*([+\-]?\d*\.?\d+(?:[eE][+\-]?\d+)?)\s*;)");
    static const std::regex rePin     (R"(^\s*pin\s*\(\s*([^\s\)]+)\s*\)\s*\{)");
    // internal_power() 或 internal_power(template)
    static const std::regex reIntPw   (R"(^\s*internal_power\s*\([^\)]*\)\s*\{)");
    static const std::regex reRelated (R"(^\s*related_pin\s*:\s*\"?([^\"]+)\"?\s*;)");
    static const std::regex reRiseFall(R"(^\s*(rise_power|fall_power)\s*\([^\)]*\)\s*\{)");
    static const std::regex reIdx1    (R"(\bindex_1\s*\()");
    static const std::regex reIdx2    (R"(\bindex_2\s*\()");
    static const std::regex reValues  (R"(values\s*\((.*)\))");
    static const std::regex reBlockEnd(R"(^\s*\}\s*$)");

    // 1) cell begin
    if (std::regex_search(line, m, reCell)) {
        curCell = m[1];
        inCell = true;
        if (!cells_.count(curCell)) cells_[curCell] = LibCellInfo{curCell, 0.0, 0.0};
        return;
    }

    // 2) area
    if (inCell && std::regex_search(line, m, reArea)) {
        cells_[curCell].area = std::stod(m[1]);
        return;
    }

    // 3) pin begin
    if (inCell && std::regex_search(line, m, rePin)) {
        curPin = m[1];
        inPin = true;
        return;
    }

    // 4) internal_power begin（在 pin 內）
    if (inPin && std::regex_search(line, reIntPw)) {
        inIntPw = true;
        relatedPin.clear();
        curWorst_accum_for_this_IP = 0.0;
        // 若區塊內沒寫 related_pin，預設取當前 pin 名稱
        relatedPin = curPin;
        return;
    }

    // 5) internal_power 內容
    if (inIntPw && std::regex_search(line, m, reRelated)) {
        relatedPin = m[1]; // 覆蓋預設
        return;
    }

    // 6) 進入 rise_power / fall_power 表
    if (inIntPw && std::regex_search(line, reRiseFall)) {
        inRiseFall = true;
        sawIdx1_in_this_RF = false;
        sawIdx2_in_this_RF = false;
        return;
    }

    // 7) 在 rise/fall 表中偵測 index_1 / index_2 與 values
    if (inRiseFall) {
        if (std::regex_search(line, reIdx1)) { sawIdx1_in_this_RF = true; return; }
        if (std::regex_search(line, reIdx2)) { sawIdx2_in_this_RF = true; return; }

        if (std::regex_search(line, m, reValues)) {
            // 只有「出現 index_1 且未出現 index_2」的表才計入
            if (sawIdx1_in_this_RF && !sawIdx2_in_this_RF) {
                std::string vals = m[1];
                static const std::regex reNum(R"(([+\-]?\d*\.?\d+(?:[eE][+\-]?\d+)?))");
                for (std::sregex_iterator it(vals.begin(), vals.end(), reNum), ed; it != ed; ++it) {
                    double v = std::stod((*it)[1]);
                    if (v > curWorst_accum_for_this_IP) curWorst_accum_for_this_IP = v;
                }
            }
            return;
        }
    }

    // 8) 任一區塊結束 '}' 的處理
    if (std::regex_match(line, reBlockEnd)) {
        if (inRiseFall) {
            inRiseFall = false;
            return;
        }
        if (inIntPw) {
            // internal_power 區塊結束：若 relatedPin 是 clock 名，將本區塊累積值併入 cell 的 worst（取 max）
            if (isClockName(relatedPin)) {
                auto& slot = cells_[curCell].worst_clk_power_idx1;
                if (curWorst_accum_for_this_IP > slot) slot = curWorst_accum_for_this_IP;
            }
            inIntPw = false;
            return;
        }
        if (inPin) {
            inPin = false;
            return;
        }
        if (inCell) {
            inCell = false;
            return;
        }
    }
}

const LibCellInfo* LibParser::getCell(const std::string& name) const {
    auto it = cells_.find(name);
    if (it == cells_.end()) return nullptr;
    return &it->second;
}

void LibParser::debugPrint(int limit) const {
    int cnt = 0;
    for (const auto& kv : cells_) {
        std::cout << "[Lib] " << kv.first
                  << " | area=" << kv.second.area
                  << " | worst_clk_power_idx1=" << kv.second.worst_clk_power_idx1
                  << "\n";
        if (++cnt >= limit) break;
    }
}
