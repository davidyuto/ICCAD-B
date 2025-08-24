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
}

} // namespace
