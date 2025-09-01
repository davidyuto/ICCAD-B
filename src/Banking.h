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
#include "Banking.h"
#include "LibParser.h"
#include "VerilogParser.h"

namespace my_lefdef {

struct MBFFGroup {
    int id = -1;
    std::string inst_name;
    std::string macro;
    std::vector<FlipFlop*> bits;
    int place_x = 0, place_y = 0;
    double cost = 0.0;
    double width = 0.0, height = 0.0;
    double area = 0.0;   // << 新增：從 .lib 抓的 area
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
    std::vector<MBFFGroup>& get_MBFFs() { return mbff_groups_; }
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
    std::vector<std::string> operation_log_;
    std::vector<Cluster> clusters_;
    std::vector<MBFFGroup> mbff_groups_;
    
};
extern std::vector<MBFFGroup> last_banking_result;
} // namespace my_lefdef