#include "ListWriter.h"
#include <fstream>
#include <iostream>

namespace my_lefdef {

// === Helper: 取得 instance 的完整名字 (加上 module prefix，如果需要) ===
static std::string getHierName(const vparse::VerilogDesign& design,
                               const std::string& inst_name) {
    std::string module_name;

    // 搜尋 instance 屬於哪個 module
    for (const auto& mod : design.modules) {
        for (const auto& inst : mod.ff_instances) {
            if (inst.inst_name == inst_name) {
                module_name = mod.name;
                break;
            }
        }
        if (!module_name.empty()) break;
    }

    // 如果設計只有一個 module，或找不到 → 不加 prefix
    if (design.modules.size() == 1 || module_name.empty()) {
        return inst_name;
    }

    // 如果有多個 module → 加上 module prefix
    return module_name + "/" + inst_name;
}

void writeListFile(const Banking& banking,
                   const std::string& filename,
                   const vparse::VerilogDesign& design) {
    std::ofstream fout(filename);
    if (!fout) {
        std::cerr << "[Error] Cannot open " << filename << " for writing.\n";
        return;
    }

    // === Debug: 印出 module/hierarchy 狀況 ===
    std::cout << "[Debug] VerilogDesign contains " 
              << design.modules.size() << " module(s)\n";
    for (const auto& mod : design.modules) {
        std::cout << "   - Module " << mod.name 
                  << " with " << mod.ff_instances.size() << " FFs\n";
    }
    std::cout << "========================================\n";

    const auto& groups = banking.getMBFFs();

    // ================= Part 1: Pin Mapping =================
    fout << "CellInst " << groups.size() << "\n";

    for (const auto& g : groups) {
        for (size_t i = 0; i < g.bits.size(); i++) {
            auto* ff = g.bits[i];

            // 找回原始的 FF instance
            const vparse::FFInstance* ffi = nullptr;
            for (const auto& mod : design.modules) {
                for (const auto& inst : mod.ff_instances) {
                    if (inst.inst_name == ff->name) {
                        ffi = &inst;
                        break;
                    }
                }
                if (ffi) break;
            }
            if (!ffi) continue;

            // 對來源 FF 的每個 pin 做 mapping
            for (auto& [pin, net] : ffi->pin2net) {
                std::string tgt_pin;

                if (pin == "D")        tgt_pin = "D"  + std::to_string(i);
                else if (pin == "Q")   tgt_pin = "Q"  + std::to_string(i);
                else if (pin == "QN")  tgt_pin = "QN" + std::to_string(i);
                else if (pin == "CK" || pin == "CLK") tgt_pin = "CK";
                else if (pin == "SE" || pin == "SI" ||
                         pin == "RESET" || pin == "SET")
                    tgt_pin = pin;
                else
                    continue; // 其他暫時略過

                fout << getHierName(design, ff->name) << "/" << pin
                     << " map " << getHierName(design, g.inst_name) 
                     << "/" << tgt_pin << "\n";
            }
        }
    }
    fout << "\n";

    // ================= Part 2: Operation Log =================
    fout << "OPERATION " << groups.size() << "\n";
    for (const auto& g : groups) {
        fout << "create_multibit { ";
        for (auto* ff : g.bits) {
            fout << "{" << getHierName(design, ff->name)
                 << " " << ff->macro << " 1} ";
        }
        fout << "{" << getHierName(design, g.inst_name)
             << " " << g.macro << " " << g.bits.size() << "} }\n";
    }

    fout.close();
    std::cout << "[Output] Wrote list file: " << filename << "\n";
}


} // namespace my_lefdef
