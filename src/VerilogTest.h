#pragma once
#include "VerilogParser.h"
#include <string>
#include <cstddef>

namespace vparse_tools {

// 專門用來「檢視」testcase2 的 .v 解析結果（不需要 .lib）。
// - design: 你用 VerilogParser 解析後的 VerilogDesign
// - out_path: 輸出路徑（txt）
// - max_per_module: 每個 module 最多輸出多少顆 FF 詳細內容（0=全部）
//
// 會輸出：
// 1) 總共有幾個 module（列出名稱）
// 2) 每個 module 的 FF 總數、依 cell_macro 的統計
// 3) 每個 module 範例 FF（含 inst_name、cell_macro、pin 列表）
// 4) 自動在輸出中加上 "<module>/<inst>" 的層級前綴（若 module>1）
void dump_testcase2_verilog(const vparse::VerilogDesign& design,
                            const std::string& out_path,
                            std::size_t max_per_module = 50);

} // namespace vparse_tools
