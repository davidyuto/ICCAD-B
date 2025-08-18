#ifndef _CLUSTER_H_
#define _CLUSTER_H_

#include <vector>
#include <string>
#include <algorithm>
#include "LefDefParser.h"

namespace my_lefdef {

class Cluster {
private:
    int id;
    std::vector<FlipFlop*> FFs;
    int center_x;
    int center_y;

public:

    Cluster(int id = -1) : id(id), center_x(0), center_y(0) {}
    ~Cluster() {}

    void setID(int id) { this->id = id; }
    int getID() const { return id; }

    void addFF(FlipFlop* ff) { FFs.push_back(ff); }
    const std::vector<FlipFlop*>& getFFs() const { return FFs; }

    void computeCenter() {
        std::vector<int> xs, ys;
        for (const auto& ff : FFs) {
            xs.push_back(ff->x);
            ys.push_back(ff->y);
        }
        if (xs.empty()) {
            center_x = center_y = 0;
            return;
        }
        std::sort(xs.begin(), xs.end());
        std::sort(ys.begin(), ys.end());
        size_t mid = xs.size() / 2;
        center_x = (xs.size() % 2 == 0) ? (xs[mid - 1] + xs[mid]) / 2 : xs[mid];
        center_y = (ys.size() % 2 == 0) ? (ys[mid - 1] + ys[mid]) / 2 : ys[mid];
    }

    int getCenterX() const { return center_x; }
    int getCenterY() const { return center_y; }
};

}  // namespace my_lefdef

#endif
