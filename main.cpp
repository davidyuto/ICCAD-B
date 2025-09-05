/**
 * @file    main.cpp
 * @brief   Main entry with Auto-K selection + Banking
 */

#include "Logger.h"
#include "Watch.h"
#include "ArgParser.h"
#include "LefDefParser.h"
#include "VerilogParser.h"
#include "MeanShift.h"
#include "Cluster.h"
#include "Banking.h"
#include "PlacementStructure.h"
#include "CompatParser.h"
#include "LibParser.h"
#include "EmitMBFF.h"
#include "ListWriter.h"
#include "Legalizer/Legalizer.hpp"
#include "Legalizer/Data.hpp"
#include "Legalizer/ResultWriter.hpp"
#include "VerilogTest.h"
#include "DefWriter.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <climits>

using namespace std;

int main(int argc, char** argv) {
    auto& ap = ArgParser::get();
    ap.initialize(argc, argv);

    auto lef_files = ap.get_multiple_arguments("-lef");
    auto def_files = ap.get_multiple_arguments("-def");
    auto lib_files = ap.get_multiple_arguments("-lib");
    auto v_files = ap.get_multiple_arguments("-v");
    auto out_name = ap.get_argument("-out");

    if (lef_files.empty() || def_files.empty() || lib_files.empty() || v_files.empty()) {
        cerr << "Usage: " << argv[0]
             << " -lef <lef1> <lef2> ... -def <def1> <def2> ... -lib <lib1> ... -v <vfile> -out <prefix>\n";
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

        //---- 獲取 blockages ----
    auto& d = def::Def::get_instance();
    const auto& blockages = d.get_blockages();
    if(!blockages.empty()){
        int placement_count = 0;
        int layer_count = 0;
        for (const auto& blockage : blockages) {
            if (blockage->type_ == def::BlockageType::PLACEMENT) {
                placement_count++;
            } else {
                layer_count++;
                if (blockage->spacing_ > 0) {
                }
            }
        }
    }

    // === 讀取 .v ===
    cout << "Reading VERILOG file: " << v_files[0] << "\n";
    auto design = vparse::parse_verilog(v_files[0]);
    auto out_v = out_name + ".v";

    // === 讀取 .lib ===
    LibParser lib;

    // 若提供 .lib，讀進來覆寫/補充（可選）
    // for (const auto& libfile : lib_files) {
    //     cout << "Reading LIB file: " << libfile << "\n";
    //     if (!lib.loadLib(libfile)) {
    //         cerr << "[Error] Failed to load " << libfile << "\n";
    //         return 1;
    //     }
    // }


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
        "SNPSHOPT25_FSDN_V2_0P5",  // 1-bit FF
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
    // int bestK = chooseBestK(candidateKs, ff_copy, maps, lib);

    // ============ 正式跑 Banking ============
    my_lefdef::Banking banking(ff_copy, lib);
    banking.run_big(maps, 2.0, 2500.0, 2000.0, 10, true);
    const auto& groups = banking.getMBFFs();

    // === Legalize ===
    Input input;                     // 自動吃 last_banking_result
    Legalizer legalizer(&input);
    auto result = legalizer.solve(); // 得到結果 writer

    // ============ 處理 .v檔 ============
    std::vector<SimpleGroup> sgroups;
    for (const auto& g : groups) {
        SimpleGroup sg;

        // 去掉 hierachy
        auto pos = g.inst_name.find_last_of('/');
        if (pos != std::string::npos)
            sg.new_inst = g.inst_name.substr(pos + 1);
        else
            sg.new_inst = g.inst_name;

        sg.mbff_master = g.macro;

        for (auto* ff : g.bits) {
            std::string name = ff->name;
            auto p2 = name.find_last_of('/');
            if (p2 != std::string::npos) name = name.substr(p2 + 1);
            sg.members.push_back(name);
        }

        sgroups.push_back(std::move(sg));
    }
    write_banked_two_types(design, sgroups, out_v);
    std::cout << "Wrote " << out_v << "\n";

    // vparse_tools::dump_testcase2_verilog(design2, "tc2_verilog_dump.txt", /*max_per_module=*/100);

    // ============ 輸出 .list ============
    auto out_list = out_name + ".list";
    my_lefdef::writeListFile(banking, out_list, design);

    //修正ff的座標
    vector<my_lefdef::MBFFGroup>& Groups = banking.get_MBFFs();
    result->write_to_MbffGroup(&Groups);

    // write def
    VerilogParser parser;
    
    if (parser.parseFile(out_v)) {
        std::cout << "Successfully parsed Verilog file" << std::endl;
        
        // 打印網路連接（用於調試）
        parser.printNets();
        
        // 你可以在這裡使用解析結果來生成 DEF nets
        const auto& nets = parser.getNets();
        
        std::cout << "Total nets found: " << nets.size() << std::endl;
    }
    auto& ldp1 = my_lefdef::DefWriter::get_instance();
    auto &def_ = def::Def::get_instance();
    ldp1.write_def(def_,&Groups,parser,out_name+".def");
    return 0;
}



