#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

// 簡易相容表 parser（支援 banking_compatible.rpt.txt 與 debanking_compatible.rpt.txt）
// 會同時建立 single->multi 與 multi->single 兩向的查表。
// 用法：
//   CompatMaps maps;
//   CompatParser::load("banking_compatible.rpt.txt", maps);
//   CompatParser::load("debanking_compatible.rpt.txt", maps);
//   // 之後 maps.single2multi / maps.multi2single 就齊了。

struct CompatMaps {
    std::unordered_map<std::string, std::vector<std::string>> single2multi; // banking 查表
    std::unordered_map<std::string, std::vector<std::string>> multi2single; // debanking 查表
};

namespace CompatParser {

inline std::string trim(const std::string &s){
    size_t i = 0, j = s.size();
    while (i < j && std::isspace((unsigned char)s[i])) ++i;
    while (j > i && std::isspace((unsigned char)s[j-1])) --j;
    return s.substr(i, j - i);
}

inline std::vector<std::string> split_ws(const std::string &s){
    std::istringstream iss(s);
    std::vector<std::string> out; std::string t;
    while (iss >> t) out.push_back(t);
    return out;
}

// 嘗試從表頭判斷左欄位是 Single 還是 Multi
inline void detect_orientation(std::istream &in, bool &left_is_single){
    left_is_single = true; // 預設（banking 檔）
    std::streampos pos = in.tellg();
    std::string line;
    while (std::getline(in, line)){
        auto l = trim(line);
        if (l.empty()) continue;
        if (l.find("Single bit Lib cell") != std::string::npos){ left_is_single = true; break; }
        if (l.find("Multi bit Lib cell")  != std::string::npos){ left_is_single = false; break; }
        if (l.find("----") != std::string::npos) continue; // 分隔線
        // 一旦遇到資料列前仍未偵測到，就用預設
        if (!line.empty()) break;
    }
    in.clear(); in.seekg(pos);
}

inline void insert_unique(std::unordered_map<std::string, std::vector<std::string>> &mp,
                          const std::string &k, const std::string &v){
    auto &vec = mp[k];
    if (std::find(vec.begin(), vec.end(), v) == vec.end()) vec.push_back(v);
}

inline void load_stream(std::istream &in, CompatMaps &maps){
    bool left_is_single = true; // true: 左邊是 single，右邊是 multi（banking 檔）
    detect_orientation(in, left_is_single);

    std::string line, current_left;
    bool in_data = false;

    while (std::getline(in, line)){
        // 跳過空行與分隔線
        std::string raw = line;
        std::string t = trim(raw);
        if (t.empty()) continue;
        if (t.size() >= 3 && t.find("---") != std::string::npos) { in_data = true; continue; }
        if (!in_data) continue; // 尚未到資料區

        // 新條目：行首非空白
        bool new_entry = !raw.empty() && !std::isspace((unsigned char)raw[0]);
        if (new_entry){
            // 取左欄名稱：到 "COMPATIBLE" 之前
            size_t comp_pos = raw.find("COMPATIBLE");
            std::string left = (comp_pos == std::string::npos) ? raw : raw.substr(0, comp_pos);
            left = trim(left);
            current_left = left;

            // 若同一行右側就有第一個相容 cell，也一併加進去
            if (comp_pos != std::string::npos){
                std::string right = trim(raw.substr(comp_pos + std::string("COMPATIBLE").size()));
                if (!right.empty()){
                    auto toks = split_ws(right);
                    if (!toks.empty()){
                        const std::string &r0 = toks[0];
                        if (left_is_single){
                            insert_unique(maps.single2multi, current_left, r0);
                            insert_unique(maps.multi2single, r0, current_left);
                        }else{
                            insert_unique(maps.multi2single, current_left, r0);
                            insert_unique(maps.single2multi, r0, current_left);
                        }
                    }
                }
            }
        } else {
            // 延續行：右欄只有相容 cell 名稱
            // 直接撈第一個 token 即可
            auto toks = split_ws(raw);
            if (toks.empty()) continue;
            const std::string &r0 = toks[0];
            if (current_left.empty()) continue; // 防呆
            if (left_is_single){
                insert_unique(maps.single2multi, current_left, r0);
                insert_unique(maps.multi2single, r0, current_left);
            } else {
                insert_unique(maps.multi2single, current_left, r0);
                insert_unique(maps.single2multi, r0, current_left);
            }
        }
    }
}

inline bool load(const std::string &path, CompatMaps &maps){
    std::ifstream fin(path);
    if (!fin) return false;
    load_stream(fin, maps);
    return true;
}

// 小工具：查詢 API
inline const std::vector<std::string>& single_to_multi(const CompatMaps &m, const std::string &single){
    static const std::vector<std::string> kEmpty;
    auto it = m.single2multi.find(single);
    return (it==m.single2multi.end()) ? kEmpty : it->second;
}
inline const std::vector<std::string>& multi_to_single(const CompatMaps &m, const std::string &multi){
    static const std::vector<std::string> kEmpty;
    auto it = m.multi2single.find(multi);
    return (it==m.multi2single.end()) ? kEmpty : it->second;
}

} // namespace CompatParser