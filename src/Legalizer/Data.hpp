#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../LefDefParser.h"

// Forward declare
struct Cluster;
struct Row;
struct SubRow;

struct Cell {
    using ptr = std::shared_ptr<Cell>;

    std::string name;
    double width, height;
    int x, y;          // 原始座標
    int optimalX, optimalY;
    double weight;

    Cell();
    Cell(const std::string &name, double width, double height, int x, int y);

    double displacement() const;
};

struct Cluster {
    using ptr = std::shared_ptr<Cluster>;
    double x;
    double weight;
    double q;
    double width;
    Cluster::ptr predecessor;

    std::vector<Cell*> member;   // ★ Legalizer 用來存放 Cell

    Cluster();
    Cluster(double x, Cluster::ptr predecessor);
};

struct SubRow {
    using ptr = std::shared_ptr<SubRow>;
    int minX, maxX;
    int freeWidth;
    Cluster::ptr lastCluster;

    SubRow();
    SubRow(int minX, int maxX);
    void updateMinMax(int minX_, int maxX_);
};

struct Row {
    using ptr = std::shared_ptr<Row>;
    int x, y;
    double height;
    double siteWidth;
    std::vector<SubRow::ptr> subRows;

    Row();
    Row(int x, int y, double height, double siteWidth);

    // ★ 單參數：預設用 round
    int getSiteX(double nonalignX) const;

    // ★ 雙參數：可指定 floor/ceil/round
    int getSiteX(double nonalignX, double (*func)(double)) const;
};

struct Input {
    std::vector<Cell::ptr> cells;
    std::vector<Cell::ptr> blockages;
    std::vector<Row::ptr> rows;

    lef::Lef* lef_;
    def::Def* def_;

    int maxDisplacementInSite = 100; // ★ 給 Legalizer 使用

    Input();
};
