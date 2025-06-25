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
#include "PlacementStructure.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

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

    // 打印单比特 FF 列表
    const auto& ffs = ldp.getFFs();
    const auto& mbffs = ldp.getMBFFs();
    const auto& comps = ldp.get_def().get_component_umap();

    cout << "\nSummary of FF classification:\n";
    cout << "  Single-bit FF count : " << ffs.size() << "\n";
    cout << "  Multi-bit FF groups : " << mbffs.size() << "\n";

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

    cout << "\n[Sample] First up to 10 MBFF bits:\n";
    size_t count = 0;
    for (const auto& mb : mbffs) {
        for (const auto& bit : mb.bits) {
            if (count++ >= 10) break;
            auto it = comps.find(bit.name);
            std::string macro = (it != comps.end() && it->second->lef_macro_) 
                ? it->second->lef_macro_->name_ : "UNKNOWN";
            cout << "  " << bit.name << " @ (" << bit.x << "," << bit.y << ")"
                << "   [Group: " << mb.group << ", Macro: " << macro << "]\n";
        }
        if (count >= 10) break;
    }


    return 0;
}
