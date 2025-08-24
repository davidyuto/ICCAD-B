// Banking.h
#pragma once
#include <vector>
#include <string>
#include "Cluster.h"
#include "CompatParser.h"   // ★ 新增：讓 CompatMaps 已宣告

namespace my_lefdef {

class Banking {
public:
    Banking(std::vector<FlipFlop>& ffs);

    void run();

    // ★ 新版：需要相容表 maps
    void run_big(const CompatMaps& maps,
                 double tau_merge = 0.6,
                 double max_pair_dist = 2500.0,
                 double h_cap = 2000.0);

    // ★ 舊版 wrapper：維持相容性；若有人仍呼叫舊版，會報錯提示或直接走無相容分桶（你可擇一）
    void run_big(double tau_merge, double max_pair_dist, double h_cap);

    const std::vector<Cluster>& getClusters() const { return clusters_; }

private:
    std::vector<FlipFlop>& ffs_;
    std::vector<Cluster> clusters_;
};

} // namespace my_lefdef
