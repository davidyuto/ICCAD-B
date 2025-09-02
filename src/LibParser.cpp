
#include "LibParser.h"
#include "FFSet.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>

// ==== 字串小工具 ====
static inline void trim(std::string& s){
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    if (l == std::string::npos) { s.clear(); return; }
    s = s.substr(l, r-l+1);
}
static inline std::string to_upper(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

// ==== 公開 API ====
bool LibParser::loadLib(const std::string& path){
    return parseOneFile(path);
}

FFPowerArea LibParser::getFFPowerArea(const std::string& cell) const {
    auto it = ff_.find(cell);
    return (it == ff_.end()) ? FFPowerArea{} : it->second;
}

// ==== 內部工具 ====
void LibParser::stripComments(std::string& s){
    // 去掉 // 到行尾
    for (size_t pos = s.find("//"); pos != std::string::npos; pos = s.find("//", pos)) {
        auto end = s.find('\n', pos);
        s.erase(pos, (end==std::string::npos)? s.size()-pos : end-pos);
    }
    // 去掉 /* ... */
    for (size_t pos = s.find("/*"); pos != std::string::npos; pos = s.find("/*", pos)) {
        auto end = s.find("*/", pos+2);
        if (end == std::string::npos) { s.erase(pos); break; }
        s.erase(pos, end-pos+2);
    }
}

bool LibParser::isFFName(const std::string& cellName){
    auto u = to_upper(cellName);
    // 你說 FSDN 與 LSRD；實務上庫多用 LSRDPQ，所以兩者都支援
    // return (u.find("FSDN") != std::string::npos) ||
    //        (u.find("LSRDPQ") != std::string::npos) ||
    //        (u.find("LSRD") != std::string::npos);
    return single_ff_set().count(cellName) > 0 ||
           multi_ff_set().count(cellName) > 0;
}

void LibParser::accumulateValuesFromBlock(const std::string& block, double& sum, size_t& cnt){
    // 找 "values(" 的開始
    size_t vpos = block.find("values");
    if (vpos == std::string::npos) return;

    // 找對應的 '(' 與配對 ')'
    size_t lp = block.find('(', vpos);
    if (lp == std::string::npos) return;
    int depth = 0;
    size_t rp = std::string::npos;
    for (size_t i = lp; i < block.size(); ++i) {
        char c = block[i];
        if (c == '(') ++depth;
        else if (c == ')') {
            --depth;
            if (depth == 0) { rp = i; break; }
        }
    }
    if (rp == std::string::npos || rp <= lp) return;

    // 取出括號內容並去掉引號/續行反斜線
    std::string inner = block.substr(lp + 1, rp - lp - 1);
    inner.erase(std::remove(inner.begin(), inner.end(), '\\'), inner.end());
    inner.erase(std::remove(inner.begin(), inner.end(), '"'), inner.end());

    // 抓所有數字（含正負與科學記號）
    static const std::regex reNum(R"(([+\-]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+\-]?\d+)?))");
    for (std::sregex_iterator it(inner.begin(), inner.end(), reNum), ed; it != ed; ++it) {
        try {
            double v = std::stod((*it)[1].str());
            sum += v;
            ++cnt;
        } catch (...) {
            // 略過非數字
        }
    }
}


bool LibParser::parseOneFile(const std::string& path){
    std::ifstream fin(path);
    if (!fin) return false;
    std::ostringstream oss; oss << fin.rdbuf();
    std::string s = oss.str();
    stripComments(s);

    std::istringstream iss(s);
    std::string line;

    // ===== 狀態 =====
    bool in_cell = false;
    std::string cell_name;
    int  cell_depth = 0;            // 追蹤 cell 的大括號深度

    bool keep_this_cell = false;    // 是否為 FF（FSDN/LSRDPQ/LSRD）
    double cur_area = 0.0;

    // pin(Q*) 區塊
    bool in_pin_q = false;
    int  pin_depth = 0;
    bool pin_q_pending_brace = false; // 偵測到 pin(Q*) 但同一行沒有 '{'，等待下一行

    // internal_power 收集控制
    bool collect_power = false;     // 在 pin(Q*) 內且已遇到 internal_power()、但尚未遇到 timing()
    bool saw_timing = false;        // 一旦遇到 timing()，就停止收集 power
    bool related_ck = false;        // CK/CLK/CP/C？
    bool collecting_values = false; // 正在跨行蒐集 values(...)
    std::string values_acc;

    // internal_power 的深度（支援 multiple blocks）
    bool in_internal = false;
    int  internal_depth = 0;
    bool internal_pending_brace = false;

    double pwr_sum = 0.0; size_t pwr_cnt = 0;

    auto count_ch = [](const std::string& s, char ch)->int{
        int c=0; for(char x: s) if (x==ch) ++c; return c;
    };

    while (std::getline(iss, line)) {
        trim(line);
        if (line.empty()) continue;

        // === 進入 cell(...) ===
        if (!in_cell) {
            if (line.rfind("cell", 0) == 0) {
                auto lp = line.find('('), rp = line.find(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                    cell_name = line.substr(lp+1, rp-lp-1);
                    trim(cell_name);
                    if (!cell_name.empty() && (cell_name.front()=='"' || cell_name.front()=='\'')) {
                        if (cell_name.back()==cell_name.front())
                            cell_name = cell_name.substr(1, cell_name.size()-2);
                    }
                    in_cell = true;
                    keep_this_cell = isFFName(cell_name);
                    cur_area = 0.0;

                    // 重置 cell 內狀態
                    in_pin_q = false;  pin_depth = 0;  pin_q_pending_brace = false;
                    collect_power = false; saw_timing = false;
                    related_ck = false; collecting_values = false; values_acc.clear();
                    in_internal = false; internal_depth = 0; internal_pending_brace = false;
                    pwr_sum = 0.0; pwr_cnt = 0;

                    // 設定 cell 深度
                    cell_depth = count_ch(line, '{') - count_ch(line, '}');
                }
            }
            continue;
        }

        // === 在 cell 內（非 FF 僅維護深度）===
        if (!keep_this_cell) {
            cell_depth += count_ch(line, '{') - count_ch(line, '}');
            if (cell_depth <= 0) {
                // reset
                in_cell = false; cell_name.clear(); keep_this_cell = false; cur_area = 0.0;
                in_pin_q = false; pin_depth = 0; pin_q_pending_brace = false;
                collect_power = false; saw_timing = false;
                in_internal = false; internal_depth = 0; internal_pending_brace = false;
                related_ck = false; collecting_values = false; values_acc.clear();
                pwr_sum = 0.0; pwr_cnt = 0;
            }
            continue;
        }

        // --- area ---
        if (!in_pin_q && line.rfind("area", 0) == 0) {
            auto colon = line.find(':'), semi = line.find(';', colon+1);
            if (colon != std::string::npos) {
                std::string num = (semi==std::string::npos) ? line.substr(colon+1)
                                                            : line.substr(colon+1, semi-colon-1);
                trim(num);
                try { cur_area = std::stod(num); } catch(...) {}
            }
        }

        // --- 偵測 pin(Q*)（允許 '{' 在下一行） ---
        if (!in_pin_q) {
            if (line.rfind("pin", 0) == 0) {
                auto lp = line.find('('), rp = line.find(')', lp+1);
                if (lp != std::string::npos && rp != std::string::npos) {
                    std::string pname = line.substr(lp+1, rp-lp-1);
                    trim(pname);
                    auto up = to_upper(pname);
                    if (!up.empty() && up[0] == 'Q') {
                        if (line.find('{', rp) != std::string::npos) {
                            in_pin_q = true;
                            pin_depth = 1;
                            collect_power = false; saw_timing = false;
                        } else {
                            pin_q_pending_brace = true;
                            collect_power = false; saw_timing = false;
                        }
                    }
                }
            }
        } else {
            // 只要還沒遇到 timing()，就尋找 internal_power 與 values
            if (!saw_timing) {
                // timing() 出現 → 停止收集 internal_power 的 values
                if (line.rfind("timing", 0) == 0) {
                    saw_timing = true;
                    if (collecting_values && related_ck && !values_acc.empty()) {
                        accumulateValuesFromBlock(values_acc, pwr_sum, pwr_cnt);
                        collecting_values = false;
                        values_acc.clear();
                    }
                }

                // internal_power begin
                if (!in_internal && line.rfind("internal_power", 0) == 0) {
                    if (line.find('{') != std::string::npos) {
                        in_internal = true;
                        internal_depth = 1;
                        collect_power = true;
                        related_ck = false;
                        collecting_values = false; values_acc.clear();
                    } else {
                        internal_pending_brace = true;
                        collect_power = true;
                        related_ck = false;
                        collecting_values = false; values_acc.clear();
                    }
                }

                // internal_power 內容（只有 collect_power==true 時才有效）
                if (in_internal && collect_power) {
                    // related_pin
                    if (line.rfind("related_pin", 0) == 0) {
                        auto colon = line.find(':'), semi = line.find(';', colon+1);
                        std::string val;
                        if (colon != std::string::npos) {
                            val = (semi==std::string::npos) ? line.substr(colon+1)
                                                            : line.substr(colon+1, semi-colon-1);
                            trim(val);
                            if (!val.empty() && (val.front()=='"' || val.front()=='\'')) {
                                if (val.back()==val.front())
                                    val = val.substr(1, val.size()-2);
                            }
                        }
                        auto vUp = to_upper(val);
                        related_ck = (vUp == "CK" || vUp == "CLK" || vUp == "CP" || vUp == "C");
                    }

                    // values(...)：跨行累積（僅 related_ck==true）
                    if (related_ck) {
                        if (!collecting_values && line.find("values(") != std::string::npos) {
                            collecting_values = true;
                            values_acc.clear();
                        }
                        if (collecting_values) {
                            values_acc.append(line);
                            if (line.find(");") != std::string::npos) {
                                accumulateValuesFromBlock(values_acc, pwr_sum, pwr_cnt);
                                collecting_values = false;
                                values_acc.clear();
                            }
                        }
                    }
                }
            }
        }

        // === 統一做大括號深度更新 & 區塊收尾 ===
        int opens  = count_ch(line, '{');
        int closes = count_ch(line, '}');

        // pin(Q*) 等待下一行的 '{'
        if (pin_q_pending_brace && opens > 0) {
            in_pin_q = true;
            pin_depth = 0; // 之後用統一增減來處理
            collect_power = false; saw_timing = false;
            pin_q_pending_brace = false;
        }
        // internal_power 等待下一行的 '{'
        if (internal_pending_brace && opens > 0) {
            in_internal = true;
            internal_depth = 0;
            collect_power = true;
            related_ck = false;
            collecting_values = false; values_acc.clear();
            internal_pending_brace = false;
        }

        // 更新 internal_power 深度
        if (in_internal) {
            internal_depth += opens - closes;
            if (internal_depth <= 0) {
                // 區塊結束（但只要還沒遇到 timing()，後面可能還有新的 internal_power）
                if (collecting_values && related_ck && !values_acc.empty()) {
                    accumulateValuesFromBlock(values_acc, pwr_sum, pwr_cnt);
                    collecting_values = false;
                    values_acc.clear();
                }
                in_internal = false;
                related_ck = false;
            }
        }

        // 更新 pin 深度
        if (in_pin_q) {
            pin_depth += opens - closes;
            if (pin_depth <= 0) {
                in_pin_q = false;
                collect_power = false;
                saw_timing = false;
            }
        }

        // 最後更新 cell 深度 & 判定 cell 結束
        cell_depth += opens - closes;
        if (cell_depth <= 0) {
            if (keep_this_cell) {
                FFPowerArea& dst = ff_[cell_name];
                dst.area  = cur_area;
                dst.power = (pwr_cnt ? (pwr_sum / (double)pwr_cnt) : 0.0);
                // 只在離開 FF cell 時輸出 area & power
                std::cerr << "[FF] " << cell_name
                          << " area=" << dst.area
                          << " power=" << dst.power
                          << std::endl;
            }
            // reset
            in_cell = false;
            cell_name.clear();
            keep_this_cell = false;
            cur_area = 0.0;

            in_pin_q = false;  pin_depth = 0;  pin_q_pending_brace = false;
            in_internal = false; internal_depth = 0; internal_pending_brace = false;
            collect_power = false; saw_timing = false;
            related_ck = false; collecting_values = false; values_acc.clear();
            pwr_sum = 0.0; pwr_cnt = 0;
        }
    }

    return true;
}

void LibParser::dumpCache(const std::string& path) const {
    std::ofstream fout(path);
    if (!fout) {
        std::cerr << "[Error] Cannot open cache file for writing: " << path << "\n";
        return;
    }
    for (const auto& [cell, pa] : ff_) {
        fout << cell << " " << pa.area << " " << pa.power << "\n";
    }
    std::cout << "[Cache] Dumped " << ff_.size() << " FF cells to " << path << "\n";
}

bool LibParser::loadCache(const std::string& path) {
    std::ifstream fin(path);
    if (!fin) {
        std::cerr << "[Cache] No cache file found: " << path << "\n";
        return false;
    }
    ff_.clear();
    std::string cell;
    double area, power;
    while (fin >> cell >> area >> power) {
        ff_[cell] = {area, power};
    }
    std::cout << "[Cache] Loaded " << ff_.size() << " FF cells from " << path << "\n";
    return !ff_.empty();
}
