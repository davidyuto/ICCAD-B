#pragma once
#include "ResultWriter.hpp"
#include "Data.hpp"
#include "../LefDefParser.h"
#include "../Def.h"
#include "../Lef.h"
#include "../PlacementStructure.h"
#include <utility>

class Legalizer
{
    lef::Lef* lef_;
    def::Def* def_;
    // lef_ = lef::Lef::get_instance();
    // def_ = def::Def::get_instance();
    Input *input;
    double maxDisplacementConstraint;

    int getRowIdx(const Cell *cell) const;
    int getSubRowIdx(const Row *row, const Cell *cell) const;
    std::pair<double, double> getTotalAndMaxDisplacement() const;

    void divideRow();
    std::pair<int, double> placeRowTrial(const Row *row, Cell *cell, bool addPenalty);
    void placeRowFinal(SubRow *subRow, Cell *cell);
    void abacusProcess();
    void determinePosition();

public:
    Legalizer(Input *input);
    ResultWriter::ptr solve();
};
