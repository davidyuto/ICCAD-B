#include "VerilogParser.h"
#include "CheckFF.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cctype>
#include <stdexcept>
#include <iostream>

namespace vparse {

// ========== 小工具：字串處理（不改原文，僅供判斷） ==========

static inline std::string strip_line_comment(const std::string& s) {
    // 去掉 // 後內容（不處理字串常量內的 //，第一版可忽略）
    std::string out; out.reserve(s.size());
    bool in_block = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!in_block && i+1 < s.size() && s[i]=='/' && s[i+1]=='*') { in_block=true; ++i; continue; }
        if ( in_block && i+1 < s.size() && s[i]=='*' && s[i+1]=='/') { in_block=false; ++i; continue; }
        if (in_block) continue;
        if (s[i]=='/' && i+1<s.size() && s[i+1]=='/') { break; }
        out.push_back(s[i]);
    }
    return out;
}

static inline std::string strip_block_comments(const std::string& s) {
    std::string out; out.reserve(s.size());
    bool in_block = false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (!in_block && i+1 < s.size() && s[i]=='/' && s[i+1]=='*') { in_block=true; ++i; continue; }
        if ( in_block && i+1 < s.size() && s[i]=='*' && s[i+1]=='/') { in_block=false; ++i; continue; }
        if (!in_block) out.push_back(s[i]);
    }
    return out;
}

static inline std::string strip_comments(const std::string& s) {
    // 先移多行塊註解，再去單行
    return strip_line_comment(strip_block_comments(s));
}

static inline bool is_space_only(const std::string& s) {
    for (char c: s) if (!std::isspace((unsigned char)c)) return false;
    return true;
}

static inline std::string trim(const std::string& s){
    size_t i=0, j=s.size();
    while (i<j && std::isspace((unsigned char)s[i])) ++i;
    while (j>i && std::isspace((unsigned char)s[j-1])) --j;
    return s.substr(i, j-i);
}

// 括號平衡切片：回傳語句 [begin, end]（含 ';'）的 end 位置（包含 end）
static size_t find_stmt_end(const std::string& text, size_t begin) {
    int par=0, brk=0, brc=0;
    for (size_t i=begin; i<text.size(); ++i) {
        char c = text[i];
        if (c=='(') ++par; else if (c==')') --par;
        else if (c=='[') ++brk; else if (c==']') --brk;
        else if (c=='{') ++brc; else if (c=='}') --brc;
        else if (c==';' && par==0 && brk==0 && brc==0) return i+1; // 含 ';'
    }
    return text.size();
}

static bool starts_with_module(const std::string& line_shadow, std::string& modname_out) {
    // 寬鬆偵測 module name
    static const std::regex re(R"(^\s*module\s+([A-Za-z_]\w*))");
    std::smatch m;
    if (std::regex_search(line_shadow, m, re)) { modname_out = m[1]; return true; }
    return false;
}

static bool is_endmodule_line(const std::string& line_shadow) {
    static const std::regex re(R"(\bendmodule\b)");
    return std::regex_search(line_shadow, re);
}

// 嘗試把 "cell [#(...)] inst (" 三段抓出來；回傳是否像 instance
struct InstHead {
    std::string cell;
    std::string param; // 包含 #(...)
    std::string inst;
    size_t open_paren_pos = std::string::npos; // '(' 的位置（在 shadow 中）
};

static bool parse_instance_head(const std::string& shadow, InstHead& out) {
    // NOTE: 不吃 array inst / generate 第一版可忽略
    static const std::regex re(
        R"(^\s*([A-Za-z_]\w*)\s*(#\s*\([^;]*\))?\s+([A-Za-z_]\w*)\s*\()"
    );
    std::smatch m;
    if (!std::regex_search(shadow, m, re)) return false;
    out.cell  = m[1];
    out.param = m[2].matched ? m[2].str() : "";
    out.inst  = m[3];
    // 找到這個 '(' 的位置（用來切連線表）
    auto at = shadow.find('(', (size_t)m.position(0));
    out.open_paren_pos = at;
    return true;
}
void vparse::VerilogDesign::dump_instances(const std::string& path) const {
    std::ofstream fout(path);
    if (!fout) {
        std::cerr << "[Error] Cannot open dump file: " << path << "\n";
        return;
    }

    for (const auto& mod : modules) {
        fout << "Module " << mod.name
             << "  FF count=" << mod.ff_instances.size() << "\n";
        for (const auto& ffi : mod.ff_instances) {
            fout << "  " << ffi.inst_name
                 << "  macro=" << ffi.cell_macro << "\n";
            for (const auto& [pin, net] : ffi.pin2net) {
                fout << "     ." << pin << " -> " << net << "\n";
            }
        }
        fout << "\n";
    }

    fout.close();
    std::cout << "[Dump] Wrote Verilog instances to " << path << "\n";
}
// 解析連線列表：named 或 positional
static void parse_port_list(const std::string& port_blob_shadow,
                            bool& named,
                            std::unordered_map<std::string,std::string>& pin2net,
                            std::vector<std::string>& pos_ports)
{
    // 移除首尾括號與尾部分號
    std::string s = trim(port_blob_shadow);
    if (!s.empty() && s.front()=='(') s.erase(0,1);
    s = trim(s);
    if (!s.empty() && s.back()==';') s.pop_back();
    if (!s.empty() && s.back()==')') s.pop_back();
    s = trim(s);
    if (s.empty()) { named=true; return; }

    // 看看有沒有 ".NAME(" pattern
    static const std::regex re_named(R"(\.\s*[A-Za-z_]\w*\s*\()");
    named = std::regex_search(s, re_named);

    // 切分逗號，但要做 (),[],{} 平衡
    auto split_args = [](const std::string& t)->std::vector<std::string>{
        std::vector<std::string> out; std::string cur; cur.reserve(t.size());
        int p=0,b=0,c=0;
        for (size_t i=0;i<t.size();++i){
            char ch=t[i];
            if      (ch=='(') ++p;
            else if (ch==')') --p;
            else if (ch=='[') ++b;
            else if (ch==']') --b;
            else if (ch=='{') ++c;
            else if (ch=='}') --c;

            if (ch==',' && p==0 && b==0 && c==0) {
                out.push_back(trim(cur)); cur.clear();
            } else cur.push_back(ch);
        }
        if (!trim(cur).empty()) out.push_back(trim(cur));
        return out;
    };

    auto items = split_args(s);
    if (named) {
        // .PIN(expr) 形式
        static const std::regex one(R"(^\s*\.\s*([A-Za-z_]\w*)\s*\((.*)\)\s*$)");
        for (auto& it : items) {
            std::smatch m;
            if (std::regex_match(it, m, one)) {
                std::string pin = trim(m[1]);
                std::string ex  = trim(m[2]);
                pin2net[pin] = ex;
            } else {
                // 容錯：丟到位置式清單以免資料遺失
                pos_ports.push_back(it);
            }
        }
    } else {
        // 位置式
        pos_ports = std::move(items);
    }
}

// ========== 主解析：把一個 module 區段切成語句，抓 FF instance ==========

static void parse_module_body(const std::string& module_text,
                              VerilogModule& mod_out)
{
    size_t i = 0, N = module_text.size();
    while (i < N) {
        // 找「下一個語句」的結尾（含 ';'）
        size_t end = find_stmt_end(module_text, i);
        std::string stmt_original = module_text.substr(i, end - i);
        std::string shadow = strip_comments(stmt_original);

        // 嘗試當作 instance
        InstHead ih;
        bool is_inst = parse_instance_head(shadow, ih);

        if (!is_inst) {
            // 非 instance，原樣當作 Raw
            VerilogModule::Chunk c;
            c.kind = VerilogModule::Chunk::Raw;
            c.text = std::move(stmt_original);
            mod_out.chunks.push_back(std::move(c));
            i = end;
            continue;
        }

        // 是 instance，看看是不是 FF
        if (!CompatParser::is_ff_master(ih.cell)) {
            // 不是 FF → 當作 Raw 以保留排版
            VerilogModule::Chunk c;
            c.kind = VerilogModule::Chunk::Raw;
            c.text = std::move(stmt_original);
            mod_out.chunks.push_back(std::move(c));
            i = end;
            continue;
        }

        // 解析 FF instance 連線
        // shadow: ... instName ( ... ); 我們要切出 "( ... );" 這一段
        size_t par_open = ih.open_paren_pos;
        if (par_open == std::string::npos) {
            // 容錯：當 Raw
            VerilogModule::Chunk c;
            c.kind = VerilogModule::Chunk::Raw;
            c.text = std::move(stmt_original);
            mod_out.chunks.push_back(std::move(c));
            i = end;
            continue;
        }

        // 在 "shadow" 中找到與 '(' 配對的尾端（對齊原文 offset 不重要，僅供解析）
        size_t k = par_open;
        int par=0;
        for (; k<shadow.size(); ++k) {
            char ch = shadow[k];
            if (ch=='(') ++par;
            else if (ch==')') { --par; if (par==0) break; }
        }
        // 把 "( ... );" 這段抓出（以 shadow）
        std::string port_blob = shadow.substr(par_open, k - par_open + 2); // +2 讓 ');' 都在

        FFInstance ffi;
        ffi.cell_macro = ih.cell;
        ffi.param_overrides = trim(ih.param);
        // 抓 instance name：ih.inst
        ffi.inst_name = ih.inst;
        ffi.original_text = stmt_original;

        parse_port_list(port_blob, ffi.named_port, ffi.pin2net, ffi.pos_ports);

        int id = (int)mod_out.ff_instances.size();
        mod_out.ff_instances.push_back(std::move(ffi));

        VerilogModule::Chunk c;
        c.kind = VerilogModule::Chunk::FF;
        c.ff_id = id;
        mod_out.chunks.push_back(std::move(c));

        i = end;
    }
}

// ========== 將整個檔案切成多個 module ==========

static VerilogDesign parse_stream(std::istream& fin, const std::string& vname) {
    std::ostringstream oss; oss << fin.rdbuf();
    std::string whole = oss.str();

    VerilogDesign design;

    size_t i=0, N=whole.size();
    while (i<N) {
        // 找到下一個 module
        size_t line_beg = i;
        // 從目前位置找下一個 "module"
        size_t mod_pos = whole.find("module", i);
        if (mod_pos == std::string::npos) break;

        // 把 module 之前的內容（如果在任何 module 外）直接丟掉或保留？
        // 多數 netlist 不會有 module 外內容；若要保留，可放在一個 dummy 模組或設計前綴。

        // 從 module 開始讀到 endmodule
        size_t body_start = mod_pos;
        // 找 endmodule 的位置（簡單找關鍵字，嚴謹可用計數器，這裡第一版足夠）
        size_t end_pos = whole.find("endmodule", body_start);
        if (end_pos == std::string::npos) end_pos = N;

        // 切出這段
        std::string module_chunk = whole.substr(body_start, end_pos - body_start);
        std::istringstream mis(module_chunk);
        std::string first_line; std::getline(mis, first_line);

        std::string modname;
        std::string first_shadow = strip_comments(first_line);
        if (!starts_with_module(first_shadow, modname)) {
            // 找不到 module 名，往後推
            i = end_pos + 9; // skip "endmodule"
            continue;
        }

        // 把 "module ... ;" 到 "endmodule" 整段做成 VerilogModule
        VerilogModule vm;
        vm.name = modname;

        // 我們要保留 module 頭與尾也在 chunks（Raw），中間身體再切語句
        // 先抓 module 頭到 ';'
        size_t head_end = module_chunk.find(';');
        if (head_end == std::string::npos) head_end = module_chunk.size();
        std::string module_head = module_chunk.substr(0, head_end + 1);

        VerilogModule::Chunk ch_head;
        ch_head.kind = VerilogModule::Chunk::Raw;
        ch_head.text = module_head + "\n";
        vm.chunks.push_back(std::move(ch_head));

        // 把剩餘內容（直到 endmodule）作為 body
        std::string module_body = (head_end+1 < module_chunk.size())
                                  ? module_chunk.substr(head_end+1)
                                  : std::string();

        // 去掉最後的 "endmodule"（保留到設計輸出時再補上）
        size_t tail_pos = module_body.rfind("endmodule");
        std::string body_only = (tail_pos != std::string::npos)
                              ? module_body.substr(0, tail_pos)
                              : module_body;

        // 切 body 為語句並擷取 FF
        parse_module_body(body_only, vm);

        // 加上 endmodule
        VerilogModule::Chunk ch_tail;
        ch_tail.kind = VerilogModule::Chunk::Raw;
        ch_tail.text = "endmodule\n";
        vm.chunks.push_back(std::move(ch_tail));

        // 收進設計
        design.mod_idx[vm.name] = design.modules.size();
        design.modules.push_back(std::move(vm));

        i = end_pos + std::string("endmodule").size();
    }

    (void)vname; // 目前未使用
    return design;
}

// ========== Public API ==========

VerilogDesign parse_verilog(const std::string& path) {
    std::ifstream fin(path);
    if (!fin) throw std::runtime_error("[VerilogParser] Cannot open: " + path);
    return parse_stream(fin, path);
}

VerilogDesign parse_string(const std::string& content, const std::string& virtual_name) {
    std::istringstream iss(content);
    return parse_stream(iss, virtual_name);
}

void write_module(std::ostream& os,
                  const VerilogModule& mod,
                  const std::function<std::string(const FFInstance&)>& emitFF)
{
    auto ends_nl = [](const std::string& s){
        return !s.empty() && s.back() == '\n';
    };

    bool last_ended_nl = true; // 假設一開始在新行

    for (const auto& c : mod.chunks) {
        if (c.kind == VerilogModule::Chunk::Raw) {
            os << c.text;
            last_ended_nl = ends_nl(c.text);   // 完全不改 Raw 的換行行為
        } else {
            const auto& ffi = mod.ff_instances[c.ff_id];
            std::string tmp = emitFF ? emitFF(ffi) : std::string();
            bool replaced = !tmp.empty();      // 有回字串 => 你打算替換/刪除
            std::string repl = replaced ? tmp : ffi.original_text;

            // 只在「有替換且上一段沒換行且這次不是純刪除」時，補一個換行
            if (replaced && repl != "\n" && !last_ended_nl) {
                os << '\n';
                last_ended_nl = true;
            }

            os << repl;
            last_ended_nl = ends_nl(repl);
        }
    }
}

void write_design(std::ostream& os,
                  const VerilogDesign& design,
                  const std::function<std::string(const FFInstance&)>& emitFF)
{
    for (const auto& m : design.modules) {
        write_module(os, m, emitFF);
        os << "\n"; // 模組間保留一行
    }
}

} // namespace vparse
