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

// 单比特 Flip-Flop 描述
struct FlipFlop {
    std::string name;
    int x;
    int y;
};


struct MBFF {
    std::string group;             // 以 component 名称去掉末尾“_bit”编号得到
    std::vector<FlipFlop> bits;    // 每一位的实例和坐标
};

class LefDefParser
{
public:
    // 单例访问
    static LefDefParser& get_instance();

    // 读取 LEF → 调用 Lef::read_lef / report
    void read_lef(const std::string &filename);

    // 读取 DEF → 调用 Def::read_def / report 并自动 extractFlipFlops
    void read_def(const std::string &filename);

    // 从 DEF 解析后的组件里，提取所有单比特 FF 与多比特 FF 群组
    void extractFlipFlops();

    // 访问结果
    const std::vector<FlipFlop>& getFFs() const { return ffs_; }
    const std::vector<MBFF>&    getMBFFs() const { return mbffs_; }


    void write_bookshelf      (const std::string &filename) const;
    void write_bookshelf_nodes(const std::string &filename) const;
    void write_bookshelf_nets (const std::string &filename) const;
    void write_bookshelf_wts  (const std::string &filename) const;
    void write_bookshelf_scl  (const std::string &filename) const;
    void write_bookshelf_pl   (const std::string &filename) const;
    void update_def           (const std::string &bookshelf_pl);

    def::Def& get_def();

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
