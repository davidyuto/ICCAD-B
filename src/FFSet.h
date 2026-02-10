// ================= FFSet.h =================
//
// 修改點：新增此檔案，專門存放單/多 bit FF 名稱集合，統一判斷。
// ===========================================

#pragma once
#include <unordered_set>
#include <string>
#include <algorithm>

// 小工具：字串轉大寫
inline std::string to_upper_copy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return out;
}

// 單 bit FF 集合
inline const std::unordered_set<std::string>& single_ff_set(bool to_upper=false) {
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
    if (to_upper && S_upper.empty())
        for (auto& x : S_raw) S_upper.insert(to_upper_copy(x));
    return to_upper ? S_upper : S_raw;
}

// 多 bit FF 集合
inline const std::unordered_set<std::string>& multi_ff_set(bool to_upper=false) {
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
    if (to_upper && M_upper.empty())
        for (auto& x : M_raw) M_upper.insert(to_upper_copy(x));
    return to_upper ? M_upper : M_raw;
}
