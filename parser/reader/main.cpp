/**
 * @file    main.cpp
 * @author  Jinwook Jung
 * @date    2017-12-23 22:12:10
 *
 * Modified: Support multiple LEF (--lef) and single DEF (--def) arguments.
 */

#include "Logger.h"
#include "Watch.h"
#include "ArgParser.h"
#include "LefDefParser.h"
#include "MeanShift.h"
#include "Cluster.h"
#include "PlacementStructure.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#define MAX_NEIGHBORS 5
using namespace std;

void show_usage()
{
    cout << "\nUsage:\n"
         << "  LefDefParser --lef <lef1[,lef2,...]> --def <def>\n\n";
}

int main(int argc, char** argv) {
    auto& ap = ArgParser::get();
    ap.initialize(argc, argv);


    string lef_arg = ap.get_argument("--lef");
    string def_file = ap.get_argument("--def");
    if (lef_arg.empty() || def_file.empty()) {
        cerr << "Usage: " << argv[0]
             << " --lef <lef1[,lef2,...]> --def <def>\n";
        return 1;
    }

    vector<string> lef_files;
    {
        stringstream ss(lef_arg);
        string token;
        while (getline(ss, token, ',')) {
            if (!token.empty()) lef_files.push_back(token);
        }
    }

    auto& ldp = my_lefdef::LefDefParser::get_instance();
    for (auto const& lf : lef_files) {
        cout << "Reading LEF file: " << lf << "\n";
        ldp.read_lef(lf);
    }


    cout << "Reading DEF file: " << def_file << "\n";
    ldp.read_def(def_file);
    ldp.fillFlipFlopNets();



    cout << "\nParsing complete.\n";

    auto rows = extractRowInfos();
    cout << "Total physical rows: " << rows.size() << "\n";
    for (size_t i = 0; i < min(rows.size(), size_t(10)); ++i) {
        auto &r = rows[i];
        cout << "Row@Y="    << r.y
             << " origX="   << r.orig_x
             << " count="   << r.num_sites
             << " pitch="   << r.site_step
             << "\n";
    }

    
    const auto& ffs = ldp.getFFs();
    const auto& mbffs = ldp.getMBFFs();
    const auto& comps = ldp.get_def().get_component_umap();
    size_t mbff_bits_total = 0;
    for (const auto& mb : mbffs) mbff_bits_total += mb.bits.size();

    cout << "\n========== FF Classification Summary ==========\n";
    cout << "\nSummary of FF classification:\n";
    cout << "  Single-bit FF count : " << ffs.size() << "\n";
    cout << "  Multi-bit FF groups : " << mbffs.size() << "\n";
    cout << "Total MBFF instances    : " << mbff_bits_total << "\n";
    cout << "================================================\n";
    // 印出前 10 個 FF
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

    //     =======================================
    // 印出前 10 條 Net 及其詳細 Connection 資訊（含 bbox / pin / instance）
    // // =======================================
    // const auto& nets = ldp.get_def().get_net_umap();

    // std::cout << "\n========== Sample DEF Netlist (up to 10) ==========\n";
    // int net_count = 0;
    // for (const auto& [net_name, net_ptr] : nets) {
    //     std::cout << net_name << "[" << net_ptr->connections_.size() << "]\n";
    //     for (const auto& conn : net_ptr->connections_) {
    //         std::cout << "  " << *conn << "\n";  // 使用你原本的 operator<< (Connection)
    //     }
    //     if (++net_count >= 10) break;
    // }

    // auto netlist = ldp.extractNetlist();

    // std::cout << "\n[Check] Sample InternalNetlist (up to 5 nets):\n";
    // int shown = 0;
    // for (const auto& [net_name, net] : netlist.nets) {
    //     std::cout << "Net: " << net_name << "\n";
    //     for (const auto& conn : net.connections) {
    //         std::cout << "  Instance: " << conn.instance
    //                 << ", Pin: " << conn.pin << "\n";
    //     }
    //     if (++shown >= 5) break;
    // }
    std::cout << "\n[Check] First 3 FFs with Net Connections:\n";
    for (int i = 0; i < std::min(3, (int)ldp.getFFs().size()); ++i) {
        const auto& ff = ldp.getFFs()[i];
        std::cout << "  " << ff.name 
              << " | D: "   << (ff.fanin_net.empty() ? "None" : ff.fanin_net)
              << ", Q: "    << (ff.fanout_net.empty() ? "None" : ff.fanout_net)
              << ", CLK: "  << (ff.clk_net.empty() ? "None" : ff.clk_net)
              << "\n";
    }

        // =============================
    // R-tree + KNN 測試（前3個FF印出最近的鄰居）
    // =============================
    std::vector<my_lefdef::FlipFlop> ff_copy = ffs;
    my_lefdef::FlipFlopClustering clustering(ff_copy);

    // ==== Step 1: 簡單統計 FF 平均密度 ====
    double estimate_radius = 100.0;  // 預設探查半徑
    int sample_count = 100;          // 只隨機抽樣 100 個 FF 來估算密度
    int total_neighbors = 0;

    clustering.buildRTree();  // 確保 R-tree 建好

    for (int i = 0; i < std::min((int)ff_copy.size(), sample_count); ++i) {
        const auto& ff = ff_copy[i];
        int count = clustering.countNeighborsWithinRadius(ff.x, ff.y, estimate_radius);
        total_neighbors += count;
    }

    double avg_density = (double)total_neighbors / sample_count;

    // ==== Step 2: 根據密度估算參數 ====
    int adaptive_K = std::clamp((int)(avg_density * 0.5), 5, 30);  // 建議值介於 5~30 之間
    double adaptive_radius = estimate_radius * 1.5;  // 可放大些，避免漏掉稀疏區
    double max_square_displacement = adaptive_radius * adaptive_radius;

    std::cout << "\n[Auto-Tune] Estimated avg_density = " << avg_density
            << ", adaptive_K = " << adaptive_K
            << ", radius = " << adaptive_radius << "\n";

    // ==== Step 3: 初始化 KNN ====
    clustering.initKNN(adaptive_K, 4000000);

    // 印出前 3 個 FF 及其最近鄰
    std::cout << "\n[Check] R-tree KNN Results for first 3 FFs:\n";
    for (int i = 0; i < std::min(20, (int)ff_copy.size()); ++i) {
        const auto& ff = ff_copy[i];
        std::cout << "  [FF] " << ff.name << " @ (" << ff.x << ", " << ff.y << ") has neighbors:\n";
        if (ff.neighbors.empty()) {
            std::cout << "    No neighbors found.\n";
            continue;
        }
        for (const auto& [nid, dist2] : ff.neighbors) {
            const auto& neighbor = ff_copy[nid];
            std::cout << "    -> " << neighbor.name 
                    << " @ (" << neighbor.x << ", " << neighbor.y << ")"
                    << " | distance = " << std::sqrt(dist2) << "\n";
        }
        std::cout << "  Bandwidth (h_i) = " << ff.bandwidth << "\n";
    }

    clustering.shiftAllFlipFlops(); 

    for (int i = 0; i < std::min((int)ff_copy.size(), sample_count); ++i) {
        const auto& ff = ff_copy[i];
        std::cout << "FF[" << i << "]: "
                << "Old = (" << ff.x << ", " << ff.y << "), "
                << "New = (" << ff.new_x << ", " << ff.new_y << ")"
                << std::endl;
    }

    clustering.buildClusters();
    const auto& clusters = clustering.getClusters();

    std::cout << "\n========== MeanShift Clustering Result ==========\n";
    if (clusters.empty()) {
        std::cout << "No clusters found.\n";
        return 0;
    }
    int count_1 = 0;
    int count_2 = 0;
    int count_3 = 0;
    int count_4 = 0;
    int count_5 = 0;
    int count_more = 0;
    for (const auto& cl : clusters) {
        size_t size = cl.getFFs().size();
        if (size == 1) ++count_1;
        else if (size == 2) ++count_2;
        else if (size == 3) ++count_3;
        else if (size == 4) ++count_4;
        else if (size == 5) ++count_5;
        else if (size > 5) ++count_more;
    }

    std::cout << "\n========== Cluster Size Distribution ==========\n";
    std::cout << "Clusters with 1 FF  : " << count_1 << "\n";
    std::cout << "Clusters with 2 FFs : " << count_2 << "\n";
    std::cout << "Clusters with 3 FFs : " << count_3 << "\n";
    std::cout << "Clusters with >3 FFs: " << count_more << "\n";
    std::cout << "Total Clusters: " << clusters.size() << "\n";
    for (size_t i = 0; i < std::min<size_t>(clusters.size(), 5); ++i) {
        const auto& cl = clusters[i];
        std::cout << "Cluster ID " << cl.getID()
                << ": " << cl.getFFs().size()
                << " FFs, Center = (" << cl.getCenterX() << "," << cl.getCenterY() << ")\n";
        for (const auto* ff : cl.getFFs()) {
            std::cout << "  -> " << ff->name << " @ (" << ff->x << ", " << ff->y << ")\n";
        }
    }
    return 0;
}