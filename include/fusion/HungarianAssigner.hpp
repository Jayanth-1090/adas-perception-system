#pragma once
#include <vector>
#include <utility>

namespace adas {

struct AssignmentResult {
    std::vector<std::pair<int,int>> matched;
    std::vector<int> unmatched_tracks;
    std::vector<int> unmatched_detections;
};

class HungarianAssigner {
public:
    // n_dets passed explicitly — cost_matrix may be empty when n_tracks=0
    static AssignmentResult assign(
        const std::vector<std::vector<float>>& cost_matrix,
        int n_dets,
        float max_cost
    );

private:
    static std::vector<int> hungarian(std::vector<std::vector<float>> cost);
};

} // namespace adas
