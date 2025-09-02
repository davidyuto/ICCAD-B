
#include "EmitMBFF.h"
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <climits>
#include <cstring>
#include <cctype>

using namespace vparse;

struct FFRef { int mod_idx; int order_in_mod; const FFInstance* ptr; };

static std::unordered_map<std::string, FFRef>
build_index(const VerilogDesign& d){
    std::unordered_map<std::string, FFRef> m;
    for (int mi=0; mi<(int)d.modules.size(); ++mi){
        const auto& M = d.modules[mi];
        for (int ci=0; ci<(int)M.chunks.size(); ++ci){
            const auto& ch = M.chunks[ci];
            if (ch.kind != VerilogModule::Chunk::FF) continue;
            const auto& ff = M.ff_instances[ch.ff_id];
            m.emplace(ff.inst_name, FFRef{mi, ci, &ff});
        }
    }
    return m;
}

static bool icontains(const std::string& s, const char* key){
    auto it = std::search(s.begin(), s.end(), key, key+std::strlen(key),
                          [](char a,char b){ return std::toupper(a)==std::toupper(b); });
    return it != s.end();
}

// --- family 判斷（先判 FSDNQ 再判 FSDN，避免 FSDNQ 被誤判為 FSDN） ---
enum class Family { LSRDPQ, FSDNQ, FSDN, OTHER };
static Family family_of(const std::string& master){
    if (icontains(master, "LSRDPQ")) return Family::LSRDPQ;
    if (icontains(master, "FSDNQ"))  return Family::FSDNQ;
    if (icontains(master, "FSDN"))   return Family::FSDN;
    return Family::OTHER;
}
static bool family_has_qn(Family f){
    switch (f){
        case Family::FSDNQ: return false; // 單顆 FSDNQ 無 QN
        default:            return true;  // 其他家族有 QN
    }
}

static std::string find_net(const std::unordered_map<std::string,std::string>& p2n,
                            std::initializer_list<const char*> keys,
                            const std::string& defv = ""){
    // 精確鍵
    for (auto k: keys) {
        auto it = p2n.find(k);
        if (it != p2n.end()) return it->second;
    }
    // 前綴匹配（D0/D1、Q0/Q1、QN0/QN1、CK/CLK/CP）
    for (auto k: keys) {
        for (const auto& kv : p2n) {
            if (kv.first.rfind(k, 0) == 0) return kv.second;
        }
    }
    return defv;
}

// === 單顆 FF 的 emitter：只輸出 D/Q(/QN)/CK/(SE/SI)/VDD/VSS（LSRDPQ 再加 VDDR） ===
static std::string emit_single_ff(const std::string& master,
                                  const std::string& new_inst,
                                  const FFInstance* leader){
    std::ostringstream os;
    const auto& p2n = leader->pin2net;

    auto fam = family_of(master);
    bool need_qn   = family_has_qn(fam);
    bool need_vddr = (fam == Family::LSRDPQ);

    auto ck = find_net(p2n, {"CK","CLK","CP","C"}, "clk");
    auto d  = find_net(p2n, {"D"},  "VSS");
    auto q  = find_net(p2n, {"Q"},  "UNCONNECTED");
    auto qn = find_net(p2n, {"QN"}, "UNCONNECTED");
    auto se = find_net(p2n, {"SE"}, "VSS");
    auto si = find_net(p2n, {"SI"}, "VSS");

    os << master << " " << new_inst << " (\n";
    os << "  .CK ( "  << ck << " ),\n";
    os << "  .D  ( "  << d  << " ),\n";
    os << "  .Q  ( "  << q  << " ),\n";
    if (need_qn) {
        os << "  .QN ( " << qn << " ),\n";
    }
    os << "  .SE ( "  << se << " ),\n";
    os << "  .SI ( "  << si << " ),\n";

    if (need_vddr)
        os << "  .VDDR ( VDDR ), .VDD ( VDD ), .VSS ( VSS )\n";
    else
        os << "  .VDD ( VDD ), .VSS ( VSS )\n";
    os << ");\n";
    return os.str();
}

// === 多位元 FSDN：D/Q/QN 以 0-based：D0.. / Q0.. / QN0..；SE/SI 強制接 VSS ===
static std::string emit_fsdn(const std::string& master,
                             const std::string& new_inst,
                             const std::vector<const FFInstance*>& mems){
    std::ostringstream os;
    const auto* leader = mems.front();
    auto ck = find_net(leader->pin2net, {"CK","CLK","CP","C"}, "clk");

    os << master << " " << new_inst << " (\n";
    os << "  .CK ( " << ck << " ),\n";

    // D0.. / Q0.. / QN0..
    for (int i=0;i<(int)mems.size();++i)
        os << "  .D" << i  << " ( " << find_net(mems[i]->pin2net, {"D"}, "VSS") << " ),\n";
    for (int i=0;i<(int)mems.size();++i)
        os << "  .Q" << i  << " ( " << find_net(mems[i]->pin2net, {"Q"}, "UNCONNECTED") << " ),\n";
    for (int i=0;i<(int)mems.size();++i)
        os << "  .QN" << i << " ( " << find_net(mems[i]->pin2net, {"QN"}, "UNCONNECTED") << " ),\n";

    // SE/SI 拉低
    os << "  .SE ( VSS ),\n";
    os << "  .SI ( VSS ),\n";

    // 電源
    os << "  .VDD ( VDD ), .VSS ( VSS )\n";
    os << ");\n";
    return os.str();
}

// === 多位元 LSRDPQ：1-based 腳位：D1.. / Q1.. / QN1..；含 VDDR ===
static std::string emit_lsrdpq(const std::string& master,
                               const std::string& new_inst,
                               const std::vector<const FFInstance*>& mems){
    std::ostringstream os;
    const auto* leader = mems.front();
    auto ck = find_net(leader->pin2net, {"CK","CLK","CP","C"}, "clk");

    os << master << " " << new_inst << " (\n";
    os << "  .CK ( " << ck << " ),\n";

    for (int i=0;i<(int)mems.size();++i) {
        int k = i+1;
        os << "  .D"  << k << " ( " << find_net(mems[i]->pin2net, {"D"},  "VSS") << " ),\n";
        os << "  .Q"  << k << " ( " << find_net(mems[i]->pin2net, {"Q"},  "UNCONNECTED") << " ),\n";
        os << "  .QN" << k << " ( " << find_net(mems[i]->pin2net, {"QN"}, "UNCONNECTED") << " ),\n";
    }

    os << "  .VDDR ( VDDR ), .VDD ( VDD ), .VSS ( VSS )\n";
    os << ");\n";
    return os.str();
}

void write_banked_two_types(const VerilogDesign& design,
                            const std::vector<SimpleGroup>& groups,
                            const std::string& out_path)
{
    auto idx = build_index(design);

    // 產生刪除/替換計畫
    std::unordered_set<std::string> erase_set;
    std::unordered_map<std::string,std::string> replace_map; // leader_inst -> new text

    for (const auto& g : groups){
        // 找成員 FF 的 FFInstance*，並以 (mod_idx, order_in_mod) 決定 leader
        std::vector<const FFInstance*> mems;
        int lead_mod = INT_MAX, lead_ord = INT_MAX; const FFInstance* leader=nullptr;

        for (const auto& inst_name : g.members){
            auto it = idx.find(inst_name);
            if (it == idx.end()) continue;
            mems.push_back(it->second.ptr);
            if (it->second.mod_idx < lead_mod ||
               (it->second.mod_idx == lead_mod && it->second.order_in_mod < lead_ord)){
                lead_mod = it->second.mod_idx; lead_ord = it->second.order_in_mod; leader = it->second.ptr;
            }
        }
        if (mems.empty() || !leader) continue;

        // 依 master 判斷類型並產生文字（快速可靠版：1-bit 一律走單顆模板）
        const auto fam = family_of(g.mbff_master);
        std::string text;
        if ((int)mems.size() == 1) {
            text = emit_single_ff(g.mbff_master, g.new_inst, leader);
        } else {
            if (fam == Family::LSRDPQ) {
                text = emit_lsrdpq(g.mbff_master, g.new_inst, mems);
            } else {
                // 預設走 FSDN-like（含 SE/SI tie VSS）
                text = emit_fsdn(g.mbff_master, g.new_inst, mems);
            }
        }

        // leader 改寫，其餘刪除
        replace_map[leader->inst_name] = std::move(text);
        for (auto* m : mems) if (m != leader) erase_set.insert(m->inst_name);
    }

    // 輸出：非 FF 全複製；FF 依計畫替換/刪除
    std::ofstream fout(out_path);
    vparse::write_design(fout, design, [&](const FFInstance& ffi)->std::string{
        if (erase_set.count(ffi.inst_name)) return std::string("\n"); // 刪掉（輸出空行）
        if (auto it = replace_map.find(ffi.inst_name); it!=replace_map.end())
            return it->second;                                        // leader → 新 inst 文字
        return {};                                                   // 其它 FF → 原文
    });
}
