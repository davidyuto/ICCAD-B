/**
 * @file    main.cpp
 * @author  Jinwook Jung (jinwookjung@kaist.ac.kr)
 * @date    2017-12-23 22:12:10
 *
 * Modified: Removed Bookshelf output for CAD contest.
 */

#include "Logger.h"
#include "Watch.h"
#include "ArgParser.h"
#include "LefDefParser.h"
// #include "PlaceStructure.h"
#include "PlacementStructure.h"

#include <iostream>
#include <sstream>
#include <string>

using namespace std;

void show_usage();
void show_banner();
void show_cmd_args();

#ifndef UNIT_TEST

int main(int argc, char* argv[])
{
    util::Watch watch;

    // 1. Parse command line
    auto& ap = ArgParser::get();
    ap.initialize(argc, argv);

    // LEF list (comma-separated) and DEF file
    auto filename_lef_list = ap.get_argument("--lef");
    auto filename_def      = ap.get_argument("--def");

    // 2. Check arguments
    if (filename_lef_list.empty() || filename_def.empty()) {
        show_usage();
        return -1;
    }

    // 3. Show info
    show_banner();
    show_cmd_args();

    // 4. Get parser instance
    auto& ldp = my_lefdef::LefDefParser::get_instance();

    // 5. Read all LEF files
    {
        istringstream iss(filename_lef_list);
        string lef_file;
        while (getline(iss, lef_file, ',')) {
            if (lef_file.empty()) continue;
            cout << "Reading LEF: " << lef_file << endl;
            ldp.read_lef(lef_file);
        }
    }

    // 6. Read DEF file
    ldp.read_def(filename_def);

    cout << endl << "Parsing complete." << endl;
    auto rows = extractRowInfos();
    std::cout << "Total physical rows: " << rows.size() << "\n";
    for (int i = 0; i < std::min<size_t>(rows.size(), 10); ++i) {
    auto &r = rows[i];
    std::cout << "Row@Y=" << r.y
            << " origX=" << r.orig_x
            << " count=" << r.num_sites
            << " pitch=" << r.site_step << "\n";
}

    return 0;
}

void show_usage()
{
    cout << endl;
    cout << "Usage:" << endl;
    cout << "  LefDefParser --lef <lef1[,lef2,...]> --def <def>" << endl << endl;
}

void show_banner()
{
    cout << string(79, '=') << endl;
    cout << "LEF/DEF Parser" << endl;
    cout << string(79, '=') << endl;
}

void show_cmd_args()
{
    auto& ap = ArgParser::get();
    cout << "  LEF file(s): " << ap.get_argument("--lef") << endl;
    cout << "  DEF file   : " << ap.get_argument("--def") << endl;
}

#else

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Simple testcases
#include <boost/test/unit_test.hpp>

#endif
