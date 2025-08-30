#pragma once

#include "LefDefParser.h"
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <vector>
#include <utility>
#include <cmath>
#include "Cluster.h"
namespace my_lefdef {

using BoostPoint = boost::geometry::model::point<int, 2, boost::geometry::cs::cartesian>;
using PointWithID = std::pair<BoostPoint, int>;
using RTree = boost::geometry::index::rtree<PointWithID, boost::geometry::index::quadratic<16>>;

class FlipFlopClustering {
public:
    FlipFlopClustering(std::vector<FlipFlop>& ffs);
    void buildRTree();
    void initKNN(int max_neighbors, double max_square_displacement);
    void shiftAllFlipFlops(int max_iterations = 50, double shift_tolerance = 0.1);
    std::vector<Cluster>& getClusters();
    const std::vector<FlipFlop>& getFFs() const { return ffs_; }
    int countNeighborsWithinRadius(double x, double y, double radius) const {
        namespace bg = boost::geometry;
        namespace bgi = boost::geometry::index;

        using Box = bg::model::box<BoostPoint>;

        BoostPoint center(static_cast<int>(x), static_cast<int>(y));
        Box query_box(
            BoostPoint(static_cast<int>(x - radius), static_cast<int>(y - radius)),
            BoostPoint(static_cast<int>(x + radius), static_cast<int>(y + radius))
        );

        std::vector<PointWithID> result;
        rtree_.query(bgi::within(query_box), std::back_inserter(result));

        return result.size() > 0 ? result.size() - 1 : 0;  // 排除自己（若包含）
    }



private:
    std::vector<FlipFlop>& ffs_;
    RTree rtree_;
    std::vector<Cluster> clusters_;
    double gaussianKernel(int x1, int y1, int x2, int y2, double bandwidth) const;
    double squareDistance(int x1, int y1, int x2, int y2) const;
};

}