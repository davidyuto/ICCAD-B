#include "Banking.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <map>

// ===== R-tree（Boost.Geometry）=====
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>

namespace my_lefdef {

namespace bg  = boost::geometry;
namespace bgi = boost::geometry::index;

using BoostPoint   = bg::model::point<int, 2, bg::cs::cartesian>;
using PointWithID  = std::pair<BoostPoint, int>;
using RTree        = bgi::rtree<PointWithID, bgi::quadratic<16>>;

// 小工具：兩點曼哈頓距離（兩點 HPWL 等價）
static inline int manhattan(const FlipFlop* a, const FlipFlop* b) {
    return std::abs(a->x - b->x) + std::abs(a->y - b->y);
}

// 你原本的建構子保留
Banking::Banking(std::vector<FlipFlop>& ffs)
    : ffs_(ffs) {}

void Banking::bitOrdering() {
    bitOrder_.clear();
    // 假設先嘗試 8-bit, 4-bit, 2-bit 的順序
    bitOrder_.push_back(8);
    bitOrder_.push_back(4);
    bitOrder_.push_back(2);
}


void Banking::run() {
    bitOrdering(); // 目前先用 dummy 排序

    clusters_.clear();
    int clusterCount = 0;

    const int K_NEIGHBORS = 32;
    std::vector<int> allHPWL; // 收集所有 HPWL

    // 全域已分群標記
    std::unordered_set<FlipFlop*> globallyClustered;

    // 1) Clock domain 分組
    std::unordered_map<std::string, std::vector<FlipFlop*>> domains;
    for (auto& ff : ffs_) {
        const std::string key = ff.clk_net.empty() ? "__NOCLK__" : ff.clk_net;
        domains[key].push_back(&ff);
    }

    // 第一次掃描 — 收集所有 HPWL 值
    for (auto& [clk_name, group] : domains) {
        for (size_t i = 0; i < group.size(); ++i) {
            FlipFlop* nowFF = group[i];
            for (size_t j = i + 1; j < group.size(); ++j) {
                FlipFlop* otherFF = group[j];
                int dist = manhattan(nowFF, otherFF);
                allHPWL.push_back(dist);
            }
        }
    }

    if (allHPWL.empty()) return;

    std::sort(allHPWL.begin(), allHPWL.end());
    auto percentile = [&](double p) {
        size_t idx = static_cast<size_t>(p * (allHPWL.size() - 1));
        return allHPWL[idx];
    };
    int Q50 = percentile(0.50);
    int Q75 = percentile(0.75);
    int Q90 = percentile(0.90);

    // 自動設定門檻
    int HPWL_THRESHOLD = (Q75 + Q50) / 2;

    std::cout << "\n========== HPWL Stats ==========\n";
    std::cout << "Q50 = " << Q50 << "\n";
    std::cout << "Q75 = " << Q75 << "\n";
    std::cout << "Q90 = " << Q90 << "\n";
    std::cout << "Using HPWL_THRESHOLD = (Q75 + Q50) / 2 = " << HPWL_THRESHOLD << "\n";
    std::cout << "================================\n";

    int passCount = 0, failCount = 0;

    // 2) 依 targetBit 順序處理
    for (int targetBit : bitOrder_) {
        if (targetBit <= 1) continue;
        std::cout << "[Banking] Start targetBit = " << targetBit << "\n";

        for (auto& [clk_name, group] : domains) {
            if (group.empty()) continue;

            std::vector<bool> isClustered(group.size(), false);
            for (size_t i = 0; i < group.size(); ++i) {
                if (globallyClustered.count(group[i])) {
                    isClustered[i] = true;
                }
            }

            // 建 R-tree
            RTree rtree;
            std::vector<PointWithID> points;
            for (int i = 0; i < (int)group.size(); ++i) {
                if (!isClustered[i]) {
                    points.emplace_back(BoostPoint(group[i]->x, group[i]->y), i);
                }
            }
            rtree.insert(points.begin(), points.end());

            // 嘗試分群
            for (int idx = 0; idx < (int)group.size(); ++idx) {
                if (isClustered[idx]) continue;
                FlipFlop* nowFF = group[idx];

                // KNN 查詢
                std::vector<PointWithID> results;
                rtree.query(bgi::nearest(BoostPoint(nowFF->x, nowFF->y), K_NEIGHBORS),
                            std::back_inserter(results));

                // 過濾已分群 + HPWL 門檻
                std::vector<std::pair<int, int>> candidates;
                for (auto& pr : results) {
                    int gid = pr.second;
                    if (gid < 0 || gid >= (int)group.size()) continue;
                    if (isClustered[gid]) continue;

                    int dist = manhattan(nowFF, group[gid]);
                    if (dist <= HPWL_THRESHOLD) {
                        candidates.emplace_back(gid, dist);
                        passCount++;
                    } else {
                        failCount++;
                    }
                }

                std::sort(candidates.begin(), candidates.end(),
                          [](auto& a, auto& b) { return a.second < b.second; });

                // 湊滿 targetBit
                std::vector<FlipFlop*> selected;
                std::vector<int> toRemoveIDs;
                int bitSum = 0;
                for (auto& [gid, _dist] : candidates) {
                    FlipFlop* ff = group[gid];
                    int bit = (ff->width > 0) ? (int)ff->width : 1;
                    if (bitSum + bit <= targetBit) {
                        selected.push_back(ff);
                        toRemoveIDs.push_back(gid);
                        bitSum += bit;
                    }
                    if (bitSum == targetBit) break;
                }
                if (bitSum != targetBit) continue;

                // 形成 cluster
                Cluster cluster(clusterCount++);
                for (auto* ff : selected) cluster.addFF(ff);
                cluster.computeCenter();
                int cx = cluster.getCenterX();
                int cy = cluster.getCenterY();

                for (auto* ff : selected) {
                    ff->new_x = cx;
                    ff->new_y = cy;
                    ff->x     = cx;
                    ff->y     = cy;
                    ff->clusterIdx = cluster.getID();
                    globallyClustered.insert(ff);
                }
                clusters_.push_back(std::move(cluster));

                for (int gid : toRemoveIDs) {
                    isClustered[gid] = true;
                    FlipFlop* ff = group[gid];
                    rtree.remove(std::make_pair(BoostPoint(ff->x, ff->y), gid));
                }
            }
        }
    }

    // HPWL 過濾統計
    std::cout << "\n========== HPWL Filter Summary ==========\n";
    std::cout << "PASS : " << passCount << "\n";
    std::cout << "FAIL : " << failCount << "\n";
    std::cout << "=========================================\n";

    // ===== 原本的 Debug Check =====
    std::cout << "\n========== Banking Debug Check ==========\n";
    std::map<size_t, int> cluster_size_count;
    int printedClusters = 0, maxPrint = 10;

    for (const auto& cluster : clusters_) {
        cluster_size_count[cluster.getFFs().size()]++;
        if (printedClusters < maxPrint) {
            int cid = cluster.getID();
            int cx = cluster.getCenterX();
            int cy = cluster.getCenterY();
            const auto& ffs = cluster.getFFs();
            std::string clusterClk = ffs.empty() ? "UNKNOWN" : ffs[0]->clk_net;

            std::cout << "Cluster #" << cid
                      << " size=" << ffs.size()
                      << " center=(" << cx << "," << cy << ")"
                      << " clk_domain=" << clusterClk << "\n";
            for (auto* ff : ffs) {
                std::cout << "   - " << ff->name
                          << " orig=(" << ff->x << "," << ff->y << ")"
                          << " new=(" << ff->new_x << "," << ff->new_y << ")"
                          << " clusterIdx=" << ff->clusterIdx
                          << " clk=" << ff->clk_net << "\n";
                if (ff->clk_net != clusterClk)
                    std::cout << "     [WARN] Clock domain mismatch!\n";
                if (ff->clusterIdx != cid)
                    std::cout << "     [WARN] FF clusterIdx != cluster.getID()\n";
            }
            printedClusters++;
        }
    }

    std::cout << "\n========== Cluster Size Distribution ==========\n";
    for (auto& [size, count] : cluster_size_count) {
        std::cout << "Clusters with " << size << " FF"
                  << (size > 1 ? "s" : "") << " : " << count << "\n";
    }
    std::cout << "===============================================\n";
}



// =================== 留存你原本的介面：目前未使用 ===================
// 如果你想保留舊流程，可繼續用這個版本（現在 run() 已不呼叫它）
std::vector<FlipFlop*> Banking::chooseCandidateFF(const FlipFlop& nowFF,
                                                  std::vector<FlipFlop>& allFFs,
                                                  int targetBit) {
    std::vector<FlipFlop*> candidates;
    std::vector<std::pair<FlipFlop*, double>> sorted;

    for (auto& ff : allFFs) {
        if (ff.clusterIdx != -1) continue;
        double dist = std::abs(ff.new_x - nowFF.new_x) + std::abs(ff.new_y - nowFF.new_y);
        sorted.emplace_back(&ff, dist);
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    int bitSum = 0;
    for (auto& [ff, _] : sorted) {
        int bit = static_cast<int>(ff->width);
        if (bit <= 0) bit = 1;
        if (bitSum + bit <= targetBit) {
            candidates.push_back(ff);
            bitSum += bit;
        }
        if (bitSum == targetBit) break;
    }
    if (bitSum == targetBit) return candidates;
    return {};
}

} // namespace my_lefdef
