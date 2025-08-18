#pragma once

#include "LefDefParser.h"
#include "Cluster.h"
#include <vector>

namespace my_lefdef {

class Banking {
public:
    explicit Banking(std::vector<FlipFlop>& ffs);
    void run();
    const std::vector<Cluster>& getClusters() const { return clusters_; }

private:
    std::vector<FlipFlop>& ffs_;
    std::vector<Cluster> clusters_;
    std::vector<int> bitOrder_;

    void bitOrdering();

    std::vector<FlipFlop*> chooseCandidateFF(const FlipFlop& nowFF,
                                             std::vector<FlipFlop>& allFFs,
                                             int targetBit);
};

}  // namespace my_lefdef
