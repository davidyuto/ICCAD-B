#include "MeanShift.h"
#include <boost/functional/hash.hpp> 
#include <algorithm>
#include <iostream>
#include <cmath>

namespace my_lefdef {

FlipFlopClustering::FlipFlopClustering(std::vector<FlipFlop>& ffs) : ffs_(ffs) {}

void FlipFlopClustering::buildRTree() {
    std::vector<PointWithID> points;
    for (size_t i = 0; i < ffs_.size(); ++i) {
        ffs_[i].ffIdx = static_cast<int>(i);
        points.emplace_back(BoostPoint(ffs_[i].x, ffs_[i].y), i);
    }
    rtree_.insert(points.begin(), points.end());
}

void FlipFlopClustering::initKNN(int max_neighbors, double max_square_displacement) {
    constexpr int M = 14;          
    constexpr double alpha_base = 1.5;
    constexpr double h_max = 1500.0;

    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(ffs_.size()); ++i) {
        FlipFlop& ff = ffs_[i];
        ff.new_x = ff.x;
        ff.new_y = ff.y;
        ff.neighbors.clear();

        std::vector<PointWithID> results;
        rtree_.query(boost::geometry::index::nearest(BoostPoint(ff.x, ff.y), max_neighbors),
                     std::back_inserter(results));

        for (const auto& p : results) {
            int neighbor_idx = p.second;
            if (neighbor_idx == i) continue;

            const FlipFlop& neighbor = ffs_[neighbor_idx];
            double dist2 = squareDistance(ff.x, ff.y, neighbor.x, neighbor.y);
            if (dist2 <= max_square_displacement) {
                ff.neighbors.emplace_back(neighbor_idx, dist2);
            }
        }

        std::sort(ff.neighbors.begin(), ff.neighbors.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

        ff.isShifting = !ff.neighbors.empty();

        if (ff.isShifting) {
            double dist_to_Mth = 800.0;  // Default
            if ((int)ff.neighbors.size() > M) {
                dist_to_Mth = std::sqrt(ff.neighbors[M].second);
            } else {
                dist_to_Mth = std::sqrt(ff.neighbors.back().second);
            }
            ff.setBandwidth(dist_to_Mth, h_max);
        } else {
            ff.bandwidth = 300.0;  
        }
    }
}

void FlipFlopClustering::shiftAllFlipFlops(int max_iterations, double shift_tolerance) {
    std::cout << "[Debug] Entering shiftAllFlipFlops()\n";
    std::cout << "[Debug] shiftAllFlipFlops() — total FFs: " << ffs_.size() << "\n";
    int shifting_count = 0;
    for (const auto& ff : ffs_) {
        if (ff.isShifting) shifting_count++;
    }
    std::cout << "[Debug] isShifting FFs = " << shifting_count << "\n";

    double max_movement = 0.0;

    for (int i = 0; i < static_cast<int>(ffs_.size()); ++i) {
        FlipFlop& ff = ffs_[i];
        if (!ff.isShifting) {
            // std::cout << "[FF " << ff.name << "] is not shifting, skipping.\n";
            continue;
        } 

        for (int iter = 0; iter < max_iterations; ++iter) {
            // std::cout << "[Shift] Iteration " << iter + 1 << " for FF " << ff.name << "\n";
            double x_shift = 0.0, y_shift = 0.0, scale = 0.0;
            for (const auto& [nid, dist] : ff.neighbors) {
                const auto& nbr = ffs_[nid];
                double bw = nbr.bandwidth;
                if (bw < 1e-6) continue;
                double weight = std::exp(-dist / (2 * bw * bw)) / std::pow(bw, 2);
                x_shift += nbr.x * weight;
                y_shift += nbr.y * weight;
                scale += weight;
            }

            if (scale < 1e-10) break;

            x_shift /= scale;
            y_shift /= scale;

            double dx = x_shift - ff.new_x;
            double dy = y_shift - ff.new_y;
            double dist = std::sqrt(dx * dx + dy * dy);

            ff.new_x = static_cast<int>(x_shift);
            ff.new_y = static_cast<int>(y_shift);

            // std::cout << "[Shift] " << ff.name << ": move = " << dist 
            //           << ", new = (" << ff.new_x << "," << ff.new_y << ")\n";
            if (dist < shift_tolerance) {
                // std::cout << "have reach its the shift tolerance, break.\n";
                break;
            }

            if (dist > max_movement) {
                max_movement = dist;
            }
        }

        ff.setNewCoor(ff.new_x, ff.new_y);
        ff.setBandwidth(1.5, 1500.0);
        ff.isLegalize = true; 
    }

    
}


double FlipFlopClustering::gaussianKernel(int x1, int y1, int x2, int y2, double bandwidth) const {
    double dist2 = squareDistance(x1, y1, x2, y2);
    double bw2 = bandwidth * bandwidth;
    return std::exp(-dist2 / (2 * bw2));
}

double FlipFlopClustering::squareDistance(int x1, int y1, int x2, int y2) const {
    double dx = static_cast<double>(x1 - x2);
    double dy = static_cast<double>(y1 - y2);
    return dx * dx + dy * dy;
}

} // namespace my_lefdef
