/**
 * @file    LefDefParser.cpp
 * @author  Jinwook Jung (jinwookjung@kaist.ac.kr)
 * @date    2018-10-18 10:40:18
 *
 * Extended to extract single-bit FF and multi-bit FF (MBFF) from LEF/DEF.
 */

#include "LefDefParser.h"
#include "StringUtil.h"
#include "Watch.h"
#include "Logger.h"
#include "Lef.h"
#include "Def.h"
#include "FFSet.h"
#include <unordered_map>
#include <cctype>
#include <cassert>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iostream>



constexpr int MAX_NEIGHBORS = 50;
constexpr int BANDWIDTH_SELECTION_NEIGHBOR = 15;
constexpr double MAX_SQUARE_DISPLACEMENT = 2000 * 2000;
constexpr double MAX_BANDWIDTH = 1000;
constexpr double SHIFT_TOLERANCE = 1e-2;

namespace my_lefdef
{

//------------------------------------------------------------------------------
// Static helper functions for FF classification
//------------------------------------------------------------------------------


bool is_main_Q_pin(const std::string& name) {

    if (name.size() >= 2 && name[0] == 'Q' && std::isdigit(name[1])) {
        cout << "Q-pin name: " << name << endl;
        return true;
    }
        
    else if (name == "Q")
        return true;
    else return false;
}

int LefDefParser::countQpins(const lef::MacroPtr &m) {
    int cnt = 0;
    for (const auto& kv : m->pin_umap_) {
        const auto& pin_name = kv.first;
        // cout << "PIN's name:" << pin_name << endl;
        const auto& pin = kv.second;

        if (pin->dir_ != PinDir::output) continue;

        if (is_main_Q_pin(pin_name)) {
            ++cnt;
        }
    }
    return cnt;
}




// bool LefDefParser::isMultiBitMacro(const lef::MacroPtr &m) {
//     return countQpins(m) > 1;
// }

// bool LefDefParser::isSingleBitMacro(const lef::MacroPtr &m) {
//     if (isMultiBitMacro(m)) return false;
//     bool hasQ   = (countQpins(m) == 1);
//     bool hasD   = (m->pin_umap_.count("D") > 0);
//     bool hasCK  = (m->pin_umap_.count("CLK") > 0) || (m->pin_umap_.count("CK") > 0);
//     return hasQ && hasD && hasCK;
// }
// 修改點：原本是用 pin 判斷，改成集合判斷
bool LefDefParser::isSingleBitMacro(const lef::MacroPtr &m) {
    return single_ff_set().count(m->name_) > 0;
}

bool LefDefParser::isMultiBitMacro(const lef::MacroPtr &m) {
    return multi_ff_set().count(m->name_) > 0;
}


std::string LefDefParser::extractGroupName(const std::string &compName) {
    auto pos = compName.find_last_of('_');
    if (pos != std::string::npos)
        return compName.substr(0, pos);
    return compName;
}

//------------------------------------------------------------------------------
// LefDefParser implementation
//------------------------------------------------------------------------------

LefDefParser::LefDefParser()
    : lef_(lef::Lef::get_instance()),
      def_(def::Def::get_instance())
{
}

LefDefParser& LefDefParser::get_instance() {
    static LefDefParser instance;
    return instance;
}

void LefDefParser::read_lef(const std::string &filename) {
    lef_.read_lef(filename);
    // lef_.report();
}

void LefDefParser::read_def(const std::string &filename) {
    def_.read_def(filename);
    // def_.report();
    // After DEF parse, extract FF and MBFF
    extractFlipFlops();
}

void LefDefParser::extractFlipFlops() {
    ffs_.clear();
    mbffs_.clear();
    std::unordered_map<std::string, MBFF> tmp;

    auto &comps = def_.get_component_umap();
    for (auto const &kv : comps) {
        auto comp = kv.second;
        auto macro = comp->lef_macro_;
        if (!macro) continue;

        // 判斷 Macro 屬性
        if (isMultiBitMacro(macro)) {
            auto group = extractGroupName(comp->name_);
            auto &mb = tmp[group];
            mb.group = group;
            mb.bits.push_back({ comp->name_, comp->x_, comp->y_ });
        }
        else if (isSingleBitMacro(macro)) {
            ffs_.push_back({
                comp->name_,
                comp->x_,
                comp->y_,
                macro->name_,
                macro->size_x_,
                macro->size_y_
            });
            // std::cout << "Found single-bit FF: " << comp->name_ << " of macro " << macro->name_ << std::endl;
        }
        // else {
        //     std::cout << "Non-FF macro: " << macro->name_ << std::endl;
        // }
    }


    
    mbffs_.reserve(tmp.size());
    for (auto &kv : tmp)
        mbffs_.push_back(std::move(kv.second));

    for (auto const &kv : comps) {
        auto comp = kv.second;
        auto macro = comp->lef_macro_;
        if (!macro) continue;
    }
    
}
def::Def& LefDefParser::get_def() {
    return def_;
}
InternalNetlist LefDefParser::extractNetlist() const {
    InternalNetlist netlist;
    const auto& net_umap = def_.get_net_umap();

    for (const auto& [net_name, net_ptr] : net_umap) {
        NetlistNet net;
        net.name = net_name;

        for (const auto& conn : net_ptr->connections_) {
            NetConnection nc;
            if (conn->component_) {
                nc.instance = conn->component_->name_;
                nc.pin = conn->lef_pin_ ? conn->lef_pin_->name_ : conn->name_;
            } else if (conn->pin_) {
                nc.instance = conn->pin_->name_;  // IO pin name
                nc.pin = conn->name_;
            }
            net.connections.push_back(nc);
        }

        netlist.nets.emplace(net_name, std::move(net));
    }

    return netlist;
}
void LefDefParser::fillFlipFlopNets() {
    const auto& netlist = extractNetlist();  // 使用你已有的 Netlist
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> instance_pin_to_net;

    for (const auto& [net_name, net] : netlist.nets) {
        for (const auto& conn : net.connections) {
            instance_pin_to_net[conn.instance][conn.pin] = net_name;
        }
    }

    auto fill = [&](FlipFlop& ff) {
        const auto& inst_name = ff.name;
        const auto it = instance_pin_to_net.find(inst_name);
        if (it == instance_pin_to_net.end()) return;

        const auto& pin_map = it->second;
        for (const auto& [pin, net] : pin_map) {
            std::string p = StringUtil::to_upper(pin);
            if (p == "D" || p.find("D") == 0) {
                ff.fanin_net = net;
            } else if (p == "Q" || p.find("Q") == 0) {
                ff.fanout_net = net;
            } else if (p == "CK" ) {
                ff.clk_net = net;
            }
        }
    };

    for (auto& ff : ffs_) {
        fill(ff);
    }

    for (auto& mbff : mbffs_) {
        for (auto& bit : mbff.bits) {
            fill(bit);
        }
    }
}


} // namespace my_lefdef
