#include "Banking.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace my_lefdef {

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

Banking::Banking(std::vector<FlipFlop>& ffs, const LibParser& lib)
: ffs_(ffs), lib_(lib) {}

double Banking::computeCost(const std::string& mbff_macro,
                            const std::vector<FlipFlop*>& bits) const {
    const FFPowerArea info = lib_.getFFPowerArea(mbff_macro);
    if (info.area <= 0.0 && info.power <= 0.0)
        return std::numeric_limits<double>::max();

    int cx = 0, cy = 0;
    for (auto* ff : bits) { cx += ff->new_x; cy += ff->new_y; }
    cx /= (int)bits.size(); cy /= (int)bits.size();

    double disp = 0.0;
    for (auto* ff : bits) {
        double dx = ff->new_x - cx;
        double dy = ff->new_y - cy;
        disp += std::sqrt(dx*dx + dy*dy);
    }

    // === Cost function ===
    double cost = disp * 0.05 + info.area + info.power;

    // === Debug 印出詳細資訊 ===
    // std::cout << "[Cost] macro=" << mbff_macro
    //           << " bits=" << bits.size()
    //           << " area=" << info.area
    //           << " power=" << info.power
    //           << " disp=" << disp
    //           << " => cost=" << cost << "\n";

    return cost;
}

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
    // ===== Debug: 統計合併前 =====
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
                if (cj.getFFs().size() > 8) continue; // 避免過大 cluster

                if (f1->clk_net != cj.getFFs()[0]->clk_net) continue;

                double dx = f1->new_x - cj.getCenterX();
                double dy = f1->new_y - cj.getCenterY();
                double dist = std::sqrt(dx*dx + dy*dy);

                double avg_bw = (f1->bandwidth + cj.getFFs()[0]->bandwidth) / 2;
                double threshold =  0.8*avg_bw; // heuristic 門檻

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

    // ===== Debug: 統計合併後 =====
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
    clusters_.clear();
    mbff_groups_.clear();

    // === Step 0: 如果 forceK > 0，先用這個 K 重建 bandwidth ===
    if (forceK > 0) {
        // std::cout << "[run_big] Re-clustering with forceK=" << forceK << "\n";
        my_lefdef::FlipFlopClustering cl(ffs_);
        cl.buildRTree();
        cl.initKNN(forceK, 4000*4000);
        cl.shiftAllFlipFlops();

        // 把新的座標與 bandwidth 寫回 ffs_
        const auto& ffs_new = cl.getFFs();
        for (size_t i = 0; i < ffs_.size(); i++) {
            ffs_[i].new_x      = ffs_new[i].new_x;
            ffs_[i].new_y      = ffs_new[i].new_y;
            ffs_[i].bandwidth  = ffs_new[i].bandwidth;
            ffs_[i].isShifting = ffs_new[i].isShifting;
        }
    }

    // === Step 1: Clock domain 分組 ===
    std::unordered_map<std::string, std::vector<int>> by_clk;
    for (int i = 0; i < (int)ffs_.size(); i++) {
        std::string key = ffs_[i].clk_net.empty() ? "__NOCLK__" : ffs_[i].clk_net;
        by_clk[key].push_back(i);
    }

    std::vector<int> belong(ffs_.size(), -1);
    int cid = 0;

        // === Debug: Clock domains summary ===
    std::cout << "\n========== Clock Domain Summary ==========\n";
    for (auto& kv : by_clk) {
        const std::string& clk = kv.first;
        const auto& idxs = kv.second;
        std::cout << "CLK = " << clk
                  << "  #FFs = " << idxs.size() << "\n";
        for (int k = 0; k < 5 ; k++) {
            const FlipFlop& ff = ffs_[idxs[k]];
            std::cout << "   - " << ff.name
                      << " (macro=" << ff.macro
                      << ", x=" << ff.x
                      << ", y=" << ff.y << ")\n";
        }
        if ((int)idxs.size() > 5) {
            std::cout << "   ... (" << (idxs.size() - 5)
                      << " more FFs)\n";
        }
    }
    std::cout << "==========================================\n";

    // === Step 2: Clustering ===
    for (auto& kv : by_clk) {
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

    // === Step 3: 如果只要 Clustering，直接返回 ===
    if (!doBanking) {
        return;
    }

    // === Step 4: Cluster → MBFF (Banking) ===
    const double displacement_threshold = 1500.0;
    int gid = 0;
    for (auto& c : clusters_) {
        std::vector<FlipFlop*> bits;
        c.computeCenter();
        for (auto* ff : c.getFFs()) {
            double dx = ff->new_x - c.getCenterX();
            double dy = ff->new_y - c.getCenterY();
            double disp = std::sqrt(dx*dx + dy*dy);
            if (disp <= displacement_threshold) {
                bits.push_back(ff);
            }
        }

        int n = (int)bits.size();
        while (n >= 4) {
            std::vector<FlipFlop*> sub(bits.end()-4, bits.end());
            bits.erase(bits.end()-4, bits.end());
            n -= 4;
            std::string mb = pickMBFFMacro(sub, maps);
            if (!mb.empty()) {
                MBFFGroup g{gid++, mb, sub,
                            c.getCenterX(), c.getCenterY(),
                            computeCost(mb, sub)};
                mbff_groups_.push_back(std::move(g));
            }
        }
        if (n == 2) {
            std::vector<FlipFlop*> sub(bits.end()-2, bits.end());
            std::string mb = pickMBFFMacro(sub, maps);
            if (!mb.empty()) {
                MBFFGroup g{gid++, mb, sub,
                            c.getCenterX(), c.getCenterY(),
                            computeCost(mb, sub)};
                mbff_groups_.push_back(std::move(g));
            }
        }
    }

    // === Step 5: Debug 輸出 ===
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

    int showN = 3;
    std::cout << "\n[Debug] Show first " << showN << " MBFF groups:\n";
    for (size_t i = 0; i < mbff_groups_.size() && i < (size_t)showN; i++) {
        const auto& g = mbff_groups_[i];
        std::cout << "MBFFGroup ID=" << g.id
                << " macro=" << g.macro
                << " size=" << g.bits.size()
                << " cost=" << g.cost
                << " @(" << g.place_x << "," << g.place_y << ")\n";
        for (auto* ff : g.bits) {
            std::cout << "   - FF " << ff->name
                    << " macro=" << ff->macro
                    << " (" << ff->new_x << "," << ff->new_y << ")\n";
        }
    }
}
void Banking::printFinalGroups(const std::unordered_set<int>& pickIDs) const {
    std::cout << "\n[Final Banking Result]\n";
    for (const auto& g : mbff_groups_) {
        if (!pickIDs.empty() && !pickIDs.count(g.id)) continue;
        std::cout << "MBFFGroup ID=" << g.id
                  << " macro=" << g.macro
                  << " size=" << g.bits.size()
                  << " cost=" << g.cost
                  << " @(" << g.place_x << "," << g.place_y << ")\n";
        for (auto* ff : g.bits) {
            std::cout << "   - FF " << ff->name
                      << " macro=" << ff->macro
                      << " (" << ff->new_x << "," << ff->new_y << ")\n";
        }
    }
}



} // namespace my_lefdef
