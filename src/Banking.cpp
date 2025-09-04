#include "Banking.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace my_lefdef {
std::vector<MBFFGroup> last_banking_result;

inline double Banking::dist2_new(const FlipFlop& a, const FlipFlop& b) {
    double dx = double(a.new_x) - double(b.new_x);
    double dy = double(a.new_y) - double(b.new_y);
    return dx*dx + dy*dy;
}

static bool singles_have_common_mbff(const std::string& mA,
                                     const std::string& mB,
                                     const CompatMaps& maps) {
    const auto& A = CompatParser::single_to_multi(maps, mA);
    const auto& B = CompatParser::single_to_multi(maps, mB);
    if (A.empty() || B.empty()) return false;
    std::unordered_set<std::string> s(A.begin(), A.end());
    for (const auto& x : B) if (s.count(x)) return true;
    return false;
}
double Banking::computeCost(const std::string& mbff_macro,
                            const std::vector<FlipFlop*>& bits) const {
    const FFPowerArea info = lib_.getFFPowerArea(mbff_macro);
    if (info.area <= 0.0 && info.power <= 0.0)
        return std::numeric_limits<double>::max();

    // === Step 1: 掃描所有 cell，找出 area/power 範圍 ===
    double minArea = 1e18, maxArea = 0.0;
    double minPower = 1e18, maxPower = 0.0;
    for (const auto& kv : lib_.table()) {
        minArea  = std::min(minArea,  kv.second.area);
        maxArea  = std::max(maxArea,  kv.second.area);
        minPower = std::min(minPower, kv.second.power);
        maxPower = std::max(maxPower, kv.second.power);
    }

    // === Step 2: normalize ===
    double normArea  = (info.area  - minArea)  / (maxArea  - minArea + 1e-9);
    double normPower = (info.power - minPower) / (maxPower - minPower + 1e-9);

    // === Step 3: 加入 cluster displacement (可選) ===
    int cx = 0, cy = 0;
    for (auto* ff : bits) { cx += ff->new_x; cy += ff->new_y; }
    cx /= (int)bits.size(); cy /= (int)bits.size();

    double disp = 0.0;
    for (auto* ff : bits) {
        double dx = ff->new_x - cx;
        double dy = ff->new_y - cy;
        disp += std::sqrt(dx*dx + dy*dy);
    }
    double normDisp = disp / (disp + 100.0); // scale to 0~1

    // === Step 4: 組合 cost ===
    double alpha = 0.5;  // 面積權重
    double beta  = 0.5;  // 功耗權重
    double gamma = 0.2;  // 位移權重

    double cost = alpha * normArea + beta * normPower ;
    return cost;
}

struct DPState {
    double minCost;
    std::vector<std::vector<int>> groups;  // 每個 group 內 FF 的 indices
    std::vector<int> groupTypes;           // 1, 2, or 4
};

Banking::DPState Banking::findOptimalBanking(const std::vector<FlipFlop*>& ffs,
                                             const CompatMaps& maps,
                                             double cx, double cy) const {
    int n = ffs.size();
    if (n == 0) return {0.0, {}, {}};

    std::vector<DPState> dp(1 << n);
    dp[0] = {0.0, {}, {}};

    std::unordered_map<int, double> groupCost;
    std::unordered_map<int, std::string> groupMacro;

    // 枚舉所有可能的 {1,2,4} group
    for (int mask = 1; mask < (1 << n); mask++) {
        int bitCount = __builtin_popcount(mask);
        if (bitCount != 1 && bitCount != 2 && bitCount != 4) continue;

        std::vector<FlipFlop*> group;
        for (int i = 0; i < n; i++) {
            if (mask & (1 << i)) {
                group.push_back(ffs[i]);
            }
        }

        if (bitCount == 1) {
            // 單顆 FF：直接 fallback 為 SBFF
            groupCost[mask] = lib_.getFFPowerArea(group[0]->macro).area +
                            lib_.getFFPowerArea(group[0]->macro).power;
            groupMacro[mask] = group[0]->macro;
        } else {
            std::string macro = pickMBFFMacro(group, maps);
            if (!macro.empty()) {
                double cost = computeCost(macro, group);
                for (auto* ff : group) {
                    double dist = std::sqrt(pow(ff->new_x - cx, 2) +
                                            pow(ff->new_y - cy, 2));
                    cost += dist * 0.001;
                }
                groupCost[mask] = cost;
                groupMacro[mask] = macro;
            }
        }

    }

    for (int mask = 1; mask < (1 << n); mask++) {
        dp[mask].minCost = 1e18;

        for (int sub = mask; sub > 0; sub = (sub - 1) & mask) {
            if (groupCost.count(sub) == 0) continue;

            int remain = mask ^ sub;
            double newCost = dp[remain].minCost + groupCost[sub];

            if (newCost < dp[mask].minCost) {
                DPState candidate = dp[remain];   // 拷貝已有方案
                candidate.minCost = newCost;

                std::vector<int> groupIndices;
                for (int i = 0; i < n; i++) {
                    if (sub & (1 << i)) groupIndices.push_back(i);
                }
                candidate.groups.push_back(groupIndices);
                candidate.groupTypes.push_back(__builtin_popcount(sub));

                dp[mask] = std::move(candidate);  // 更新最佳解
            }

        }
    }

    return dp[(1 << n) - 1];
}



Banking::Banking(std::vector<FlipFlop>& ffs, const LibParser& lib)
: ffs_(ffs), lib_(lib) {}



void Banking::debugClusterBanking(const CompatMaps& maps, int limit) const {
    std::cout << "\n[Debug] Show up to " << limit << " clusters banking process with cost breakdown:\n";
    int shown = 0;
    for (const auto& c : clusters_) {
        if (shown++ >= limit) break;

        std::cout << "\nCluster ID=" << c.getID()
                  << " size=" << c.getFFs().size()
                  << " center=(" << c.getCenterX()
                  << "," << c.getCenterY() << ")\n";

        // 列出 FF
        for (auto* ff : c.getFFs()) {
            std::cout << "  FF " << ff->name
                      << " macro=" << ff->macro
                      << " (" << ff->new_x << "," << ff->new_y << ")\n";
        }

        // 嘗試 Banking (只展示 2-bit 和 4-bit 組合)
        auto tryBank = [&](int bits) {
            if ((int)c.getFFs().size() >= bits) {
                std::vector<FlipFlop*> sub(c.getFFs().begin(),
                                           c.getFFs().begin() + bits);
                std::string mb = pickMBFFMacro(sub, maps);
                if (!mb.empty()) {
                    FFPowerArea info = lib_.getFFPowerArea(mb);

                    // 計算 disp
                    int cx = 0, cy = 0;
                    for (auto* ff : sub) { cx += ff->new_x; cy += ff->new_y; }
                    cx /= (int)sub.size(); cy /= (int)sub.size();
                    double disp = 0.0;
                    for (auto* ff : sub) {
                        double dx = ff->new_x - cx;
                        double dy = ff->new_y - cy;
                        disp += std::sqrt(dx*dx + dy*dy);
                    }

                    double cost = disp * 0.5 + info.area + info.power;

                    std::cout << "  Try " << bits << "-bit → " << mb
                              << "  area=" << info.area
                              << "  power=" << info.power
                              << "  disp=" << disp
                              << "  => cost=" << cost << "\n";
                }
            }
        };

        tryBank(2);
        tryBank(4);
    }
}



std::string Banking::pickMBFFMacro(const std::vector<FlipFlop*>& bits,
                                   const CompatMaps& maps) const {
    size_t B = bits.size();
    if (B != 2 && B != 4) return "";

    std::unordered_set<std::string> common;
    bool first = true;
    for (auto* ff : bits) {
        const auto& cands = CompatParser::single_to_multi(maps, ff->macro);
        if (cands.empty()) return "";
        if (first) {
            common.insert(cands.begin(), cands.end());
            first = false;
        } else {
            std::unordered_set<std::string> tmp;
            for (auto& c : cands) if (common.count(c)) tmp.insert(c);
            common.swap(tmp);
        }
    }
    if (common.empty()) return "";

    // 過濾符合 2/4-bit 的名稱
    std::vector<std::string> filtered;
    for (auto& mb : common) {
        if ((B == 2 && mb.find("2") != std::string::npos) ||
            (B == 4 && mb.find("4") != std::string::npos)) {
            filtered.push_back(mb);
        }
    }
    if (filtered.empty()) filtered.assign(common.begin(), common.end());

    std::string best;
    double bestCost = std::numeric_limits<double>::max();
    for (auto& mb : filtered) {
        double c = computeCost(mb, bits);
        if (c < bestCost) { bestCost = c; best = mb; }
    }
    return best;
}


void Banking::mergeCluster(const CompatMaps& maps) {
    int before_single = 0;
    for (auto& c : clusters_) {
        if ((int)c.getFFs().size() == 1) before_single++;
    }
    std::cout << "\n[mergeCluster] Before merge: " << before_single << " clusters with size=1\n";

    std::vector<Cluster> newClusters;
    std::unordered_set<int> merged;

    for (size_t i = 0; i < clusters_.size(); i++) {
        if (merged.count(i)) continue;
        Cluster& ci = clusters_[i];

        if (ci.getFFs().size() == 1) {
            FlipFlop* f1 = ci.getFFs()[0];
            Cluster* best = nullptr;
            double bestDist = 1e9;

            for (size_t j = 0; j < clusters_.size(); j++) {
                if (i == j || merged.count(j)) continue;
                Cluster& cj = clusters_[j];
                if (cj.getFFs().size() > 8) continue;

                if (f1->clk_net != cj.getFFs()[0]->clk_net) continue;

                double dx = f1->new_x - cj.getCenterX();
                double dy = f1->new_y - cj.getCenterY();
                double dist = std::sqrt(dx*dx + dy*dy);

                double avg_bw = (f1->bandwidth + cj.getFFs()[0]->bandwidth) / 2;
                double threshold = 0.8 * avg_bw;

                if (dist < threshold && dist < bestDist) {
                    best = &cj;
                    bestDist = dist;
                }
            }

            if (best) {
                best->addFF(f1);
                best->computeCenter();
                merged.insert(i);
            }
        }
    }

    for (size_t i = 0; i < clusters_.size(); i++) {
        if (!merged.count(i)) newClusters.push_back(std::move(clusters_[i]));
    }
    clusters_.swap(newClusters);

    int after_single = 0;
    for (auto& c : clusters_) {
        if ((int)c.getFFs().size() == 1) after_single++;
    }
    std::cout << "[mergeCluster] After merge:  " << after_single << " clusters with size=1\n";
    std::cout << "[mergeCluster] Reduced:     " << (before_single - after_single) << " clusters merged\n";
}


void Banking::run_big(const CompatMaps& maps,
                      double tau_merge,
                      double max_pair_dist,
                      double h_cap,
                      int forceK,          // Auto-K 用，強制指定 K
                      bool doBanking) {    // 是否要跑到 Banking
    std::cout<< "now size " << mbff_groups_.size() << "\n";
    clusters_.clear();
    mbff_groups_.clear();

    // === Step 0: 如果 forceK > 0，先用這個 K 重建 bandwidth ===
    if (forceK > 0) {
        my_lefdef::FlipFlopClustering cl(ffs_);
        cl.buildRTree();
        cl.initKNN(forceK, 4000*4000);
        cl.shiftAllFlipFlops();

        const auto& ffs_new = cl.getFFs();
        for (size_t i = 0; i < ffs_.size(); i++) {
            ffs_[i].new_x      = ffs_new[i].new_x;
            ffs_[i].new_y      = ffs_new[i].new_y;
            ffs_[i].bandwidth  = ffs_new[i].bandwidth;
            ffs_[i].isShifting = ffs_new[i].isShifting;
        }
    }

    // === Step 1: 分群 (Clock + Hierarchy) ===
    std::unordered_map<std::string, std::vector<int>> by_group;
    for (int i = 0; i < (int)ffs_.size(); i++) {
        std::string clk_key  = ffs_[i].clk_net.empty() ? "__NOCLK__" : ffs_[i].clk_net;
        std::string hier_key = ffs_[i].hier_module.empty() ? "__NOHIER__" : ffs_[i].hier_module;
        std::string key = clk_key + "|" + hier_key;
        by_group[key].push_back(i);
    }

    std::vector<int> belong(ffs_.size(), -1);
    int cid = 0;

    // === Step 2: Clustering ===
    for (auto& kv : by_group) {
        const auto& idxs = kv.second;
        for (size_t a = 0; a < idxs.size(); a++) {
            int i = idxs[a];
            if (belong[i] != -1) continue;

            Cluster c(cid++);
            c.addFF(&ffs_[i]);
            belong[i] = c.getID();
            double hi = std::min(ffs_[i].bandwidth, h_cap);

            for (size_t b = a+1; b < idxs.size(); b++) {
                int j = idxs[b];
                if (belong[j] != -1) continue;
                double hj = std::min(ffs_[j].bandwidth, h_cap);
                double eps = tau_merge * std::min(hi, hj);
                double d = std::sqrt(dist2_new(ffs_[i], ffs_[j]));
                if (d > max_pair_dist) continue;
                if (d <= eps) {
                    if (singles_have_common_mbff(ffs_[i].macro, ffs_[j].macro, maps)) {
                        c.addFF(&ffs_[j]);
                        belong[j] = c.getID();
                    }
                }
            }
            c.computeCenter();
            clusters_.push_back(std::move(c));
        }
    }

    if (!doBanking) return;

    auto& ldp = my_lefdef::LefDefParser::get_instance();

    const double displacement_threshold = 2500.0;
    int gid = 0;
    std::unordered_set<FlipFlop*> usedFF;

    // === Step 4: Cluster → MBFF (Banking) ===
    for (auto& c : clusters_) {
        std::vector<FlipFlop*> bits;
        c.computeCenter();

        for (auto* ff : c.getFFs()) bits.push_back(ff);
        int n = (int)bits.size();

        // ---- 4-bit banking ----
        while (n >= 4) {
            std::vector<FlipFlop*> sub(bits.end()-4, bits.end());
            bits.erase(bits.end()-4, bits.end());
            n -= 4;

            std::string mb = pickMBFFMacro(sub, maps);
            if (!mb.empty()) {
                MBFFGroup g;
                g.id      = gid++;
                g.macro   = mb;
                g.bits    = sub;
                g.place_x = c.getCenterX();
                g.place_y = c.getCenterY();
                g.cost    = computeCost(mb, sub);
                g.inst_name = sub[0]->name + "_mb";
                g.area = lib_.getFFPowerArea(mb).area;

                mbff_groups_.push_back(std::move(g));
                for (auto* ff : sub) usedFF.insert(ff);
            }
        }

        // ---- 特判 3 → 2+1 ----
        if (n == 3) {
            std::vector<FlipFlop*> sub2(bits.end()-2, bits.end());
            bits.erase(bits.end()-2, bits.end());
            n -= 2;

            std::string mb = pickMBFFMacro(sub2, maps);
            if (!mb.empty()) {
                MBFFGroup g;
                g.id = gid++;
                g.macro = mb;
                g.bits = sub2;
                g.place_x = c.getCenterX();
                g.place_y = c.getCenterY();
                g.cost = computeCost(mb, sub2);
                g.inst_name = sub2[0]->name + "_mb";
                g.area = lib_.getFFPowerArea(mb).area;

                mbff_groups_.push_back(std::move(g));
                for (auto* ff : sub2) usedFF.insert(ff);
            }
            n = (int)bits.size();
        }

        // ---- 2-bit banking ----
        if (n == 2) {
            std::vector<FlipFlop*> sub(bits.end()-2, bits.end());
            bits.erase(bits.end()-2, bits.end());
            n -= 2;

            std::string mb = pickMBFFMacro(sub, maps);
            if (!mb.empty()) {
                MBFFGroup g;
                g.id = gid++;
                g.macro = mb;
                g.bits = sub;
                g.place_x = c.getCenterX();
                g.place_y = c.getCenterY();
                g.cost = computeCost(mb, sub);
                g.inst_name = sub[0]->name + "_mb";
                g.area = lib_.getFFPowerArea(mb).area;

                mbff_groups_.push_back(std::move(g));
                for (auto* ff : sub) usedFF.insert(ff);
            }
        }

        // ---- 單顆 (可能換 cell type) ----
        if (n == 1) {
            FlipFlop* ff = bits.back();
            bits.pop_back();

            // 原始 macro
            auto best_macro = ff->macro;
            auto best_info  = lib_.getFFPowerArea(best_macro);
            double best_cost = best_info.area + best_info.power;

            // 嘗試找到同 family 更好的 single-bit
            for (const auto& cand : CompatParser::single_ff_set()) {
                if (CompatParser::family_of(cand) != CompatParser::family_of(ff->macro))
                    continue;
                auto info = lib_.getFFPowerArea(cand);
                if (info.area <= 0 || info.power <= 0) continue;

                double cost = info.area + info.power;
                if (cost < best_cost) {
                    best_cost  = cost;
                    best_macro = cand;
                }
            }

            MBFFGroup g;
            g.id      = gid++;
            g.macro   = best_macro;       // 如果有找到更好 → 換 cell type
            g.bits.clear();
            g.bits.push_back(ff);
            g.place_x = ff->new_x;
            g.place_y = ff->new_y;
            g.cost    = best_cost;
            g.inst_name = ff->name + "_sb";
            g.area = lib_.getFFPowerArea(best_macro).area;

            mbff_groups_.push_back(std::move(g));
            usedFF.insert(ff);
        }
    }

    // ---- 加入 DEF 裡已有的 MBFF ----
    gid = static_cast<int>(mbff_groups_.size());
    if (!ldp.getMBFFs().empty()) {
        std::cout << "[Debug] Added " << ldp.getMBFFs().size()
                  << " existing MBFFs from DEF into mbff_groups_\n";
    }
    for (const auto& mb : ldp.getMBFFs()) {
        MBFFGroup g;
        g.id       = gid++;
        g.inst_name = mb.group;
        g.macro    = mb.macro;
        g.place_x  = mb.x;
        g.place_y  = mb.y;
        g.cost     = 0.0;
        mbff_groups_.push_back(std::move(g));
    }

    // === Debug ===
    int totalFF = 0;
    for (auto& g : mbff_groups_) totalFF += g.bits.size();
    std::cout << "[Check] FF in MBFF groups = " << totalFF
              << " , original total = " << ffs_.size() << "\n";

    std::cout << "\n[Banking] Final MBFF groups = " << mbff_groups_.size() << "\n";
    std::unordered_map<int,int> sizeHist;
    for (const auto& g : mbff_groups_) sizeHist[g.bits.size()]++;
    std::cout << "\n========== Banking MBFF Size Distribution ==========\n";
    for (auto& kv : sizeHist) {
        std::cout << "  size=" << kv.first << " : " << kv.second << " groups\n";
    }
    std::unordered_map<int,int> clusterSizeHist;
    for (const auto& c : clusters_) clusterSizeHist[(int)c.getFFs().size()]++;
    std::cout << "\n========== Cluster Size Distribution ==========\n";
    for (auto& kv : clusterSizeHist) {
        std::cout << " size=" << kv.first << " : " << kv.second << " clusters\n";
    }

    last_banking_result = mbff_groups_;
}



} // namespace my_lefdef