#pragma once
#include <string>
#include <unordered_map>

// =====================
//  FFCellInfo 結構
// =====================
struct FFCellInfo {
    std::string name;
    double area = 0.0;       // cell 面積
    double cq_delay = 0.0;   // CK→Q delay (worst-case)
    double cq_power = 0.0;   // CK internal power (worst-case)
};

// =====================
//  LibParser 類別
// =====================
class LibParser {
public:
    void parseLib(const std::string& filename);
    void debugPrint() const;

    const std::unordered_map<std::string, FFCellInfo>& getCells() const {
        return ff_cells;
    }

private:
    std::unordered_map<std::string, FFCellInfo> ff_cells;
};
