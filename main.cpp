#include "Logger.h"
#include "Watch.h"
#include "ArgParser.h"
#include "LefDefParser.h"
#include "MeanShift.h"
#include "Cluster.h"
#include "Banking.h"
#include "PlacementStructure.h"
#include "CompatParser.h"
#include "LibParser.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <climits>

#define MAX_NEIGHBORS 5
using namespace std;

static inline double clamp_double(double v, double lo, double hi) {
    return std::max(lo, std::min(v, hi));
}

int main(int argc, char** argv) {
    auto& ap = ArgParser::get();
    ap.initialize(argc, argv);

    auto lef_files = ap.get_multiple_arguments("-lef");
    auto def_files = ap.get_multiple_arguments("-def");

    if (lef_files.empty() || def_files.empty()) {
        cerr << "Usage: " << argv[0]
             << " -lef <lef1> <lef2> ... -def <def1> <def2> ...\n";
        return 1;
    }

    auto& ldp = my_lefdef::LefDefParser::get_instance();

    for (const auto& lf : lef_files) {
        cout << "Reading LEF file: " << lf << "\n";
        ldp.read_lef(lf);
    }
    for (const auto& def : def_files) {
        cout << "Reading DEF file: " << def << "\n";
        ldp.read_def(def);
    }

    ldp.fillFlipFlopNets();
    cout << "\nParsing complete.\n";

    // ============ Summary ============
    const auto& ffs   = ldp.getFFs();
    const auto& mbffs = ldp.getMBFFs();
    const auto& comps = ldp.get_def().get_component_umap();

    size_t mbff_bits_total = 0;
    for (const auto& mb : mbffs) mbff_bits_total += mb.bits.size();

    cout << "\n========== FF Classification Summary ==========\n";
    cout << "  Single-bit FF count : " << ffs.size() << "\n";
    cout << "  Multi-bit FF groups : " << mbffs.size() << "\n";
    cout << "  Total MBFF instances: " << mbff_bits_total << "\n";
    cout << "================================================\n";

    // 印前 10 顆 FF
    cout << "\n[Sample] First up to 10 FFs:\n";
    for (size_t i = 0; i < std::min<size_t>(ffs.size(), 10); ++i) {
        const auto& ff = ffs[i];
        auto it = comps.find(ff.name);
        std::string macro = (it != comps.end() && it->second->lef_macro_)
            ? it->second->lef_macro_->name_ : "UNKNOWN";
        cout << "  " << ff.name << " @ (" << ff.x << "," << ff.y << ")"
             << "   [Macro: " << macro << "]\n";
    }

    std::cout << "\n[Check] First 5 FFs with Size:\n";
    for (int i = 0; i < std::min((int)ffs.size(), 5); ++i) {
        const auto& ff = ffs[i];
        std::cout << "  " << ff.name
                  << " @ (" << ff.x << ", " << ff.y << ")"
                  << " | Macro: " << ff.macro
                  << " | Size: " << ff.width << " x " << ff.height << "\n";
    }

    std::cout << "\n[Check] First 3 FFs with Net Connections:\n";
    for (int i = 0; i < std::min(3, (int)ffs.size()); ++i) {
        const auto& ff = ffs[i];
        std::cout << "  " << ff.name
                  << " | D: "   << (ff.fanin_net.empty() ? "None" : ff.fanin_net)
                  << ", Q: "    << (ff.fanout_net.empty() ? "None" : ff.fanout_net)
                  << ", CLK: "  << (ff.clk_net.empty() ? "None" : ff.clk_net)
                  << "\n";
    }

    // ============ Clock domain 分群 + MeanShift ============
    std::vector<my_lefdef::FlipFlop> ff_copy = ffs;

    std::unordered_map<std::string, std::vector<int>> domain2idx;
    for (int i = 0; i < (int)ff_copy.size(); i++) {
        std::string clk = ff_copy[i].clk_net.empty() ? "__NOCLK__" : ff_copy[i].clk_net;
        domain2idx[clk].push_back(i);
    }

    cout << "\n========== Clock Domains ==========\n";
    for (const auto& kv : domain2idx) {
        cout << "  " << kv.first << " : " << kv.second.size() << " FFs\n";
    }

    for (const auto& kv : domain2idx) {
        const std::string& clk = kv.first;
        const auto& idxs = kv.second;
        if (idxs.size() <= 1) continue;

        std::vector<my_lefdef::FlipFlop> dom;
        for (int id : idxs) dom.push_back(ff_copy[id]);

        // 計算 domain 尺度 → 設更保守的位移上限
        int minx = INT_MAX, miny = INT_MAX, maxx = INT_MIN, maxy = INT_MIN;
        for (const auto& f : dom) {
            minx = std::min(minx, f.x);  maxx = std::max(maxx, f.x);
            miny = std::min(miny, f.y);  maxy = std::max(maxy, f.y);
        }
        const double w = double(maxx - minx), h = double(maxy - miny);
        const double diag = std::hypot(w, h);
        const double max_move = clamp_double(0.25 * diag, 800.0, 2000.0);
        const double max_sq_disp = max_move * max_move;

        int K = (int)std::min<size_t>(std::max<size_t>(16, dom.size()/50), 100);
        if ((int)dom.size() <= K) K = std::max(2, (int)dom.size() - 1);

        std::cout << "[MeanShift@" << clk << "] FFs=" << dom.size()
                  << " max_move=" << max_move << " K=" << K << "\n";

        my_lefdef::FlipFlopClustering cl(dom);
        cl.buildRTree();
        cl.initKNN(K, max_sq_disp);
        cl.shiftAllFlipFlops();
// 寫回去ff_copy
        for (size_t j = 0; j < dom.size(); j++) {
            int g = idxs[j];
            ff_copy[g].new_x     = dom[j].new_x;
            ff_copy[g].new_y     = dom[j].new_y;
            ff_copy[g].bandwidth = dom[j].bandwidth;
            ff_copy[g].isShifting = dom[j].isShifting;
            ff_copy[g].isLegalize = true;
        }
    }

    // ============ Compatible File Parser ============
    CompatMaps maps;
    bool ok1 = CompatParser::load("testcase3/banking_compatible.rpt.txt", maps);
    bool ok2 = CompatParser::load("testcase3/debanking_compatible.rpt.txt", maps);
    std::cout << "[banking open] " << ok1
              << "  [debanking open] " << ok2 << "\n";
    std::cout << "single2multi size=" << maps.single2multi.size()
              << "  multi2single size=" << maps.multi2single.size() << "\n";

    // 查 banking: 單 bit → 多 bit 候選
    auto &cands = CompatParser::single_to_multi(maps, "SNPSHOPT25_FSDN_V2_1");
    std::cout << "Single SNPSHOPT25_FSDN_V2_1 compatible MBFF:\n";
    for (auto &s : cands) std::cout << "  - " << s << "\n";

    // 查 debanking: 多 bit → 單 bit 候選
    auto &cands2 = CompatParser::multi_to_single(maps, "SNPSHOPT25_FSDN4_V2_1");
    std::cout << "Multi SNPSHOPT25_FSDN4_V2_1 compatible single-bit:\n";
    for (auto &s : cands2) std::cout << "  - " << s << "\n";

    // ============ Banking with compatibility ============
    my_lefdef::Banking banking(ff_copy);
    banking.run_big(maps, /*tau_merge=*/1.5, /*max_pair_dist=*/2500.0, /*h_cap=*/2000.0);


    // LibParser lib;
    // if (lib.load("testcase3/SNPSHOPT25/liberty/nldm/base/snps25hopt_base_ff0p88v25c.lib")) {
    //     std::cout << "[LibParser] Load success!\n";
    //     lib.debugPrint(10); // 印前 10 顆 cell
    // } else {
    //     std::cout << "[LibParser] Load failed.\n";
    // }

    std::cout << "\n========== Liberty (.lib) Quick Check (index_1 only) ==========\n";
    LibParser lib;
    if (lib.load("testcase3/SNPSHOPT25/liberty/nldm/base/snps25hopt_base_ff0p88v25c.lib")) { // ← 換成你的 .lib 路徑
        std::cout << "[LibParser] Loaded.\n";
        lib.debugPrint(10); // 先看看前 10 個 cell

        // 指定幾顆 cell 檢查
        const char* probes[] = {
            "SNPSHOPT25_FSDN_V2_1",
            "SNPSHOPT25_FSDN4_V2_0P5"
        };
        for (auto p : probes) {
            if (auto* info = lib.getCell(p)) {
                std::cout << "[Probe] " << p
                          << " | area=" << info->area
                          << " | worst_clk_power_idx1=" << info->worst_clk_power_idx1
                          << "\n";
            } else {
                std::cout << "[Probe] " << p << " not found in .lib\n";
            }
        }
    } else {
        std::cout << "[LibParser] Load failed.\n";
    }
    return 0;
}
