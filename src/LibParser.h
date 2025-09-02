#pragma once
#include <string>
#include <unordered_map>

struct FFPowerArea {
    double area = 0.0;
    double power = 0.0; // 取自 pin(Q) 下、related_pin="CK" 的 internal_power values 平均
};

class LibParser {
public:
    // 載入單一 .lib
    bool loadLib(const std::string& path);

    // 查詢：若查不到回傳 {0,0}
    FFPowerArea getFFPowerArea(const std::string& cell) const;

    // 全部 FF 對照表：cell_name -> {area, power}
    const std::unordered_map<std::string, FFPowerArea>& table() const { return ff_; }
    void dumpCache(const std::string& path) const;

    // 新增：從快取檔載入 (失敗回 false)
    bool loadCache(const std::string& path);

private:
    std::unordered_map<std::string, FFPowerArea> ff_; // 只存 FF（FSDN/LSRDPQ/LSRD）

    static void stripComments(std::string& s);
    static bool isFFName(const std::string& cellName); // 名稱含 FSDN 或 LSRDPQ/LSRD 即視為 FF

    bool parseOneFile(const std::string& path);

    // 小工具：把一串 "values(...)"（可能含多行與反斜線續行）裡的數字抽出並追加到 sum/cnt
    static void accumulateValuesFromBlock(const std::string& block, double& sum, size_t& cnt);
};