/**
 * @file    main.cpp
 * @brief   Main entry with Auto-K selection + Banking
 */

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

using namespace std;

// === Auto-K 選擇函式 ===
int chooseBestK(const std::vector<int>& Ks,
                std::vector<my_lefdef::FlipFlop>& ffs,
                const CompatMaps& maps,
                const LibParser& lib) {
    int bestK = Ks[0];
    double bestScore = 1e18;

    for (int K : Ks) {
        std::cout << "\n[Auto-K] Testing K=" << K << "\n";

        std::vector<my_lefdef::FlipFlop> ffs_copy = ffs;
        my_lefdef::Banking banking(ffs_copy, lib);

        // 🚩 只做 Clustering，不跑 Banking
        banking.run_big(maps, 1.5, 2500.0, 2000.0, K, false);

        auto& clusters = banking.getClusters();
        int totalCluster = clusters.size();

        std::unordered_map<int,int> hist;
        for (auto& c : clusters) hist[(int)c.getFFs().size()]++;

        int single = hist[1];
        int large=0, medium=0;
        for (auto& kv : hist) {
            if (kv.first >= 12) large += kv.second;
            if (kv.first >= 2 && kv.first <= 8) medium += kv.second;
        }

        double p1     = (totalCluster>0)? (double)single / totalCluster * 100.0 : 0;
        double pLarge = (totalCluster>0)? (double)large  / totalCluster * 100.0 : 0;
        double pMed   = (totalCluster>0)? (double)medium / totalCluster * 100.0 : 0;

        std::cout << " - 1-bit clusters   = " << single << " (" << p1 << "%)\n";
        std::cout << " - 2~8-bit clusters = " << medium << " (" << pMed << "%)\n";
        std::cout << " - >=12-bit clusters= " << large  << " (" << pLarge << "%)\n";
        std::cout << " - Total clusters   = " << totalCluster << "\n";

        double score = p1*0.5 + (100-pMed)*0.3 + pLarge*1.0;
        std::cout << " - Score = " << score << "\n";

        if (score < bestScore) {
            bestScore = score;
            bestK = K;
        }
    }

    std::cout << "\n[Auto-K] >>> Chosen K = " << bestK << " <<<\n";
    return bestK;
}

int main(int argc, char** argv) {
    auto& ap = ArgParser::get();
    ap.initialize(argc, argv);

    auto lef_files = ap.get_multiple_arguments("-lef");
    auto def_files = ap.get_multiple_arguments("-def");
    auto lib_files = ap.get_multiple_arguments("-lib");

    if (lef_files.empty() || def_files.empty() || lib_files.empty()) {
        cerr << "Usage: " << argv[0]
             << " -lef <lef1> <lef2> ... -def <def1> <def2> ... -lib <lib1> ...\n";
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

    // === 讀取 .lib ===
    LibParser lib;
    for (const auto& libfile : lib_files) {
        cout << "Reading LIB file: " << libfile << "\n";
        if (!lib.loadLib(libfile)) {
            cerr << "[Error] Failed to load " << libfile << "\n";
            return 1;
        }
    }

    ldp.fillFlipFlopNets();
    cout << "\nParsing complete.\n";

    // ============ Summary ============
    const auto& ffs   = ldp.getFFs();
    const auto& mbffs = ldp.getMBFFs();
    size_t mbff_bits_total = 0;
    for (const auto& mb : mbffs) mbff_bits_total += mb.bits.size();

    cout << "\n========== FF Classification Summary ==========\n";
    cout << "  Single-bit FF count : " << ffs.size() << "\n";
    cout << "  Multi-bit FF groups : " << mbffs.size() << "\n";
    cout << "  Total MBFF instances: " << mbff_bits_total << "\n";
    cout << "================================================\n";

    // === Debug: 顯示前 10 個 FF cell 資訊 ===
    cout << "\n[Debug] First 10 FF cells from .lib:\n";
    int cnt = 0;
    for (auto& kv : lib.table()) {
        cout << "  " << kv.first
             << "  area=" << kv.second.area
             << "  power=" << kv.second.power << "\n";
        if (++cnt >= 10) break;
    }

    // === 顯示特定 cell ===
    vector<string> target_cells = {
        "SNPSHOPT25_FSDN1_V2_0P5", // 1-bit FF
        "SNPSHOPT25_FSDN2_V2_0P5", // 2-bit MBFF
        "SNPSHOPT25_FSDN4_V2_0P5"  // 4-bit MBFF
    };
    cout << "\n[Debug] Selected cells:\n";
    for (const auto& name : target_cells) {
        auto info = lib.getFFPowerArea(name);
        cout << "  " << name
             << "  area=" << info.area
             << "  power=" << info.power << "\n";
    }

    // ============ Load Compat ============
    CompatMaps maps;
    bool ok1 = CompatParser::load("testcase3/banking_compatible.rpt.txt", maps);
    bool ok2 = CompatParser::load("testcase3/debanking_compatible.rpt.txt", maps);
    std::cout << "[banking open] " << ok1
              << "  [debanking open] " << ok2 << "\n";
    std::cout << "single2multi size=" << maps.single2multi.size()
              << "  multi2single size=" << maps.multi2single.size() << "\n";

    // ============ Auto-K 選擇 ============
    std::vector<my_lefdef::FlipFlop> ff_copy = ffs;
    std::vector<int> candidateKs = {10,12,15,18,20};
    int bestK = chooseBestK(candidateKs, ff_copy, maps, lib);

    // ============ 正式跑 Banking ============
    my_lefdef::Banking banking(ff_copy, lib);
    banking.run_big(maps, 1.3, 2500.0, 2000.0, bestK, true);

    return 0;
}
