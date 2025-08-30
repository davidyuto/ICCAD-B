// Banking.h
#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "Cluster.h"
#include "CompatParser.h"
#include "LefDefParser.h"
#include "MeanShift.h"
#include "LibParser.h"

namespace my_lefdef {

struct MBFFGroup {
    int id = -1;
    std::string macro;              // 選定的 MBFF 宏名（來自真 .lib）
    std::vector<FlipFlop*> bits;    // 被合併的 1-bit FF
    int place_x = 0, place_y = 0;   // 擬定放置位置（cluster center）
    double cost = 0.0;              // costFunction 分數
};

class Banking {
public:
    Banking(std::vector<FlipFlop>& ffs, const LibParser& lib);

    void run_big(const CompatMaps& maps,
             double tau_merge = 1.5,
             double max_pair_dist = 2500.0,
             double h_cap = 2000.0,
             int forceK = -1,        // 強制指定 K (Auto-K 用)
             bool doBanking = true); // 是否要跑到 Banking
    void debugClusterBanking(const CompatMaps& maps, int limit = 5) const;
    const std::vector<Cluster>&   getClusters() const { return clusters_; }
    const std::vector<MBFFGroup>& getMBFFs()   const { return mbff_groups_; }
    void printFinalGroups(const std::unordered_set<int>& pickIDs = {}) const;

private:
    double computeCost(const std::string& mbff_macro,
                       const std::vector<FlipFlop*>& bits) const;

    std::string pickMBFFMacro(const std::vector<FlipFlop*>& bits,
                              const CompatMaps& maps) const;
    void mergeCluster(const CompatMaps& maps);
    static inline double dist2_new(const FlipFlop& a, const FlipFlop& b);

private:
    std::vector<FlipFlop>& ffs_;
    const LibParser& lib_;

    std::vector<Cluster> clusters_;
    std::vector<MBFFGroup> mbff_groups_;
    
};

} // namespace my_lefdef
