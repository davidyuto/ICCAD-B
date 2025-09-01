
#include "ResultWriter.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>

ResultWriter::ResultWriter() {}

void ResultWriter::addCell(const Cell *cell)
{
    cells.emplace_back(cell->name, cell->optimalX, cell->optimalY);
}

void ResultWriter::addBlockage(const Cell *blockage)
{
    blockages.emplace_back(blockage->name, blockage->x, blockage->y);
}

void ResultWriter::write(const std::string &filepath) const
{
    std::ofstream fout(filepath);
    if (!fout.is_open())
    {
        std::cerr << "[Error] Cannot open \"" << filepath << "\".\n";
        exit(EXIT_FAILURE);
    }

    for (const auto &[name, x, y] : cells)
        fout << name << " " << x << " " << y << " : N\n";
    for (const auto &[name, x, y] : blockages)
        fout << name << " " << x << " " << y << " : N /FIXED\n";
}

void ResultWriter::write_to_MbffGroup(std::vector<my_lefdef::MBFFGroup> *mbff_groups_) {
    if (!mbff_groups_) {
        std::cerr << "[Error] mbff_groups_ is nullptr\n";
        return;
    }
    std::unordered_map<std::string, my_lefdef::MBFFGroup*> name_to_group;
    for (auto& group : *mbff_groups_) {
        name_to_group[group.inst_name] = &group;
    }
    
    // 更新每個 cell 的座標到對應的 MBFFGroup
    for (const auto& [name, x, y] : cells) {
        auto it = name_to_group.find(name);
        if (it != name_to_group.end()) {
            it->second->place_x = x;
            it->second->place_y = y;
        } else {
            std::cerr << "[Warning] Cell \"" << name 
                      << "\" not found in mbff_groups\n";
        }
    }
    
    std::cout << "[Info] Updated " << cells.size() 
              << " cell positions to MBFFGroups\n";
}
