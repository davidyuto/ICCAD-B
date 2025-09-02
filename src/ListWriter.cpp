
#include "ListWriter.h"
#include <fstream>
#include <iostream>

namespace my_lefdef {

void writeListFile(const Banking& banking,
                   const std::string& filename,
                   const vparse::VerilogDesign& design) {
    std::ofstream fout(filename);
    if (!fout) {
        std::cerr << "[Error] Cannot open " << filename << " for writing.\n";
        return;
    }

    // === Debug: 印出 module/hierarchy 狀況 (僅供檢查) ===
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
        // if (g.bits.size() == 1) continue;
        for (size_t i = 0; i < g.bits.size(); i++) {
            auto* ff = g.bits[i];

            // 找回 VerilogParser 的 FF instance（透過 leaf 名稱比對）
            const vparse::FFInstance* ffi = nullptr;
            auto get_leaf = [](const std::string& full) {
                size_t pos = full.find_last_of('/');
                return (pos == std::string::npos) ? full : full.substr(pos+1);
            };

            for (const auto& mod : design.modules) {
                for (const auto& inst : mod.ff_instances) {
                    if (inst.inst_name == get_leaf(ff->name)) {
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

                // 用 DEF 的完整 instance name
                fout << ff->name << "/" << pin
                    << " map " << g.inst_name << "/" << tgt_pin << "\n";
            }
        }
    }

    fout << "\n";

    // ================= Part 2: Operation Log =================
    fout << "OPERATION " << groups.size() << "\n";
    for (const auto& g : groups) {
        fout << "create_multibit { ";
        for (auto* ff : g.bits) {
            fout << "{" << ff->name
                 << " " << ff->macro << " 1} ";
        }
        fout << "{" << g.inst_name
             << " " << g.macro << " " << g.bits.size() << "} }\n";
    }

    fout.close();
    std::cout << "[Output] Wrote list file: " << filename << "\n";
}

} // namespace my_lefdef
