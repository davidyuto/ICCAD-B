#pragma once
#include "VerilogParser.h"
#include <string>
#include <vector>

struct SimpleGroup {
    std::string new_inst;                 // 目標 MBFF 的新實例名
    std::string mbff_master;              // 目標 MBFF 的 master 名
    std::vector<std::string> members;     // 被併的單 FF 實例名（有順序更好）
};

void write_banked_two_types(const vparse::VerilogDesign& design,
                            const std::vector<SimpleGroup>& groups,
                            const std::string& out_path);