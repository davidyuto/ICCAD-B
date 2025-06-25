/**
 * @file    main.cpp
 * @author  Jinwook Jung (jinwookjung@kaist.ac.kr)
 * @date    2017-12-23 22:12:10
 *
 * Modified to support multiple LEF inputs.
 */

#include "Logger.h"
#include "Watch.h"
#include "ArgParser.h"
#include "LefDefParser.h"
#include "PlacementStructure.h"

#include <iostream>
#include <sstream>    // for istringstream
#include <string>

using namespace std;

void show_usage ();
void show_banner ();
void show_cmd_args ();

#ifndef UNIT_TEST

int main (int argc, char* argv[])
{
    util::Watch watch;

    // 1. 解析命令列
    auto& ap = ArgParser::get();
    ap.initialize(argc, argv);

    // 支援多個 LEF 檔案，用逗號分隔
    auto filename_lef_list      = ap.get_argument("--lef");
    auto filename_def           = ap.get_argument("--def");
    auto filename_bookshelf     = ap.get_argument("--bookshelf");

    // 2. 參數檢查
    if (filename_lef_list.empty() || filename_def.empty()) {
        show_usage();
        return -1;
    }
    if (filename_bookshelf.empty()) {
        filename_bookshelf = "out";
    }

    // 3. 顯示執行資訊
    show_banner();
    show_cmd_args();

    // 4. 取得 parser
    auto& ldp = my_lefdef::LefDefParser::get_instance();

    // 5. 依序讀入各個 LEF
    istringstream iss(filename_lef_list);
    string lef_file;
    while (getline(iss, lef_file, ',')) {
        if (lef_file.empty()) continue;
        cout << "Reading LEF: " << lef_file << endl;
        ldp.read_lef(lef_file);
    }

    // 6. 讀取 DEF，並印 summary
    ldp.read_def(filename_def);

    // 7. 輸出 bookshelf 格式
    // ldp.write_bookshelf(filename_bookshelf);

    auto rows = extractRowInfos();
    std::cout << "Total physical rows: " << rows.size() << "\n";
    for (auto const& r : rows) {
        std::cout << "Row@Y=" << r.y
                << " origX=" << r.orig_x
                << " count=" << r.num_sites
                << " pitch=" << r.site_step << "\n";
    }

    cout << endl << "Done." << endl;
    return 0;
}

void show_usage ()
{
    cout << endl;
    cout << "Usage:" << endl;
    cout << "  bookshelf_writer --lef <lef1[,lef2,...]> --def <def> [--bookshelf <prefix>]" << endl << endl;
}

void show_banner ()
{
    cout << endl;
    cout << string(79, '=') << endl;
    cout << "LEF/DEF Parser" << endl;
    cout << "Author: Jinwook Jung" << endl;
    cout << string(79, '=') << endl;
}

void show_cmd_args ()
{
    auto& ap = ArgParser::get();
    cout << "  LEF file(s): " << ap.get_argument("--lef") << endl;
    cout << "  DEF file   : " << ap.get_argument("--def") << endl;
    cout << "  Bookshelf  : " << (ap.get_argument("--bookshelf").empty() ? "out" : ap.get_argument("--bookshelf")) << endl;
}

#else

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Simple testcases
#include <boost/test/unit_test.hpp>

#endif
