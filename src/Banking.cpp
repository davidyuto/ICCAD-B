#include "Banking.h"
#include "CompatParser.h"
#include <iostream>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <array>

namespace my_lefdef {

static inline double dist2_new(const FlipFlop& a, const FlipFlop& b){
    double dx = double(a.new_x)-double(b.new_x);
    double dy = double(a.new_y)-double(b.new_y);
    return dx*dx+dy*dy;
}

static bool singles_have_common_mbff(const std::string& mA,const std::string& mB,const CompatMaps& maps){
    const auto& A = CompatParser::single_to_multi(maps,mA);
    const auto& B = CompatParser::single_to_multi(maps,mB);
    if (A.empty()||B.empty()) return false;
    std::unordered_set<std::string> s(A.begin(),A.end());
    for (auto& x:B) if (s.count(x)) return true;
    return false;
}

Banking::Banking(std::vector<FlipFlop>& ffs):ffs_(ffs){}

void Banking::run_big(const CompatMaps& maps,double tau_merge,double max_pair_dist,double h_cap){
    clusters_.clear();
    std::unordered_map<std::string,std::vector<int>> by_clk;
    for(int i=0;i<(int)ffs_.size();i++){
        std::string key = ffs_[i].clk_net.empty()?"__NOCLK__":ffs_[i].clk_net;
        by_clk[key].push_back(i);
    }

    std::vector<int> belong(ffs_.size(),-1);
    int next_cluster_id=0;

    for(auto& kv:by_clk){
        const auto& idxs = kv.second;
        for(size_t ii=0;ii<idxs.size();ii++){
            int i=idxs[ii];
            if(belong[i]!=-1) continue;

            Cluster c(next_cluster_id++);
            c.addFF(&ffs_[i]);
            belong[i]=c.getID();
            double hi = std::min(ffs_[i].bandwidth,h_cap);

            for(size_t jj=ii+1;jj<idxs.size();jj++){
                int j=idxs[jj];
                if(belong[j]!=-1) continue;
                double hj=std::min(ffs_[j].bandwidth,h_cap);
                double eps=tau_merge*std::min(hi,hj);
                double d=std::sqrt(dist2_new(ffs_[i],ffs_[j]));
                if(d>max_pair_dist) continue;
                if(d<=eps){
                    // 相容判斷
                    if(singles_have_common_mbff(ffs_[i].macro,ffs_[j].macro,maps)){
                        c.addFF(&ffs_[j]);
                        belong[j]=c.getID();
                    }
                }
            }
            c.computeCenter();
            clusters_.push_back(std::move(c));
        }
    }

    // debug: 分布
    std::unordered_map<int,int> hist;
    for(auto& c:clusters_) hist[c.getFFs().size()]++;
    std::cout<<"\n[Banking Result] Clusters="<<clusters_.size()<<"\n";
    for(auto& kv:hist){
        std::cout<<" size "<<kv.first<<" : "<<kv.second<<"\n";
    }
        // === Clock-domain-wise histograms ===
    std::unordered_map<std::string, std::unordered_map<int,int>> dom_hist; // clk -> (size -> count)
    std::unordered_map<int, std::string> cluster_clk; // clusterID -> clk

    // 先建立 clusterID -> clk 的對應
    for (const auto& kv2 : by_clk) {
        const std::string& clk = kv2.first;
        const auto& idxs = kv2.second;
        // 走訪該 domain 的 FF，把其屬的 cluster 記下
        for (int i : idxs) {
            // 找出 i 所屬 cluster
            // belong[i] 在前面已經填好
            int cid = belong[i];
            if (cid >= 0) {
                cluster_clk[cid] = clk;
            }
        }
    }

    // 統計每個 cluster 在該 clk 的大小
    for (const auto& c : clusters_) {
        std::string clk = "__UNKNOWN__";
        auto it = cluster_clk.find(c.getID());
        if (it != cluster_clk.end()) clk = it->second;
        int sz = (int)c.getFFs().size();
        dom_hist[clk][sz]++;
    }

    // 列印
    std::cout << "\n[Banking Result by Clock Domain]\n";
    for (const auto& kvd : dom_hist) {
        const auto& clk = kvd.first;
        std::cout << "  Clock Domain: " << clk << "\n";
        // 排序後列印
        std::vector<std::pair<int,int>> items(kvd.second.begin(), kvd.second.end());
        std::sort(items.begin(), items.end());
        for (auto& p : items) {
            std::cout << "    size " << p.first << " : " << p.second << "\n";
        }
    }
    // === For each clock domain, print two sample clusters with all FFs ===
    std::unordered_map<std::string, std::vector<int>> dom2clusters; // clk -> cluster IDs (unique)
    {
        std::unordered_set<int> seen;
        for (const auto& c : clusters_) {
            auto it = cluster_clk.find(c.getID());
            std::string clk = (it == cluster_clk.end()) ? "__UNKNOWN__" : it->second;
            dom2clusters[clk].push_back(c.getID());
        }
        // 去重（保險）
        for (auto& kvd : dom2clusters) {
            auto& v = kvd.second;
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        }
    }

    std::cout << "\n[Sample Clusters per Clock Domain]\n";
    for (const auto& kvd : dom2clusters) {
        const std::string& clk = kvd.first;
        const auto& vec = kvd.second;
        if (vec.empty()) continue;

        std::cout << "  Clock Domain: " << clk << "\n";

        // 只挑前兩個
        int pick = std::min<int>(2, (int)vec.size());
        for (int k = 0; k < pick; ++k) {
            int cid = vec[k];
            // 找到 cluster 物件
            const Cluster* pc = nullptr;
            for (const auto& c : clusters_) if (c.getID() == cid) { pc = &c; break; }
            if (!pc) continue;

            std::cout << "    Cluster #" << cid
                    << " | size=" << pc->getFFs().size()
                    << " | center=(" << pc->getCenterX() << "," << pc->getCenterY() << ")\n";

            // 列出所有 FF 的 name / macro / (new_x,new_y) / D/Q/CLK net（若可）
            for (auto* ff : pc->getFFs()) {
                std::cout << "      - " << ff->name
                        << " | macro=" << ff->macro
                        << " | new=(" << ff->new_x << "," << ff->new_y << ")"
                        << " | D="   << (ff->fanin_net.empty()  ? "None" : ff->fanin_net)
                        << " | Q="   << (ff->fanout_net.empty() ? "None" : ff->fanout_net)
                        << " | CLK=" << (ff->clk_net.empty()    ? "None" : ff->clk_net)
                        << "\n";
            }

            // 額外：快速 pairwise 相容「概略」檢查（是否有共同 MBFF 候選）
            bool all_pair_compatible = true;
            for (size_t i = 0; i < pc->getFFs().size() && all_pair_compatible; ++i) {
                for (size_t j = i+1; j < pc->getFFs().size(); ++j) {
                    const auto* A = pc->getFFs()[i];
                    const auto* B = pc->getFFs()[j];
                    if (!singles_have_common_mbff(A->macro, B->macro, maps)) {
                        all_pair_compatible = false;
                        break;
                    }
                }
            }
            std::cout << "      [compat-check] pairwise-common-MBFF = "
                    << (all_pair_compatible ? "PASS" : "FAIL (manual check needed)") << "\n";
        }
    }

}

} // namespace
