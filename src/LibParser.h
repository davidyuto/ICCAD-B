#pragma once
#include <string>
#include <unordered_map>

struct LibCellInfo {
    std::string name;
    double area = 0.0;
    double worst_clk_power_idx1 = 0.0; // 只取 index_1（無 index_2）的 internal_power（CK/CLK/CP/C）
};

class LibParser {
public:
    // 載入單一 .lib（可呼叫多次以合併）
    bool load(const std::string& filename);

    // 查詢
    const LibCellInfo* getCell(const std::string& name) const;
    const std::unordered_map<std::string, LibCellInfo>& allCells() const { return cells_; }

    // Debug：印前 N 筆
    void debugPrint(int limit = 10) const;

private:
    std::unordered_map<std::string, LibCellInfo> cells_;

    // 逐行解析 state machine
    void parseLine(const std::string& line,
                   std::string& curCell,
                   std::string& curPin,
                   bool& inCell,
                   bool& inPin,
                   bool& inIntPw,
                   bool& inRiseFall,
                   bool& sawIdx1_in_this_RF,
                   bool& sawIdx2_in_this_RF,
                   std::string& relatedPin,
                   double& curWorst_accum_for_this_IP);

    static bool isClockName(const std::string& pinName); // CK/CLK/CP/C（大小寫不敏感）
};
