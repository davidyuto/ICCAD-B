#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <algorithm>

// ====================== 使用方式 ======================
//
// 1) 判斷一個 master cell 是否為 FF：
//     if (CompatParser::is_ff_master(master_name)) { ... }
//
// 2) 區分 single-bit FF / multi-bit FF：
//     if (CompatParser::is_single_ff_master(master_name)) { ... }
//     if (CompatParser::is_multi_ff_master(master_name)) { ... }
//
// 3) 取得 family 類型 (回傳 "FSDN" 或 "LSRDPQ" 或 "")：
//     std::string fam = CompatParser::family_of(master_name);
//
// 4) 取得可 banking 的候選 (single → multi)：
//     auto cands = CompatParser::single_to_multi_candidates(master_name);
//     for (auto& m : cands) { ... }
//
// 5) 取得可 debanking 的候選 (multi → single)：
//     auto cands = CompatParser::multi_to_single_candidates(master_name);
//
// 6) 若已經有 DEF Component：
//     Component* comp = def_.get_component(inst_name);
//     if (comp && CompatParser::is_ff_master(comp->ref_name_)) {
//         // 這個 instance 是 FF
//     }
//
// =====================================================

namespace CompatParser {

// =============== 小工具：字串處理 ===============

inline std::string to_upper_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::toupper(c); });
    return s;
}
inline bool contains_icase(const std::string& s, const std::string& sub) {
    auto S = to_upper_copy(s);
    auto T = to_upper_copy(sub);
    return S.find(T) != std::string::npos;
}

// =============== 1) 三組白名單（硬刻） ===============

inline const std::unordered_set<std::string>& single_ff_set(bool to_upper=false){
    static const std::unordered_set<std::string> S_raw = {
        // FSDN singles
        "SNPSHOPT25_FSDN_V2_1",
        "SNPSHOPT25_FSDN_V2_2",
        "SNPSHOPT25_FSDN_V2_4",
        "SNPSLOPT25_FSDN_V2_1",
        "SNPSLOPT25_FSDN_V2_2",
        "SNPSLOPT25_FSDN_V2_4",
        "SNPSROPT25_FSDN_V2_1",
        "SNPSROPT25_FSDN_V2_2",
        "SNPSROPT25_FSDN_V2_4",
        "SNPSSLOPT25_FSDN_V2_1",
        "SNPSSLOPT25_FSDN_V2_2",
        "SNPSSLOPT25_FSDN_V2_4",
        "SNPSHOPT25_FSDNQ_V3_1",
        "SNPSLOPT25_FSDNQ_V3_1",
        "SNPSROPT25_FSDNQ_V3_1",
        "SNPSSLOPT25_FSDNQ_V3_1",
        "SNPSHOPT25_FSDNQ_V3_2",
        "SNPSLOPT25_FSDNQ_V3_2",
        "SNPSROPT25_FSDNQ_V3_2",
        "SNPSSLOPT25_FSDNQ_V3_2",
        "SNPSHOPT25_FSDNQ_V3_4",
        "SNPSLOPT25_FSDNQ_V3_4",
        "SNPSROPT25_FSDNQ_V3_4",
        "SNPSSLOPT25_FSDNQ_V3_4",
        // LSRDPQ singles
        "SNPSHOPT25_LSRDPQ_1",
        "SNPSHOPT25_LSRDPQ_2",
        "SNPSLOPT25_LSRDPQ_1",
        "SNPSLOPT25_LSRDPQ_2",
        "SNPSROPT25_LSRDPQ_1",
        "SNPSROPT25_LSRDPQ_2",
        "SNPSSLOPT25_LSRDPQ_1",
        "SNPSSLOPT25_LSRDPQ_2",
    };
    static std::unordered_set<std::string> S_upper;
    if (to_upper && S_upper.empty()) for (auto& x : S_raw) S_upper.insert(to_upper_copy(x));
    return to_upper ? S_upper : S_raw;
}

inline const std::unordered_set<std::string>& multi_ff_set(bool to_upper=false){
    static const std::unordered_set<std::string> M_raw = {
        // FSDN multis (2/4 bit)
        "SNPSSLOPT25_FSDN4_V2_1",
        "SNPSROPT25_FSDN4_V2_2",
        "SNPSLOPT25_FSDN4_V2_2",
        "SNPSHOPT25_FSDN4_V2_1",
        "SNPSSLOPT25_FSDN4_V2_0P5",
        "SNPSROPT25_FSDN4_V2_1",
        "SNPSLOPT25_FSDN4_V2_1",
        "SNPSSLOPT25_FSDN2_V2_1",
        "SNPSHOPT25_FSDN4_V2_0P5",
        "SNPSHOPT25_FSDN2_V2_1",
        "SNPSROPT25_FSDN4_V2_0P5",
        "SNPSLOPT25_FSDN4_V2_0P5",
        "SNPSSLOPT25_FSDN2_V2_0P5",
        "SNPSROPT25_FSDN2_V2_1",
        "SNPSLOPT25_FSDN2_V2_1",
        "SNPSSLOPT25_FSDN4_V2_2",
        "SNPSHOPT25_FSDN2_V2_0P5",
        "SNPSROPT25_FSDN2_V2_0P5",
        "SNPSLOPT25_FSDN2_V2_0P5",
        "SNPSHOPT25_FSDN4_V2_2",
        // LSRDPQ multis (4 bit)
        "SNPSSLOPT25_LSRDPQ4_2",
        "SNPSROPT25_LSRDPQ4_2",
        "SNPSSLOPT25_LSRDPQ4_1",
        "SNPSROPT25_LSRDPQ4_1",
        "SNPSHOPT25_LSRDPQ4_2",
        "SNPSLOPT25_LSRDPQ4_2",
        "SNPSHOPT25_LSRDPQ4_1",
        "SNPSLOPT25_LSRDPQ4_1",
    };
    static std::unordered_set<std::string> M_upper;
    if (to_upper && M_upper.empty()) for (auto& x : M_raw) M_upper.insert(to_upper_copy(x));
    return to_upper ? M_upper : M_raw;
}

inline const std::unordered_set<std::string>& ff_set(bool to_upper=false){
    static std::unordered_set<std::string> U_raw;
    static std::unordered_set<std::string> U_upper;
    if (U_raw.empty()) {
        const auto& S = single_ff_set(false);
        const auto& M = multi_ff_set(false);
        U_raw.insert(S.begin(), S.end());
        U_raw.insert(M.begin(), M.end());
    }
    if (to_upper && U_upper.empty()) for (auto& x : U_raw) U_upper.insert(to_upper_copy(x));
    return to_upper ? U_upper : U_raw;
}

// =============== 2) 類別判斷：family_of = "FSDN" / "LSRDPQ" / "" ===============

inline std::string family_of(const std::string& master) {
    if (contains_icase(master, "LSRDPQ")) return "LSRDPQ";
    if (contains_icase(master, "FSDN"))   return "FSDN";
    return "";
}

// 快速 is_* 查詢
inline bool is_single_ff_master(const std::string& master, bool to_upper=false){
    const auto& S = single_ff_set(to_upper);
    const auto& key = to_upper ? to_upper_copy(master) : master;
    return S.find(key) != S.end();
}
inline bool is_multi_ff_master(const std::string& master, bool to_upper=false){
    const auto& M = multi_ff_set(to_upper);
    const auto& key = to_upper ? to_upper_copy(master) : master;
    return M.find(key) != M.end();
}
inline bool is_ff_master(const std::string& master, bool to_upper=false){
    const auto& U = ff_set(to_upper);
    const auto& key = to_upper ? to_upper_copy(master) : master;
    return U.find(key) != U.end();
}

// =============== 3) Banking / Debanking 規則（家族對家族） ===============
//
// 規則：
//   - 如果 single master 屬於 FSDN，回傳所有 FSDN 的 multi（M_raw 中含 "FSDN" 的）
//   - 如果 single master 屬於 LSRDPQ，回傳所有 LSRDPQ 的 multi（M_raw 中含 "LSRDPQ4" 的）
//   - 反向亦然（multi -> singles）
//   - 若 family 不明，回傳空向量
//

inline std::vector<std::string> single_to_multi_candidates(const std::string& single_master){
    std::vector<std::string> out;
    const auto fam = family_of(single_master);
    if (fam.empty()) return out;

    const auto& M = multi_ff_set(false);
    if (fam == "FSDN") {
        for (const auto& m : M) if (contains_icase(m, "FSDN")) out.push_back(m);
    } else if (fam == "LSRDPQ") {
        for (const auto& m : M) if (contains_icase(m, "LSRDPQ")) out.push_back(m);
    }
    return out;
}

inline std::vector<std::string> multi_to_single_candidates(const std::string& multi_master){
    std::vector<std::string> out;
    const auto fam = family_of(multi_master);
    if (fam.empty()) return out;

    const auto& S = single_ff_set(false);
    if (fam == "FSDN") {
        for (const auto& s : S) if (contains_icase(s, "FSDN")) out.push_back(s);
    } else if (fam == "LSRDPQ") {
        for (const auto& s : S) if (contains_icase(s, "LSRDPQ")) out.push_back(s);
    }
    return out;
}

// =============== 4) 若你有 instance->master 的對照，也給 instance 版本 ===============

inline bool is_single_ff_instance(const std::unordered_map<std::string,std::string>& inst2master,
                                  const std::string& inst, bool to_upper=false){
    auto it = inst2master.find(inst);
    if (it == inst2master.end()) return false;
    return is_single_ff_master(it->second, to_upper);
}
inline bool is_multi_ff_instance(const std::unordered_map<std::string,std::string>& inst2master,
                                 const std::string& inst, bool to_upper=false){
    auto it = inst2master.find(inst);
    if (it == inst2master.end()) return false;
    return is_multi_ff_master(it->second, to_upper);
}
inline bool is_ff_instance(const std::unordered_map<std::string,std::string>& inst2master,
                           const std::string& inst, bool to_upper=false){
    auto it = inst2master.find(inst);
    if (it == inst2master.end()) return false;
    return is_ff_master(it->second, to_upper);
}

inline std::vector<std::string> single_to_multi_candidates_instance(
        const std::unordered_map<std::string,std::string>& inst2master,
        const std::string& single_inst){
    auto it = inst2master.find(single_inst);
    if (it == inst2master.end()) return {};
    return single_to_multi_candidates(it->second);
}

inline std::vector<std::string> multi_to_single_candidates_instance(
        const std::unordered_map<std::string,std::string>& inst2master,
        const std::string& multi_inst){
    auto it = inst2master.find(multi_inst);
    if (it == inst2master.end()) return {};
    return multi_to_single_candidates(it->second);
}

} // namespace CompatParser
