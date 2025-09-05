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

Banking::Banking(std::vector<FlipFlop>& ffs, const LibParser& lib)
: ffs_(ffs), lib_(lib) {}

std::string Banking::pickMBFFMacro(const std::vector<FlipFlop*>& bits,
                                   const CompatMaps& maps) const {
    if (bits.empty()) return "";

    size_t B = bits.size();
    const std::string& macro = bits[0]->macro; // 取第一個 FF 的 macro 名稱

    bool isFSDN   = (macro.find("FSDN")   != std::string::npos);
    bool isLSRDPQ = (macro.find("LSRDPQ") != std::string::npos);

    if (isFSDN) {
        if (B == 1) return "SNPSSLOPT25_FSDN_V2_1";
        if (B == 2) return "SNPSSLOPT25_FSDN2_V2_1";
        if (B == 4) return "SNPSSLOPT25_FSDN4_V2_1";
    } 
    else if (isLSRDPQ) {
        if (B == 1) return "SNPSSLOPT25_LSRDPQ_1";
        if (B == 4) return "SNPSSLOPT25_LSRDPQ4_1";
    }

    return "";
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
    // if (forceK > 0) {
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
    // }

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

        auto emit_sb = [&](FlipFlop* ff){
            MBFFGroup g;
            g.id      = gid++;
            g.macro   = ff->macro;        // 單顆沿用原 macro
            g.bits    = { ff };
            g.place_x = ff->new_x;
            g.place_y = ff->new_y;
            g.cost    = computeCost(g.macro, g.bits);
            g.inst_name = ff->name + std::string("_sb");
            auto pa = lib_.getFFPowerArea(g.macro);
            g.area = pa.area;
            if (auto lef_macro = lef::Lef::get_instance().get_macro(g.macro)) {
                g.width  = lef_macro->size_x_;
                g.height = lef_macro->size_y_;
            } else {
                g.width = g.height = 0.0;
            }
            mbff_groups_.push_back(std::move(g));
            usedFF.insert(ff);
        };

        // ---- 4-bit banking：先試後擦 ----
        while ((int)bits.size() >= 4) {
            std::vector<FlipFlop*> sub(bits.end()-4, bits.end());   // peek 4
            std::string mb = pickMBFFMacro(sub, maps);
            if (!mb.empty()) {
                // 成功才真的移除
                bits.erase(bits.end()-4, bits.end());

                MBFFGroup g;
                g.id      = gid++;
                g.macro   = mb;
                g.bits    = sub;
                g.place_x = c.getCenterX();
                g.place_y = c.getCenterY();
                g.cost    = computeCost(mb, sub);
                g.inst_name = sub[0]->name + "_mb";
                auto pa = lib_.getFFPowerArea(mb);
                g.area = pa.area;
                if (auto lef_macro = lef::Lef::get_instance().get_macro(g.macro)) {
                    g.width  = lef_macro->size_x_;
                    g.height = lef_macro->size_y_;
                } else {
                    g.width = g.height = 0.0;
                }
                mbff_groups_.push_back(std::move(g));
                for (auto* ff : sub) usedFF.insert(ff);
            } else {
                // 這個家族沒有 4-bit（理論上 FSDN/LSRDPQ 都有），跳出嘗試
                break;
            }
        }

        // 重新取得 n
        int n = (int)bits.size();

        // ---- 特判 3：優先嘗試 2-bit，失敗就三顆都當 SB ----
        if (n == 3) {
            std::vector<FlipFlop*> sub2(bits.end()-2, bits.end());  // peek 2
            std::string mb = pickMBFFMacro(sub2, maps);
            if (!mb.empty()) {
                // 成功 → 先產生 2-bit，再處理剩下 1 顆
                bits.erase(bits.end()-2, bits.end());

                MBFFGroup g;
                g.id      = gid++;
                g.macro   = mb;
                g.bits    = sub2;
                g.place_x = c.getCenterX();
                g.place_y = c.getCenterY();
                g.cost    = computeCost(mb, sub2);
                g.inst_name = sub2[0]->name + "_mb";
                auto pa = lib_.getFFPowerArea(mb);
                g.area = pa.area;
                if (auto lef_macro = lef::Lef::get_instance().get_macro(g.macro)) {
                    g.width  = lef_macro->size_x_;
                    g.height = lef_macro->size_y_;
                } else {
                    g.width = g.height = 0.0;
                }
                mbff_groups_.push_back(std::move(g));
                for (auto* ff : sub2) usedFF.insert(ff);

                // 剩下一顆 → SB
                emit_sb(bits.back());
                bits.pop_back();
            } else {
                // 這個家族（如 LSRDPQ）沒有 2-bit → 三顆全走 SB，避免遺失
                emit_sb(bits.back()); bits.pop_back();
                emit_sb(bits.back()); bits.pop_back();
                emit_sb(bits.back()); bits.pop_back();
            }
            n = (int)bits.size();
        }

        // ---- n == 2：嘗試 2-bit，失敗則兩顆都 SB ----
        if (n == 2) {
            std::vector<FlipFlop*> sub(bits.end()-2, bits.end());  // peek 2
            std::string mb = pickMBFFMacro(sub, maps);
            if (!mb.empty()) {
                bits.erase(bits.end()-2, bits.end());

                MBFFGroup g;
                g.id      = gid++;
                g.macro   = mb;
                g.bits    = sub;
                g.place_x = c.getCenterX();
                g.place_y = c.getCenterY();
                g.cost    = computeCost(mb, sub);
                g.inst_name = sub[0]->name + "_mb";
                auto pa = lib_.getFFPowerArea(mb);
                g.area = pa.area;
                if (auto lef_macro = lef::Lef::get_instance().get_macro(g.macro)) {
                    g.width  = lef_macro->size_x_;
                    g.height = lef_macro->size_y_;
                } else {
                    g.width = g.height = 0.0;
                }
                mbff_groups_.push_back(std::move(g));
                for (auto* ff : sub) usedFF.insert(ff);
            } else {
                // 家族沒有 2-bit（如 LSRDPQ）→ 兩顆都 SB
                emit_sb(bits.back()); bits.pop_back();
                emit_sb(bits.back()); bits.pop_back();
            }
            n = (int)bits.size();
        }

        // ---- n == 1：SB ----
        if (n == 1) {
            emit_sb(bits.back());
            bits.pop_back();
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