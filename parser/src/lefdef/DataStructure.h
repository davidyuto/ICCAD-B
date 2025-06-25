#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <vector>
#include "Def.h"

/**
 * @brief Basic information for a single placement row, extracted from DEF.
 */
struct RowInfo {
    int y;           ///< Row's Y coordinate (DBU)
    int orig_x;      ///< Starting X coordinate of the first site (DBU)
    int num_sites;   ///< Number of sites along the row
    int site_step;   ///< Pitch between adjacent sites (DBU)
};

/**
 * @brief Extracts a vector of RowInfo from the DEF parser singleton.
 *
 * Usage:
 *   auto rows = extractRowInfos();
 *   for (const auto &r : rows) {
 *     std::cout << "Row at y=" << r.y << ", orig_x=" << r.orig_x
 *               << ", count=" << r.num_sites
 *               << ", step=" << r.site_step << std::endl;
 *   }
 */
inline std::vector<RowInfo> extractRowInfos() {
    std::vector<RowInfo> out;
    const auto& def_rows = def::Def::get_instance().get_rows();
    out.reserve(def_rows.size());

    for (const auto& rptr : def_rows) {
        out.push_back({
            rptr->y_,       // y-coordinate
            rptr->x_,       // origin x
            rptr->num_x_,   // number of sites
            rptr->step_x_   // site step
        });
    }

    return out;
}

#endif // DATA_STRUCTURES_H
