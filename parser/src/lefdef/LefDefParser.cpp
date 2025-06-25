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

#include <unordered_map>
#include <cctype>
#include <cassert>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iostream>

namespace my_lefdef
{

//------------------------------------------------------------------------------
// Static helper functions for FF classification
//------------------------------------------------------------------------------

// int LefDefParser::countQpins(const lef::MacroPtr &m) {
//     int cnt = 0;
//     for (auto const &kv : m->pin_umap_) {
//         auto const &p = kv.first;
//         if ((p.size() >= 2 && p[0]=='Q' && std::isdigit((unsigned char)p[1])) 
//             || p == "Q") {
//             ++cnt;
//         }
//     }
//     return cnt;
// }


int LefDefParser::countQpins(const lef::MacroPtr &m) {
    int cnt = 0;
    for (const auto& kv : m->pin_umap_) {
        const std::string& pin_name = kv.first;
        auto pin = kv.second;
        if ((pin_name.find("Q") != std::string::npos) &&
            (pin->dir_ == PinDir::output)) {
            ++cnt;
        }
    }
    return cnt;
}



bool LefDefParser::isMultiBitMacro(const lef::MacroPtr &m) {
    return countQpins(m) > 1;
}

bool LefDefParser::isSingleBitMacro(const lef::MacroPtr &m) {
    if (isMultiBitMacro(m)) return false;
    bool hasQ   = (countQpins(m) == 1);
    bool hasD   = (m->pin_umap_.count("D") > 0);
    bool hasCK  = (m->pin_umap_.count("CLK") > 0) || (m->pin_umap_.count("CK") > 0);
    return hasQ && hasD && hasCK;
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
    lef_.report();
}

void LefDefParser::read_def(const std::string &filename) {
    def_.read_def(filename);
    def_.report();
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
        // Classify macro
        if (isMultiBitMacro(macro)) {
            auto group = extractGroupName(comp->name_);
            auto &mb = tmp[group];
            mb.group = group;
            mb.bits.push_back({ comp->name_, comp->x_, comp->y_ });
        }
        else if (isSingleBitMacro(macro)) {
            ffs_.push_back({ comp->name_, comp->x_, comp->y_ });
        }
    }
    mbffs_.reserve(tmp.size());
    for (auto &kv : tmp)
        mbffs_.push_back(std::move(kv.second));

    for (auto const &kv : comps) {
    auto comp = kv.second;
    auto macro = comp->lef_macro_;
    if (!macro) continue;

    cout << "[DBG] Component: " << comp->name_
         << ", Macro: " << macro->name_
         << ", Q-pin-count: " << countQpins(macro) << endl;
}

}

//------------------------------------------------------------------------------
// Bookshelf writers (unchanged)
//------------------------------------------------------------------------------

static std::string get_current_time_stamp() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%m/%d/%Y %H:%M:%S");
    return oss.str();
}

void LefDefParser::write_bookshelf(const std::string &filename) const {
    std::cout << "Writing bookshelf aux file." << std::endl;
    std::ofstream ofs(filename + ".aux");
    ofs << "RowBasedPlacement : "
        << " " << filename << ".nodes"
        << " " << filename << ".nets"
        << " " << filename << ".wts"
        << " " << filename << ".pl"
        << " " << filename << ".scl"
        << " " << filename << ".shapes" << std::endl;
    ofs.close();

    std::cout << "Writing bookshelf nodes file." << std::endl;
    write_bookshelf_nodes(filename + ".nodes");

    std::cout << "Writing bookshelf nets file." << std::endl;
    write_bookshelf_nets(filename + ".nets");

    std::cout << "Writing bookshelf wts file." << std::endl;
    write_bookshelf_wts(filename + ".wts");

    std::cout << "Writing bookshelf scl file." << std::endl;
    write_bookshelf_scl(filename + ".scl");

    std::cout << "Writing bookshelf pl file." << std::endl;
    write_bookshelf_pl(filename + ".pl");

    std::cout << "Writing bookshelf aux file." << std::endl;
}

void LefDefParser::write_bookshelf_nodes(const std::string &filename) const {
    std::ofstream ofs(filename);
    ofs << "UCLA nodes 1.0" << std::endl;
    ofs << "# Created : " << get_current_time_stamp() << std::endl;

    auto& component_umap = def_.get_component_umap();
    auto& pin_umap = def_.get_pin_umap();

    const auto x_pitch = lef_.get_min_x_pitch();
    const auto y_pitch = lef_.get_min_y_pitch();

    size_t num_terminals = pin_umap.size();
    for (auto const& c : component_umap) {
        if (c.second->is_fixed_) {
            ++num_terminals;
        }
    }

    ofs << "NumNodes : " << component_umap.size() + pin_umap.size() << std::endl;
    ofs << "NumTerminals : " << num_terminals << std::endl;

    for (auto const& p : pin_umap) {
        ofs << "\t" << std::setw(40) << std::left  << p.first
            << "\t" << std::setw(8)  << std::right << 1
            << "\t" << std::setw(8)  << std::right << 1
            << "\t" << "terminal" << std::endl;
    }

    for (auto const& c : component_umap) {
        ofs << "\t" << std::setw(40) << std::left << c.first;

        auto macro = c.second->lef_macro_;
        auto w = lround(macro->size_x_ / x_pitch);
        auto h = lround(macro->size_y_ / y_pitch);

        ofs << "\t" << std::setw(8) << std::right << w
            << "\t" << std::setw(8) << std::right << h;

        if (c.second->is_fixed_) {
            ofs << "\t" << "terminal";
        }
        ofs << std::endl;
    }
    ofs.close();
}

void LefDefParser::write_bookshelf_nets(const std::string &filename) const {
    std::ofstream ofs(filename);
    ofs << "UCLA nets 1.0" << std::endl;
    ofs << "# Created : " << get_current_time_stamp() << "\n\n";

    auto& net_umap = def_.get_net_umap();
    size_t num_connections = 0;

    for (auto const& n : net_umap)
        num_connections += n.second->connections_.size();

    ofs << "NumNets : " << net_umap.size() << std::endl;
    ofs << "NumPins : " << num_connections << std::endl << std::endl;

    for (auto const& n : net_umap) {
        auto net = n.second;
        ofs << "NetDegree : " << std::setw(8) << std::right
            << net->connections_.size() << "\t" << n.first << std::endl;

        for (auto const& c : net->connections_) {
            std::string name;
            PinDir direction;
            if (c->lef_pin_ == nullptr) {
                name = c->name_;
                direction = c->pin_->dir_;
            } else {
                name = c->component_->name_;
                direction = c->lef_pin_->dir_;
            }

            ofs << "\t" << std::setw(20) << std::left << name;
            ofs << (direction == PinDir::input ? " I  :" : " O  :");
            ofs << " 0.5 0.5" << std::endl;
        }
    }
    ofs.close();
}

void LefDefParser::write_bookshelf_wts(const std::string &filename) const {
    std::ofstream ofs(filename);
    ofs << "UCLA wts 1.0" << std::endl;
    ofs << "# Created : " << get_current_time_stamp() << "\n\n";

    auto& net_umap = def_.get_net_umap();
    for (auto const& n : net_umap)
        ofs << std::setw(40) << std::left << n.first << "\t1" << std::endl;

    ofs.close();
}

void LefDefParser::write_bookshelf_scl(const std::string &filename) const {
    std::ofstream ofs(filename);
    ofs << "UCLA scl 1.0" << std::endl;
    ofs << "# Created : " << get_current_time_stamp() << "\n\n";

    auto& rows = def_.get_rows();

    auto x_pitch_dbu = lef_.get_min_x_pitch_dbu();
    auto y_pitch_dbu = lef_.get_min_y_pitch_dbu();
    auto x_pitch = lef_.get_min_x_pitch();
    auto y_pitch = lef_.get_min_y_pitch();

    ofs << "NumRows : " << rows.size() << std::endl << std::endl;

    for (auto const& r : rows) {
        auto site = lef_.get_site(r->macro_);
        std::string sym_str;
        switch (site->symmetry_) {
            case SiteSymmetry::x:   sym_str = "X"; break;
            case SiteSymmetry::y:   sym_str = "Y"; break;
            case SiteSymmetry::r90: sym_str = "R90"; break;
            default:                sym_str = "Y"; break;
        }

        ofs << "CoreRow Horizontal" << std::endl;
        ofs << "\tCoordinate   : " << r->y_ / y_pitch_dbu << std::endl;
        ofs << "\tHeight       : " << site->y_ / y_pitch << std::endl;
        ofs << "\tSitewidth    : " << site->x_ / x_pitch << std::endl;
        ofs << "\tSitespacing  : " << r->step_x_ / x_pitch_dbu << std::endl;
        ofs << "\tSiteorient   : " << r->orient_str_ << std::endl;
        ofs << "\tSitesymmetry : " << sym_str << std::endl;
        ofs << "\tSubrowOrigin : " << r->x_ / x_pitch_dbu
            << "\tNumSites : " << r->num_x_ << std::endl;
        ofs << "End" << std::endl;
    }
    ofs.close();
}

void LefDefParser::write_bookshelf_pl(const std::string &filename) const {
    std::ofstream ofs(filename);
    ofs << "UCLA pl 1.0" << std::endl;
    ofs << "# Created : " << get_current_time_stamp() << std::endl << std::endl;

    auto& component_umap = def_.get_component_umap();
    auto& pin_umap       = def_.get_pin_umap();

    auto x_pitch_dbu = lef_.get_min_x_pitch_dbu();
    auto y_pitch_dbu = lef_.get_min_y_pitch_dbu();

    for (auto const& it : component_umap) {
        auto c = it.second;
        ofs << std::setw(40) << std::left << c->name_;
        if (c->is_placed_ || c->is_fixed_) {
            ofs << "\t" << c->x_ / x_pitch_dbu
                << "\t" << c->y_ / y_pitch_dbu
                << "\t: " << c->orient_str_ << std::endl;
        } else {
            ofs << "\t0\t0\t: N" << std::endl;
        }
    }
    for (auto const& it : pin_umap) {
        auto p = it.second;
        ofs << std::setw(40) << std::left << p->name_
            << "\t" << p->x_ / x_pitch_dbu
            << "\t" << p->y_ / y_pitch_dbu
            << "\t: " << p->orient_str_ << std::endl;
    }
    ofs.close();
}

void LefDefParser::update_def(const std::string &bookshelf_pl) {
    std::unordered_map<std::string, std::pair<int,int>> pl_umap;
    std::cout << "Reading bookshelf pl file..." << std::endl;
    std::ifstream ifs(bookshelf_pl);
    if (!ifs.is_open()) {
        throw std::invalid_argument("Cannot open " + bookshelf_pl);
    }
    std::string line;
    std::getline(ifs, line);
    assert(line == "UCLA pl 1.0");
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string name; int px, py;
        iss >> name >> px >> py;
        pl_umap[name] = {px, py};
    }
    std::cout << "Updating DEF file..." << std::endl;
    auto& component_umap = def_.get_component_umap();
    auto x_pitch_dbu = lef_.get_min_x_pitch_dbu();
    auto y_pitch_dbu = lef_.get_min_y_pitch_dbu();
    for (auto const& it : component_umap) {
        auto found = pl_umap.find(it.first);
        if (found == pl_umap.end()) {
            std::cerr << "Error: " << it.first << " not found in .pl." << std::endl;
            continue;
        }
        if (!it.second->is_fixed_) {
            int x_new = found->second.first  * x_pitch_dbu;
            int y_new = found->second.second * y_pitch_dbu;
            it.second->x_ = x_new;
            it.second->y_ = y_new;
            it.second->is_placed_ = true;
        }
    }
}

def::Def& LefDefParser::get_def() {
    return def_;
}

} // namespace my_lefdef
