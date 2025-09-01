#pragma once
#include "Banking.h"
#include "VerilogParser.h"
#include <string>

namespace my_lefdef {

// 獨立的 helper 函式，不是 Banking 成員
void writeListFile(const Banking& banking,
                   const std::string& filename,
                   const vparse::VerilogDesign& design);

} // namespace my_lefdef
