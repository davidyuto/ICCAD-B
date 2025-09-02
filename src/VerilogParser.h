#ifndef VERILOG_PARSER_H
#define VERILOG_PARSER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "CheckFF.h"

namespace vparse {

// ======= 基本資料結構 =======

struct FFInstance {
    std::string cell_macro;       // e.g. SNPSHOPT25_FSDNQ_V3_1
    std::string inst_name;        // e.g. zreg_clk1inv1__2
    std::string param_overrides;  // #(...) 字串（若無則空）
    bool named_port = true;       // true: .PIN(expr)，false: 位置式
    std::vector<std::string> pos_ports; // 位置式列出 expr 原樣（含空白不修飾）
    std::unordered_map<std::string, std::string> pin2net; // 命名連線
    std::string original_text;    // 原 instance 全文（含註解與換行）
};

struct VerilogModule {
    std::string name;

    struct Chunk {
        enum Kind { Raw, FF } kind = Raw;
        std::string text; // kind==Raw: 原樣文本；kind==FF: 忽略，用 ff_id 指引
        int ff_id = -1;   // 指向 ff_instances 的索引
    };

    std::vector<Chunk> chunks;            // 模組內語句切成片段
    std::vector<FFInstance> ff_instances; // 僅 FF 的結構化實例
};

struct VerilogDesign {
    std::vector<VerilogModule> modules;
    std::unordered_map<std::string, size_t> mod_idx; // name -> modules index

    void dump_instances(const std::string& path) const;
};

// ======= 解析與輸出 API =======

// 解析檔案（throw on I/O error）。成功回傳完整設計。
// 僅 FF instance 會被結構化解析；其餘語句以 Raw chunk 保存。
VerilogDesign parse_verilog(const std::string& path);

// 解析字串（方便單元測試）
VerilogDesign parse_string(const std::string& content, const std::string& virtual_name = "<mem>");

// 輸出單一模組：遇到 FF chunk 時呼叫 emitFF 取得要輸出的字串；
// 若 emitFF 回傳空字串，則退回輸出原始 instance（ffi.original_text）。
void write_module(std::ostream& os,
                  const VerilogModule& mod,
                  const std::function<std::string(const FFInstance&)>& emitFF);

// 輸出整個設計；emitFF 同上。
void write_design(std::ostream& os,
                  const VerilogDesign& design,
                  const std::function<std::string(const FFInstance&)>& emitFF);

} // namespace vparse

#endif // VERILOG_PARSER_H
