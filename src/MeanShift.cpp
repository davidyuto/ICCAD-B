#include "MeanShift.h"
#include <boost/functional/hash.hpp>
#include <algorithm>
#include <iostream>
#include <cmath>
#include "Cluster.h"

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
    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(ffs_.size()); ++i) {
        FlipFlop& ff = ffs_[i];
        ff.new_x = ff.x;
        ff.new_y = ff.y;
        ff.neighbors.clear();

        std::vector<PointWithID> results;
        rtree_.query(
            boost::geometry::index::nearest(BoostPoint(ff.x, ff.y), max_neighbors),
            std::back_inserter(results)
        );

        for (const auto& p : results) {
            int neighbor_idx = p.second;
            if (neighbor_idx == i) continue;

            const FlipFlop& neighbor = ffs_[neighbor_idx];
            double dist2 = squareDistance(ff.x, ff.y, neighbor.x, neighbor.y);
            if (dist2 < max_square_displacement) {
                ff.neighbors.emplace_back(neighbor_idx, dist2);
            }
        }

        std::sort(ff.neighbors.begin(), ff.neighbors.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });

        ff.isShifting = ff.neighbors.size() > 1;

        if (ff.isShifting) {
            size_t sel = std::min(ff.neighbors.size() - 1,
                                  static_cast<size_t>(max_neighbors - 1));
            ff.bandwidth = std::sqrt(ff.neighbors[sel].second);
        }
    }
}

void FlipFlopClustering::shiftAllFlipFlops(int max_iterations, double shift_tolerance) {
    double max_movement = 0.0;

    #pragma omp parallel for reduction(max:max_movement)
    for (int i = 0; i < static_cast<int>(ffs_.size()); ++i) {
        FlipFlop& ff = ffs_[i];
        if (!ff.isShifting) continue;

        for (int iter = 0; iter < max_iterations; ++iter) {
            double x_shift = 0.0, y_shift = 0.0, scale = 0.0;

            for (const auto& [nid, dist] : ff.neighbors) {
                const auto& nbr = ffs_[nid];
                double bw = nbr.bandwidth;
                if (bw < 1e-6) continue;

                double weight = std::exp(-dist / (2 * bw * bw)) / std::pow(bw, 2);
                x_shift += nbr.x * weight;
                y_shift += nbr.y * weight;
                scale   += weight;
            }

            if (scale < 1e-10) break;

            x_shift /= scale;
            y_shift /= scale;

            double dx = x_shift - ff.new_x;
            double dy = y_shift - ff.new_y;
            double d  = std::sqrt(dx * dx + dy * dy);

            ff.new_x = static_cast<int>(x_shift);
            ff.new_y = static_cast<int>(y_shift);

            if (d < shift_tolerance) break;
            if (d > max_movement) max_movement = d;
        }

        ff.setNewCoor(ff.new_x, ff.new_y);
        ff.setBandwidth();
        ff.isLegalize = true;
    }

    // 將平移後座標分桶成 cluster（粗略網格）
    // std::unordered_map<std::pair<int,int>, int, boost::hash<std::pair<int,int>>> cluster_grid;
    // int cluster_id = 0;
    // for (auto& ff : ffs_) {
    //     int gx = ff.new_x / 1000;
    //     int gy = ff.new_y / 1000;
    //     std::pair<int,int> key = std::make_pair(gx, gy);
    //     if (cluster_grid.count(key) == 0) cluster_grid[key] = cluster_id++;
    //     ff.clusterIdx = cluster_grid[key];
    // }
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

std::vector<Cluster>& FlipFlopClustering::getClusters() {
    return clusters_;
}

} // namespace my_lefdef
