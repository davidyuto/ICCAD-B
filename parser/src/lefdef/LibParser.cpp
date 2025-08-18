#include "LibParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// 從 values("...") 裡面取最大值
static double extractWorstValues(const std::string& line) {
    auto start = line.find("\"");
    auto end   = line.rfind("\"");
    double worst = 0.0;

    if (start != std::string::npos && end != std::string::npos && end > start) {
        std::string nums = line.substr(start + 1, end - start - 1);
        std::replace(nums.begin(), nums.end(), ',', ' ');
        std::istringstream iss(nums);
        double val;
        while (iss >> val) {
            worst = std::max(worst, val);
        }
    }
    return worst;
}

void LibParser::parseLib(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cerr << "Error: cannot open " << filename << "\n";
        return;
    }

    std::string line;
    FFCellInfo cur_cell;
    bool in_cell = false;
    bool found_ck_power = false;

    while (std::getline(fin, line)) {
        // trim 空白
        line.erase(line.begin(), std::find_if(line.begin(), line.end(),
                   [](unsigned char c){ return !std::isspace(c); }));
        line.erase(std::find_if(line.rbegin(), line.rend(),
                   [](unsigned char c){ return !std::isspace(c); }).base(), line.end());

        // ========== cell(...) ==========
        if (line.find("cell(") == 0) {
            in_cell = true;
            cur_cell = FFCellInfo();
            found_ck_power = false;

            auto start = line.find("(");
            auto end   = line.find(")");
            if (start != std::string::npos && end != std::string::npos) {
                cur_cell.name = line.substr(start + 1, end - start - 1);
            }
        }
        else if (in_cell && line.find("area") == 0) {
            auto pos = line.find(":");
            if (pos != std::string::npos) {
                cur_cell.area = std::stod(line.substr(pos+1));
            }
        }

        // ========== pin(Q) ==========
        else if (in_cell && line.find("pin(") == 0 && line.find("Q") != std::string::npos) {
            while (std::getline(fin, line)) {
                if (line.find("timing()") != std::string::npos) {
                    bool related_ck = false;
                    while (std::getline(fin, line)) {
                        if (line.find("related_pin") != std::string::npos &&
                            line.find("CK") != std::string::npos) {
                            related_ck = true;
                        }
                        if (related_ck &&
                            (line.find("cell_rise") != std::string::npos || line.find("cell_fall") != std::string::npos)) {
                            while (std::getline(fin, line)) {
                                if (line.find("values") != std::string::npos) {
                                    double w = extractWorstValues(line);
                                    cur_cell.cq_delay = std::max(cur_cell.cq_delay, w);
                                    std::cout << "[Debug] Cell " << cur_cell.name
                                              << " found CK->Q delay value = " << w << "\n";
                                    break;
                                }
                            }
                        }
                        if (line.find("}") != std::string::npos) break;
                    }
                }
                if (line.find("}") != std::string::npos) break;
            }
        }

        // ========== pin(CK) ==========
        else if (in_cell && line.find("pin(CK)") != std::string::npos) {
            bool in_ck_pin = true;
            std::cout << "[Debug] Found pin(CK) for cell " << cur_cell.name << "\n";
            while (in_ck_pin && std::getline(fin, line)) {
                if (line.find("internal_power") != std::string::npos) {
                    std::string when_cond = "";
                    double local_worst = 0.0;
                    while (std::getline(fin, line)) {
                        if (line.find("when") != std::string::npos) {
                            auto pos = line.find(":");
                            when_cond = line.substr(pos+1);
                            when_cond.erase(std::remove(when_cond.begin(), when_cond.end(), '\"'),
                                            when_cond.end());
                        }
                        if (line.find("values") != std::string::npos) {
                            double w = extractWorstValues(line);
                            local_worst = std::max(local_worst, w);
                        }
                        if (line.find("}") != std::string::npos) break; // end internal_power
                    }
                    if (local_worst > 0.0) {
                        cur_cell.cq_power = std::max(cur_cell.cq_power, local_worst);
                        std::cout << "[Debug] Cell " << cur_cell.name 
                                << " CK internal_power when=" << when_cond
                                << " worst=" << local_worst << "\n";
                    }
                }
                if (line.find("}") != std::string::npos) {
                    in_ck_pin = false; // end of pin(CK) block
                }
            }
        }


        // ========== 結束 cell ==========
        else if (in_cell && line.find("}") != std::string::npos) {
            if (!found_ck_power) {
                std::cout << "[Debug] Cell " << cur_cell.name
                          << " has NO CK internal power found!\n";
            }
            ff_cells[cur_cell.name] = cur_cell;
            in_cell = false;
        }
    }
}

void LibParser::debugPrint() const {
    for (auto& kv : ff_cells) {
        std::cout << "Cell " << kv.first
                  << " | Area = " << kv.second.area
                  << " | CQ Delay = " << kv.second.cq_delay
                  << " | CQ Power = " << kv.second.cq_power
                  << "\n";
    }
}
