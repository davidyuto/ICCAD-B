#ifndef LEFDEFPARSER_H
#define LEFDEFPARSER_H

#include "common_header.h"
#include "Lef.h"
#include "Def.h"

#include <string>
#include <vector>
#include <unordered_map>


namespace my_lefdef
{

struct FlipFlop {
    std::string name;
    int x, y;
    std::string macro;
    double width, height;

    std::string clk_net;     
    std::string fanin_net;   
    std::string fanout_net;  
 
    int new_x, new_y; 
    int ffIdx = -1;
    int clusterIdx = -1;

    std::vector<std::pair<int, double>> neighbors;

    double bandwidth = 0.0;
    bool isShifting = true;
    int clkIdx = -1;
    bool isLegalize = false;

    void setNewCoor(int nx, int ny) {
        new_x = nx;
        new_y = ny;
    }

    void resetShift() {
        new_x = x;
        new_y = y;
    }

    void addNeighbor(int idx, double dist) {
        neighbors.push_back({idx, dist});
    }

    void sortNeighbors() {
        std::sort(neighbors.begin(), neighbors.end(),
                  [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                      return a.second < b.second;
                  });
    }
    void setBandwidth(int M = 14, double alpha = 1.5, double h_max = 2500.0) {
        if (neighbors.empty()) {
            bandwidth = 300.0; // 最低保護
            return;
        }
        M = std::min(M, (int)neighbors.size()-1);
        double dist2 = neighbors[M].second;
        double dist = std::sqrt(dist2);

        double bw = std::min(h_max, alpha * dist);
        if (bw < 300.0) bw = 300.0;   // 避免孤立
        bandwidth = bw;
    }

// void setBandwidth(double dist_to_Mth, double criticality = 0.0, double h_max = 1500.0) {
//     criticality = std::max(0.0, std::min(1.0, criticality));  // 限制在 [0, 1]
//     double alpha = 1.5 * (1.0 - 0.8 * criticality);            // 保證 α ≥ 0.3
//     double raw_bw = alpha * dist_to_Mth;
//     double bw = std::min(h_max, raw_bw);
//     // if (bw < 300.0) bw = 300.0;
//     bandwidth = bw;

//     std::cout << "[BW] " << name << ": bw = " << bandwidth << "\n";
// }



    double squareDistanceTo(int px, int py) const {
        int dx = x - px;
        int dy = y - py;
        return static_cast<double>(dx * dx + dy * dy);
    }

    double squareDistanceToNew() const {
        int dx = x - new_x;
        int dy = y - new_y;
        return static_cast<double>(dx * dx + dy * dy);
    }

    double shift(const std::vector<FlipFlop>& allFFs) {
        double x_shift = 0.0, y_shift = 0.0, scale = 0.0;
        for (const auto& [nid, dist] : neighbors) {
            const auto& nbr = allFFs[nid];
            double bw = nbr.bandwidth;
            if (bw < 1e-6) continue;
            double weight = std::exp(-dist / (2 * bw * bw)) / std::pow(bw, 4);
            x_shift += nbr.x * weight;
            y_shift += nbr.y * weight;
            scale += weight;
        }

        if (scale < 1e-10) return 0.0;
        x_shift /= scale;
        y_shift /= scale;

        double dx = x_shift - new_x;
        double dy = y_shift - new_y;
        setNewCoor(static_cast<int>(x_shift), static_cast<int>(y_shift));
        return std::sqrt(dx * dx + dy * dy);
    }

};



struct MBFF {
    std::string group;             // 以 component 名称去掉末尾“_bit”编号得到
    std::vector<FlipFlop> bits;    // 每一位的实例和坐标
};

struct NetConnection {
    std::string instance;  // instance name or IO name
    std::string pin;       // D / Q / CLK / A1 / etc
};

struct NetlistNet {
    std::string name;
    std::vector<NetConnection> connections;
};

struct InternalNetlist {
    std::unordered_map<std::string, NetlistNet> nets;
};

class LefDefParser
{
public:
    static LefDefParser& get_instance();

    void read_lef(const std::string &filename);

    void read_def(const std::string &filename);


    void extractFlipFlops();

    const std::vector<FlipFlop>& getFFs() const { return ffs_; }
    const std::vector<MBFF>&    getMBFFs() const { return mbffs_; }


   

    def::Def& get_def();

    InternalNetlist extractNetlist() const;
    void fillFlipFlopNets();  

private:

    LefDefParser();
    ~LefDefParser() = default;
    LefDefParser(const LefDefParser&) = delete;
    LefDefParser& operator=(const LefDefParser&) = delete;
    LefDefParser(LefDefParser&&) = delete;
    LefDefParser& operator=(LefDefParser&&) = delete;

    // 底层 parser 引用
    lef::Lef& lef_;
    def::Def& def_;

    std::vector<FlipFlop> ffs_;
    std::vector<MBFF>     mbffs_;


    static int         countQpins       (const lef::MacroPtr &m);
    static bool        isMultiBitMacro  (const lef::MacroPtr &m);
    static bool        isSingleBitMacro (const lef::MacroPtr &m);
    static std::string extractGroupName (const std::string &compName);
};
} // namespace my_lefdef

#endif /* LEFDEFPARSER_H */
