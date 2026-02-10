#pragma once
#include <string>
#include <unordered_map>

struct FFPowerArea {
    double area = 0.0;
    double power = 0.0; // 取自 pin(Q) 下、related_pin="CK" 的 internal_power values 平均
};

class LibParser {
public:
    // 啟動時先用內建表初始化（硬寫）
    LibParser();

    // 仍保留：載入單一 .lib，解析到的 FF 會覆寫/補充 ff_
    bool loadLib(const std::string& path);

    // 查詢：若查不到回傳 {0,0}
    FFPowerArea getFFPowerArea(const std::string& cell) const;

    // 全部 FF 對照表：cell_name -> {area, power}
    const std::unordered_map<std::string, FFPowerArea>& table() const { return ff_; }

private:
    std::unordered_map<std::string, FFPowerArea> ff_; // 只存 FF（FSDN/LSRDPQ）

    // 解析 .lib 用到的小工具
    static void stripComments(std::string& s);
    static bool isFFName(const std::string& cellName); // 名稱在白名單即視為 FF
    bool parseOneFile(const std::string& path);

    // 小工具：把一串 "values(...)"（可能含多行與反斜線續行）裡的數字抽出並追加到 sum/cnt
    static void accumulateValuesFromBlock(const std::string& block, double& sum, size_t& cnt);
};
