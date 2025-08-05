// #include "Cluster.h"
// #include <stdexcept>
// #include <cmath>

// namespace my_lefdef {





// void Cluster::computeCenter() {
//     std::vector<int> xs, ys;
//     for (const auto& ff : FFs) {
//         xs.push_back(ff->x);
//         ys.push_back(ff->y);
//     }
//     if (xs.empty()) {
//         center_x = center_y = 0;
//         return;
//     }

//     std::sort(xs.begin(), xs.end());
//     std::sort(ys.begin(), ys.end());

//     size_t mid = xs.size() / 2;
//     center_x = (xs.size() % 2 == 0) ? (xs[mid - 1] + xs[mid]) / 2 : xs[mid];
//     center_y = (ys.size() % 2 == 0) ? (ys[mid - 1] + ys[mid]) / 2 : ys[mid];
// }

// int Cluster::getCenterX() const {
//     return center_x;
// }

// int Cluster::getCenterY() const {
//     return center_y;
// }

// }  // namespace my_lefdef