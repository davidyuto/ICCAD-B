
/**
 * @file    DefWriter.cpp
 * @author  Jinwook Jung (jinwookjung@kaist.ac.kr)
 * @date    2018-10-24 17:16:17
 *
 * Created on Wed Oct 24 17:16:17 2018.
 */

#include "DefWriter.h"
#include "def/defwWriter.hpp"

using namespace my_lefdef;

/**
 * Default ctor.
 */
DefWriter::DefWriter ()
{
    //
}

/**
 * Returns the singleton object of the detailed router.
 */
DefWriter& DefWriter::get_instance ()
{
    static DefWriter ldp;
    return ldp;
}

#define CHECK_STATUS(status) \
  if (status) {              \
     defwPrintError(status); \
     return;         \
  }

static void write_rows (def::Def* def)
{
    auto& rows = def->get_rows();

    for (auto& r : rows) {
        auto status = defwRow(r->name_.c_str(), r->macro_.c_str(), r->x_, r->y_, r->orient_,
                              r->num_x_, r->num_y_, r->step_x_, r->step_y_);
        CHECK_STATUS(status);
    }
}

static void write_tracks (def::Def* def)
{
    auto& tracks = def->get_tracks();

    for (auto& t : tracks) {
        auto layers = (const char**)malloc(sizeof(char*)*1);
        layers[0] = strdup(t->layer_.c_str());

        string dir_str = t->direction_ == TrackDir::x ? "X" : "Y";
        auto status = defwTracks(dir_str.c_str(), 
                                 t->location_, t->num_tracks_, t->step_, 
                                 1, layers);
        CHECK_STATUS(status);
        free((char*)layers[0]);
        free((char*)layers);
    }

    auto status = defwNewLine();
    CHECK_STATUS(status);
}

static void write_gcell_grids (def::Def* def)
{
    auto& gcell_grids = def->get_gcell_grids();

    for (auto& g : gcell_grids) {
        string dir_str = g->direction_ == TrackDir::x ? "X" : "Y";
        auto status = defwGcellGrid(dir_str.c_str(), g->location_, g->num_, g->step_);
        CHECK_STATUS(status);
    }
    
    auto status = defwNewLine();
    CHECK_STATUS(status);
}

// static void write_components (def::Def* def)
// {
//     auto& component_umap = def->get_component_umap();

//     auto status = defwStartComponents(component_umap.size());
//     CHECK_STATUS(status);

//     for (auto it : component_umap) {
//         auto c = it.second;

//         string status_str = "UNPLACED";
//         if (c->is_fixed_) {
//             status_str = "FIXED";
//         }
//         else if (c->is_placed_) {
//             status_str = "PLACED";
//         }

//         if (c->orient_str_ == "") {
//             c->orient_str_ = "N";
//         }

//         status = defwComponentStr(
//                     c->name_.c_str(), c->ref_name_.c_str(), 
//                     0, NULL, NULL, NULL, NULL, NULL,    // Optionals
//                     0, NULL, NULL, NULL, NULL, status_str.c_str(),
//                     c->x_, c->y_, c->orient_str_.c_str(),
//                     0, NULL, 0, 0, 0, 0);
//         CHECK_STATUS(status);
//     }

//     status = defwEndComponents();
//     CHECK_STATUS(status);
// }

static void write_components (def::Def* def, const std::vector<my_lefdef::MBFFGroup>* mbff_groups = nullptr)
{
    auto& component_umap = def->get_component_umap();
    
    // 準備要輸出的 components
    std::vector<def::ComponentPtr> components_to_write;
    
    // 建立 FF 名稱集合用於過濾
    auto& ldp = my_lefdef::LefDefParser::get_instance();
    const auto& ffs   = ldp.getFFs();
    std::unordered_set<std::string> ff_names;
    for(const auto& ff : ffs) {
        ff_names.insert(ff.name);
    }
    
    
    // 過濾掉 FF，保留其他 components
    for (auto it : component_umap) {
        auto c = it.second;
        if (ff_names.find(c->name_) == ff_names.end()) {
            // 不是 FF，保留
            components_to_write.push_back(c);
        }
    }

    std::unordered_map<int, std::string> y_to_orientation;
    auto& rows = def->get_rows();
    for (const auto& row : rows) {
        y_to_orientation[row->y_] = row->orient_str_;
    }
    
    // 計算總數：非FF components + MBFFGroups
    size_t total_components = components_to_write.size();
    if (mbff_groups) {
        total_components += mbff_groups->size();
    }
    
    auto status = defwStartComponents(total_components);
    CHECK_STATUS(status);
    
    // 先寫非FF components
    for (auto c : components_to_write) {
        string status_str = "UNPLACED";
        if (c->is_fixed_) {
            status_str = "FIXED";
        }
        else if (c->is_placed_) {
            status_str = "PLACED";
        }

        if (c->orient_str_ == "") {
            c->orient_str_ = "N";
        }

        status = defwComponentStr(
                    c->name_.c_str(), c->ref_name_.c_str(), 
                    0, NULL, NULL, NULL, NULL, NULL,
                    0, NULL, NULL, NULL, NULL, status_str.c_str(),
                    c->x_, c->y_, c->orient_str_.c_str(),
                    0, NULL, 0, 0, 0, 0);
        CHECK_STATUS(status);
    }
    
    // 再寫 MBFFGroups，根據所在的 row 決定 orientation
    if (mbff_groups) {
        for (const auto& group : *mbff_groups) {
            // 根據 MBFFGroup 的 y 座標查找對應的 row orientation
            std::string orientation = "N";  // 默認值
            auto it = y_to_orientation.find(group.place_y);
            if (it != y_to_orientation.end()) {
                orientation = it->second;
            }
            
            status = defwComponentStr(
                        group.inst_name.c_str(),    // instance name
                        group.macro.c_str(),         // macro/library cell name
                        0, NULL, NULL, NULL, NULL, NULL,
                        0, NULL, NULL, NULL, NULL, "PLACED",
                        group.place_x, group.place_y, orientation.c_str(),  // 使用動態決定的 orientation
                        0, NULL, 0, 0, 0, 0);
            CHECK_STATUS(status);
        }
    }

    status = defwEndComponents();
    CHECK_STATUS(status);
}

static void write_pins (def::Def* def)
{
    auto& pin_umap = def->get_pin_umap();

    auto status = defwStartPins(pin_umap.size());
    CHECK_STATUS(status);

    for (auto it : pin_umap) {
        auto p = it.second;

        string direction_str = "INOUT";
        if (p->dir_ == PinDir::input) {
            direction_str = "INPUT";
        }
        else if (p->dir_ == PinDir::output) {
            direction_str = "OUTPUT";
        }

        // 檢查必要參數是否有效
        string orient = p->orient_str_.empty() ? "N" : p->orient_str_;
        string layer = p->layer_.empty() ? "M1" : p->layer_;
        
        // 確保座標有效
        if (p->lx_ == 0 && p->ly_ == 0 && p->ux_ == 0 && p->uy_ == 0) {
            // 給一個預設的矩形大小
            p->ux_ = 100;
            p->uy_ = 100;
        }

        auto status = defwPinStr(p->name_.c_str(), p->net_name_.c_str(), 
                                 0, direction_str.c_str(), 
                                 "SIGNAL", "FIXED", 
                                 p->x_, p->y_, orient.c_str(), layer.c_str(),
                                 p->lx_, p->ly_, p->ux_, p->uy_);
        CHECK_STATUS(status);
    }

    status = defwEndPins();
    CHECK_STATUS(status);
}

static void write_special_nets (def::Def* def)
{
    auto& special_net_umap = def->get_special_net_umap();

    auto status = defwStartSpecialNets(special_net_umap.size());
    CHECK_STATUS(status);

    for (auto it : special_net_umap) {
        auto n = it.second;
        status = defwSpecialNet(n->name_.c_str());
        CHECK_STATUS(status);

        for (auto con : n->connections_) {
            if (con->component_ != nullptr) {
                status = defwSpecialNetConnection(con->component_->name_.c_str(), 
                                  con->lef_pin_->name_.c_str(), 0);
            }
            else if (con->pin_ != nullptr) {
                status = defwSpecialNetConnection("PIN", con->pin_->name_.c_str(), 0);
            }
            else {
                status = defwSpecialNetConnection("PIN", con->name_.c_str(), 0);
            }
            CHECK_STATUS(status);
        }

        if (n->wires_.empty() == false) {
//            status = defwSpecialNetPathStart("ROUTED");
//            status = defwSpecialNetPathEnd();
        }

        status = defwSpecialNetEndOneNet();
        CHECK_STATUS(status);
    }

    status = defwEndSpecialNets();
    CHECK_STATUS(status);
}

static void write_nets (def::Def* def)
{
    auto& net_umap = def->get_net_umap();

    auto status = defwStartNets(net_umap.size());
    CHECK_STATUS(status);

    for (auto it : net_umap) {
        auto n = it.second;

        status = defwNet(n->name_.c_str());
        CHECK_STATUS(status);

        for (auto con : n->connections_) {
            if (con->component_ != nullptr) {
                status = defwNetConnection(con->component_->name_.c_str(), 
                                  con->lef_pin_->name_.c_str(), 0);
            }
            else {
                status = defwNetConnection("PIN", con->pin_->name_.c_str(), 0);
            }
            CHECK_STATUS(status);
        }

        status = defwNetEndOneNet();
        CHECK_STATUS(status);
    }

    status = defwEndNets();
    CHECK_STATUS(status);
}

// 與 DefWriter 整合的輔助函數
void writeNetsFromVerilog(def::Def* def, const VerilogParser& parser) {
    const auto& nets = parser.getNets();

    auto status = defwStartNets(nets.size());
    CHECK_STATUS(status);

    for (const auto& net_pair : nets) {
        const auto& net = net_pair.second;

        status = defwNet(net.net_name.c_str());
        CHECK_STATUS(status);

        // 嘗試設置格式選項（如果支援）
        //defwAddIndent();  // 增加縮排

        for (const auto& conn : net.connections) {
            status = defwNetConnection(conn.first.c_str(), conn.second.c_str(), 0);
            CHECK_STATUS(status);
        }

        status = defwNetUse("SIGNAL");
        CHECK_STATUS(status);

        status = defwNetEndOneNet();
        CHECK_STATUS(status);
    }

    status = defwEndNets();
    CHECK_STATUS(status);
}

void DefWriter::write_def (def::Def& def, const std::vector<MBFFGroup>* mbff_groups, const VerilogParser& parser, string filename)
{
    def_ = &def;

    FILE* fout = fopen(filename.c_str(), "w");
    if (fout == nullptr) {
        fprintf(stderr, "ERROR: could not open output file\n");
        return;
    }

    int status;    // return code, if none 0 means error
    status = defwInitCbk(fout);
    CHECK_STATUS(status);
    status = defwVersion (5, 8);
    CHECK_STATUS(status);
    status = defwDividerChar("/");
    CHECK_STATUS(status);
    status = defwBusBitChars("[]");
    CHECK_STATUS(status);
    status = defwDesignName(def_->get_design_name().c_str());
    CHECK_STATUS(status);
    status = defwUnits(def_->get_dbu());
    CHECK_STATUS(status);

    status = defwNewLine();
    CHECK_STATUS(status);

    // history
    status = defwHistory("Placed with ComPLx.");
    CHECK_STATUS(status);
    status = defwNewLine();
    CHECK_STATUS(status);

    // Die area
    status = defwDieArea(def_->get_die_lx(), def_->get_die_ly(),
                         def_->get_die_ux(), def_->get_die_uy());
    CHECK_STATUS(status);

    status = defwNewLine();
    CHECK_STATUS(status);

    // Rows
    write_rows(def_);

    // Tracks
    write_tracks(def_);

    // GCell grid
    write_gcell_grids(def_);

    // Components
    write_components(def_, mbff_groups);

    // Pins
    write_pins(def_);

    // Special Nets
    write_special_nets(def_);

    // Nets
    //write_nets(def_);
    writeNetsFromVerilog(def_,parser);

    status = defwEnd();
    CHECK_STATUS(status);

    auto lineNumber = defwCurrentLineNumber();
    if (lineNumber == 0) {
        fprintf(stderr, "ERROR: nothing has been read.\n");
    }
    fclose(fout);
}
