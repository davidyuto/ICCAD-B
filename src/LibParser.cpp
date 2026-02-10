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

// ==== 內建硬寫表：啟動即初始化 ====
LibParser::LibParser() {
    ff_ = {
        // ===== 你提供的 ff_cache 內容（cell → {area, power}）=====
        {"SNPSLOPT25_FSDNQ_V3_4",   {1.776,  0.00197146}},
        {"SNPSHOPT25_LSRDPQ4_2",    {4.44,   0.00047944}},
        {"SNPSSLOPT25_LSRDPQ_2",    {1.2432, 0.00264654}},
        {"SNPSLOPT25_FSDNQ_V3_2",   {1.6872, 0.00145622}},
        {"SNPSSLOPT25_FSDN_V2_1",   {1.1988, 0.000856783}},
        {"SNPSLOPT25_FSDNQ_V3_1",   {1.332,  0.0012355}},
        {"SNPSLOPT25_FSDN2_V2_0P5", {2.3976, 0.000768737}},
        {"SNPSROPT25_FSDN_V2_1",    {1.1988, 0.000836732}},
        {"SNPSLOPT25_FSDN_V2_4",    {1.5096, 0.00144677}},
        {"SNPSROPT25_FSDN4_V2_2",   {5.2392, 0.0015107}},
        {"SNPSLOPT25_FSDN2_V2_1",   {2.3976, 0.00083887}},
        {"SNPSLOPT25_FSDN4_V2_2",   {5.2392, 0.00157532}},
        {"SNPSHOPT25_FSDNQ_V3_4",   {1.776,  0.00170243}},
        {"SNPSROPT25_FSDN4_V2_0P5", {4.7064, 0.000835316}},
        {"SNPSHOPT25_LSRDPQ_2",     {1.2432, 0.000648122}},
        {"SNPSLOPT25_LSRDPQ4_2",    {4.44,   0.000969226}},
        {"SNPSROPT25_FSDNQ_V3_4",   {1.776,  0.0018788}},
        {"SNPSLOPT25_FSDN4_V2_0P5", {4.7064, 0.000894425}},
        {"SNPSHOPT25_LSRDPQ4_1",    {4.0848, 0.000485739}},
        {"SNPSHOPT25_FSDN2_V2_0P5", {2.3976, 0.000686789}},
        {"SNPSROPT25_FSDN2_V2_1",   {2.3976, 0.000846857}},
        {"SNPSLOPT25_LSRDPQ_2",     {1.2432, 0.00156715}},
        {"SNPSHOPT25_FSDN4_V2_1",   {4.7064, 0.00104318}},
        {"SNPSROPT25_FSDNQ_V3_2",   {1.6872, 0.00138822}},
        {"SNPSLOPT25_FSDN_V2_1",    {1.1988, 0.000832119}},
        {"SNPSHOPT25_FSDNQ_V3_2",   {1.6872, 0.0012582}},
        {"SNPSROPT25_LSRDPQ4_2",    {4.44,   0.000520425}},
        {"SNPSLOPT25_LSRDPQ_1",     {1.1544, 0.00159386}},
        {"SNPSHOPT25_FSDN4_V2_2",   {5.2392, 0.0014503}},
        {"SNPSROPT25_FSDN_V2_2",    {1.332,  0.000973494}},
        {"SNPSROPT25_LSRDPQ_1",     {1.1544, 0.000692423}},
        {"SNPSROPT25_LSRDPQ_2",     {1.2432, 0.00067423}},
        {"SNPSLOPT25_FSDN4_V2_1",   {4.7064, 0.00115544}},
        {"SNPSROPT25_LSRDPQ4_1",    {4.0848, 0.000530281}},
        {"SNPSHOPT25_LSRDPQ_1",     {1.1544, 0.000667005}},
        {"SNPSHOPT25_FSDN2_V2_1",   {2.3976, 0.000757589}},
        {"SNPSSLOPT25_LSRDPQ4_2",   {4.44,   0.00152636}},
        {"SNPSLOPT25_FSDN_V2_2",    {1.332,  0.000966611}},
        {"SNPSSLOPT25_FSDN_V2_2",   {1.332,  0.000994837}},
        {"SNPSSLOPT25_FSDNQ_V3_2",  {1.6872, 0.00155771}},
        {"SNPSHOPT25_FSDN_V2_1",    {1.1988, 0.000749295}},
        {"SNPSHOPT25_FSDN_V2_4",    {1.5096, 0.00128773}},
        {"SNPSHOPT25_FSDNQ_V3_1",   {1.332,  0.00103784}},
        {"SNPSLOPT25_LSRDPQ4_1",    {4.0848, 0.000980683}},
        {"SNPSSLOPT25_FSDN4_V2_1",  {4.7064, 0.00114798}},
        {"SNPSROPT25_FSDN4_V2_1",   {4.7064, 0.00108537}},
        {"SNPSROPT25_FSDN_V2_4",    {1.5096, 0.00143244}},
        {"SNPSSLOPT25_LSRDPQ_1",    {1.1544, 0.00267119}},
        {"SNPSROPT25_FSDNQ_V3_1",   {1.332,  0.00114524}},
        {"SNPSSLOPT25_FSDN4_V2_2",  {5.2392, 0.00163647}},
        {"SNPSSLOPT25_FSDN2_V2_1",  {2.3976, 0.000861879}},
        {"SNPSSLOPT25_FSDN4_V2_0P5",{4.7064, 0.000903491}},
        {"SNPSSLOPT25_LSRDPQ4_1",   {4.0848, 0.00154459}},
        {"SNPSHOPT25_FSDN_V2_2",    {1.332,  0.000878781}},
        {"SNPSSLOPT25_FSDN2_V2_0P5",{2.3976, 0.000802688}},
        {"SNPSHOPT25_FSDN4_V2_0P5", {4.7064, 0.00078213}},
        {"SNPSROPT25_FSDN2_V2_0P5", {2.3976, 0.000744835}},
        {"SNPSSLOPT25_FSDNQ_V3_1",  {1.332,  0.00137361}},
        {"SNPSSLOPT25_FSDN_V2_4",   {1.5096, 0.00150469}},
        {"SNPSSLOPT25_FSDNQ_V3_4",  {1.776,  0.00211914}},
        // ===== 需要可再補，你也可隨時用 loadLib 覆寫/擴充 =====
    };
}

// ==== 公開 API ====
bool LibParser::loadLib(const std::string& path){
    return parseOneFile(path); // 讀到的數值會覆寫/補充 ff_
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

static inline std::string to_upper_copy(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}

bool LibParser::isFFName(const std::string& cellName){
    // 以 FF 白名單（FFSet.h）為準
    return single_ff_set().count(cellName) > 0 ||
           multi_ff_set().count(cellName) > 0;
}

void LibParser::accumulateValuesFromBlock(const std::string& block, double& sum, size_t& cnt){
    size_t vpos = block.find("values");
    if (vpos == std::string::npos) return;

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

    std::string inner = block.substr(lp + 1, rp - lp - 1);
    inner.erase(std::remove(inner.begin(), inner.end(), '\\'), inner.end());
    inner.erase(std::remove(inner.begin(), inner.end(), '"'), inner.end());

    static const std::regex reNum(R"(([+\-]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+\-]?\d+)?))");
    for (std::sregex_iterator it(inner.begin(), inner.end(), reNum), ed; it != ed; ++it) {
        try {
            double v = std::stod((*it)[1].str());
            sum += v;
            ++cnt;
        } catch (...) {}
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
    int  cell_depth = 0;

    bool keep_this_cell = false;
    double cur_area = 0.0;

    bool in_pin_q = false;
    int  pin_depth = 0;
    bool pin_q_pending_brace = false;

    bool collect_power = false;
    bool saw_timing = false;
    bool related_ck = false;
    bool collecting_values = false;
    std::string values_acc;

    bool in_internal = false;
    int  internal_depth = 0;
    bool internal_pending_brace = false;

    double pwr_sum = 0.0; size_t pwr_cnt = 0;

    auto count_ch = [](const std::string& s, char ch)->int{
        int c=0; for(char x: s) if (x==ch) ++c; return c;
    };

    auto to_upper = [](std::string s){
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::toupper(c); });
        return s;
    };

    while (std::getline(iss, line)) {
        trim(line);
        if (line.empty()) continue;

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

                    in_pin_q = false;  pin_depth = 0;  pin_q_pending_brace = false;
                    collect_power = false; saw_timing = false;
                    related_ck = false; collecting_values = false; values_acc.clear();
                    in_internal = false; internal_depth = 0; internal_pending_brace = false;
                    pwr_sum = 0.0; pwr_cnt = 0;

                    cell_depth = count_ch(line, '{') - count_ch(line, '}');
                }
            }
            continue;
        }

        if (!keep_this_cell) {
            cell_depth += count_ch(line, '{') - count_ch(line, '}');
            if (cell_depth <= 0) {
                in_cell = false; cell_name.clear(); keep_this_cell = false; cur_area = 0.0;
                in_pin_q = false; pin_depth = 0; pin_q_pending_brace = false;
                collect_power = false; saw_timing = false;
                in_internal = false; internal_depth = 0; internal_pending_brace = false;
                related_ck = false; collecting_values = false; values_acc.clear();
                pwr_sum = 0.0; pwr_cnt = 0;
            }
            continue;
        }

        if (!in_pin_q && line.rfind("area", 0) == 0) {
            auto colon = line.find(':'), semi = line.find(';', colon+1);
            if (colon != std::string::npos) {
                std::string num = (semi==std::string::npos) ? line.substr(colon+1)
                                                            : line.substr(colon+1, semi-colon-1);
                trim(num);
                try { cur_area = std::stod(num); } catch(...) {}
            }
        }

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
            if (!saw_timing) {
                if (line.rfind("timing", 0) == 0) {
                    saw_timing = true;
                    if (collecting_values && related_ck && !values_acc.empty()) {
                        accumulateValuesFromBlock(values_acc, pwr_sum, pwr_cnt);
                        collecting_values = false;
                        values_acc.clear();
                    }
                }

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

                if (in_internal && collect_power) {
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

        int opens  = count_ch(line, '{');
        int closes = count_ch(line, '}');

        if (pin_q_pending_brace && opens > 0) {
            in_pin_q = true;
            pin_depth = 0;
            collect_power = false; saw_timing = false;
            pin_q_pending_brace = false;
        }
        if (internal_pending_brace && opens > 0) {
            in_internal = true;
            internal_depth = 0;
            collect_power = true;
            related_ck = false;
            collecting_values = false; values_acc.clear();
            internal_pending_brace = false;
        }

        if (in_internal) {
            internal_depth += opens - closes;
            if (internal_depth <= 0) {
                if (collecting_values && related_ck && !values_acc.empty()) {
                    accumulateValuesFromBlock(values_acc, pwr_sum, pwr_cnt);
                    collecting_values = false;
                    values_acc.clear();
                }
                in_internal = false;
                related_ck = false;
            }
        }

        if (in_pin_q) {
            pin_depth += opens - closes;
            if (pin_depth <= 0) {
                in_pin_q = false;
                collect_power = false;
                saw_timing = false;
            }
        }

        cell_depth += opens - closes;
        if (cell_depth <= 0) {
            if (keep_this_cell) {
                FFPowerArea& dst = ff_[cell_name];
                dst.area  = cur_area;
                dst.power = (pwr_cnt ? (pwr_sum / (double)pwr_cnt) : 0.0);
                std::cerr << "[FF] " << cell_name
                          << " area=" << dst.area
                          << " power=" << dst.power
                          << std::endl;
            }
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
