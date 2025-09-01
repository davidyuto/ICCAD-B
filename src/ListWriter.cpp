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

    // 用 banking.getMBFFs() 拿 mbff_groups_
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

            // Pin mapping
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
                    continue;

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
            fout << "{" << ff->name << " " << ff->macro << " 1} ";
        }
        fout << "{" << g.inst_name << " " << g.macro
             << " " << g.bits.size() << "} }\n";
    }

    fout.close();
    std::cout << "[Output] Wrote list file: " << filename << "\n";
}

} // namespace my_lefdef
