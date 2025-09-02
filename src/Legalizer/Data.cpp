
#include "Data.hpp"
#include "iostream"
#include <cmath>
#include "../Banking.h"   // ★ 引入 Banking，取 mbff_groups_

Cell::Cell() : width(0), height(0), x(0), y(0), weight(0), optimalX(0), optimalY(0) {}

Cell::Cell(const std::string &name, double width, double height, int x, int y)
    : name(name), width(width), height(height), x(x), y(y), weight(1), optimalX(0), optimalY(0) {}

double Cell::displacement() const {
    double diffX = optimalX - x;
    double diffY = optimalY - y;
    return std::sqrt(diffX * diffX + diffY * diffY);
}

Cluster::Cluster() : x(0), weight(0), q(0), width(0), predecessor(nullptr) {}

Cluster::Cluster(double x, Cluster::ptr predecessor)
    : x(x), weight(0), q(0), width(0), predecessor(predecessor) {}

SubRow::SubRow() : minX(0), maxX(0), freeWidth(maxX - minX), lastCluster(nullptr) {}

SubRow::SubRow(int minX, int maxX) : minX(minX), maxX(maxX), freeWidth(maxX - minX), lastCluster(nullptr) {}

void SubRow::updateMinMax(int minX_, int maxX_) {
    minX = minX_;
    maxX = maxX_;
    freeWidth = maxX_ - minX_;
}

Row::Row() : x(0), y(0), height(0), siteWidth(0) {}

Row::Row(int x, int y, double height, double siteWidth)
    : x(x), y(y), height(height), siteWidth(siteWidth) {}

// ★ 單參數：預設 round
int Row::getSiteX(double nonalignX) const {
    double shiftX = nonalignX - x;
    return x + (int)std::round(shiftX / siteWidth) * siteWidth;
}

// ★ 雙參數：可指定 floor/ceil/round
int Row::getSiteX(double nonalignX, double (*func)(double)) const {
    double shiftX = nonalignX - x;
    return x + func(shiftX / siteWidth) * siteWidth;
}

Input::Input() {
    std::cout << "======== Now Legalize ========"<< "\n";
    lef_ = &lef::Lef::get_instance();
    def_ = &def::Def::get_instance();

    auto& ldp = my_lefdef::LefDefParser::get_instance();
    const auto& comps = ldp.get_def().get_component_umap();

    // === Step 1. 取得 Banking 結果 (mbff_groups_) ===
    const auto& mbff_groups = my_lefdef::last_banking_result; // ★ 由 Banking::run_big() 更新

    // === Step 2. 建立 cells (只吃 Banking MBFFGroup) ===
    cells.clear();
    for (const auto& g : mbff_groups) {
        Cell *cell = new Cell(
            g.inst_name,
            g.width,
            g.height,
            g.place_x,
            g.place_y
        );
        cells.emplace_back(cell);
    }

    // === Step 3. 建立 blockages (非 FF/MBFF component) ===
    const auto& ffs   = ldp.getFFs();
    std::unordered_set<std::string> ff_names;
    for(const auto& ff : ffs) {
        ff_names.insert(ff.name);
    }
    blockages.clear();
    for(const auto& [comp_name, comp_ptr] : comps) {
        // 檢查這個component是否是FF或MBFF
        if(ff_names.find(comp_name) == ff_names.end()) {
            Cell *blockage = new Cell(
                comp_ptr->name_,     // 或 comp_name
                comp_ptr->lef_macro_->size_x_,    // 需要根據實際Component結構調整
                comp_ptr->lef_macro_->size_y_,   // 需要根據實際Component結構調整
                comp_ptr->x_,        // 需要根據實際Component結構調整
                comp_ptr->y_         // 需要根據實際Component結構調整
            );
            blockages.emplace_back(blockage);
        }
    }

    // === Step 4. Rows ===
    auto unit = lef_->get_dbu();
    rows.clear();
    auto& ROWS = def_->get_rows();
    for(auto const& R : ROWS){
        auto site = lef_->get_site(R->macro_);
        Row *row = new Row(R->x_, R->y_, site->y_ * unit, site->x_ * unit);
        row->subRows.emplace_back(new SubRow(R->x_, R->x_ + site->x_ * unit * R->num_x_));
        rows.emplace_back(row);
    }
}
