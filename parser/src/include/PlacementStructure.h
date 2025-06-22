#ifndef PLACEMENT_STRUCTURES_H
#define PLACEMENT_STRUCTURES_H

#include <vector>

/**
 * @brief Basic information for a single placement row, extracted from DEF.
 */

 //
struct RowInfo {
    int y;            ///< Row's Y coordinate (DBU)
    int orig_x;       ///< Starting X coordinate of the first site (DBU)
    int num_sites;    ///< Number of sites along the row
    int site_step;    ///< Pitch between adjacent sites (DBU)
};
//changed
/**
 * @brief Extracts a vector of RowInfo from the DEF parser singleton.
 *
 * Usage:
 *   auto rows = extractRowInfos();
 *   for (auto &r : rows) {
 *     std::cout << "Row at y=" << r.y << ", orig_x=" << r.orig_x
 *               << ", count=" << r.num_sites
 *               << ", step=" << r.site_step << std::endl;
 *   }
 */
#include "Def.h"  // Assumes your Def class and RowPtr are declared here
static inline std::vector<RowInfo> extractRowInfos() {
    std::vector<RowInfo> out;
    auto const& def_rows = def::Def::get_instance().get_rows();
    out.reserve(def_rows.size());
    for (auto const& rptr : def_rows) {
        RowInfo info;
        info.y         = rptr->y_;        // row y-coordinate
        info.orig_x    = rptr->x_;        // row origin x
        info.num_sites = rptr->num_x_;    // number of repeats in X
        info.site_step = rptr->step_x_;   // step in X
        out.push_back(info);
    }
    return out;
}

#endif // PLACEMENT_STRUCTURES_H
