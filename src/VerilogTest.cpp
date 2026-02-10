#include "VerilogTest.h"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace vparse_tools {

static std::string hier_name(const vparse::VerilogDesign& design,
                             const std::string& module_name,
                             const std::string& inst_name) {
    // testcase2: 多 module 就加 "<module>/<inst>"；只有一個 module 就只用 inst
    if (design.modules.size() <= 1) return inst_name;
    return module_name + "/" + inst_name;
}

void dump_testcase2_verilog(const vparse::VerilogDesign& design,
                            const std::string& out_path,
                            std::size_t max_per_module) {
    std::ofstream fout(out_path);
    if (!fout) {
        std::cerr << "[Error] Cannot open dump file: " << out_path << "\n";
        return;
    }

    // --- 概覽 ---
    fout << "=== VerilogDesign Overview ===\n";
    fout << "Module count: " << design.modules.size() << "\n";
    for (const auto& m : design.modules) {
        fout << "  - " << m.name << " (FF count = " << m.ff_instances.size() << ")\n";
    }
    fout << "\n";

    // 逐 module 輸出
    for (const auto& m : design.modules) {
        fout << "=== Module: " << m.name << " ===\n";
        fout << "Total FFs: " << m.ff_instances.size() << "\n";

        // 依 cell_macro 統計
        std::unordered_map<std::string, std::size_t> by_macro;
        by_macro.reserve(m.ff_instances.size());
        for (const auto& ffi : m.ff_instances) {
            by_macro[ffi.cell_macro] += 1;
        }

        // 排序後輸出（數量多的在前）
        std::vector<std::pair<std::string, std::size_t>> macro_vec(by_macro.begin(), by_macro.end());
        std::sort(macro_vec.begin(), macro_vec.end(),
                  [](const auto& a, const auto& b){ return a.second > b.second; });

        fout << "By cell_macro:\n";
        for (auto& kv : macro_vec) {
            fout << "  * " << kv.first << " : " << kv.second << "\n";
        }
        fout << "\n";

        // 範例列出 FF 詳細內容
        std::size_t limit = (max_per_module == 0 ? m.ff_instances.size() : std::min(max_per_module, m.ff_instances.size()));
        fout << "Sample FF details (up to " << limit << "):\n";
        for (std::size_t i = 0; i < limit; ++i) {
            const auto& ffi = m.ff_instances[i];
            fout << "- inst: " << hier_name(design, m.name, ffi.inst_name)
                 << "   macro: " << ffi.cell_macro << "\n";
            fout << "  pins:\n";
            for (const auto& kv : ffi.pin2net) {
                fout << "    ." << kv.first << " -> " << kv.second << "\n";
            }
        }
        fout << "\n";
    }

    // 額外提示：是否偵測到 hierarchy（多 module）
    fout << "=== Notes ===\n";
    if (design.modules.size() > 1) {
        fout << "- Multiple modules detected. Instance names above are prefixed with <module>/.\n";
    } else {
        fout << "- Single module detected. Instance names are shown without module prefix.\n";
    }
    fout << "- This dump is generated purely from the .v parse; it does not require .lib.\n";

    fout.close();
    std::cout << "[Dump] Wrote testcase2 verilog parse to " << out_path << "\n";
}

} // namespace vparse_tools
