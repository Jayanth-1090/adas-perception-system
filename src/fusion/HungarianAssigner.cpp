#include "fusion/HungarianAssigner.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <limits>

namespace adas {

AssignmentResult HungarianAssigner::assign(
    const std::vector<std::vector<float>>& cost_matrix,
    int n_dets,
    float max_cost)
{
    AssignmentResult result;

    const int n_tracks = static_cast<int>(cost_matrix.size());
    // n_dets passed explicitly — do not derive from cost_matrix (may be empty when n_tracks=0)

    spdlog::debug("Hungarian::assign n_tracks={} n_dets={}", n_tracks, n_dets);

    if (n_tracks == 0) {
        for (int j = 0; j < n_dets; ++j)
            result.unmatched_detections.push_back(j);
        spdlog::debug("Hungarian: all {} dets unmatched (no tracks)", n_dets);
        return result;
    }
    if (n_dets == 0) {
        for (int i = 0; i < n_tracks; ++i)
            result.unmatched_tracks.push_back(i);
        return result;
    }

    const int   N   = std::max(n_tracks, n_dets);
    const float BIG = 1e6f;
    std::vector<std::vector<float>> square(N, std::vector<float>(N, BIG));

    for (int i = 0; i < n_tracks; ++i)
        for (int j = 0; j < n_dets; ++j)
            square[i][j] = cost_matrix[i][j];

    auto assignment = hungarian(square);

    std::vector<bool> det_matched(n_dets, false);

    for (int i = 0; i < n_tracks; ++i) {
        int j = assignment[i];
        if (j >= 0 && j < n_dets && cost_matrix[i][j] <= max_cost) {
            result.matched.emplace_back(i, j);
            det_matched[j] = true;
            spdlog::debug("Hungarian: matched track {} → det {}, cost={:.2f}",
                          i, j, cost_matrix[i][j]);
        } else {
            result.unmatched_tracks.push_back(i);
            spdlog::debug("Hungarian: track {} unmatched (j={}, cost={:.2f})",
                          i, j, j >= 0 && j < n_dets ? cost_matrix[i][j] : -1.f);
        }
    }

    for (int j = 0; j < n_dets; ++j)
        if (!det_matched[j])
            result.unmatched_detections.push_back(j);

    spdlog::debug("Hungarian: matched={} unmatched_tracks={} unmatched_dets={}",
                  result.matched.size(),
                  result.unmatched_tracks.size(),
                  result.unmatched_detections.size());

    return result;
}

std::vector<int> HungarianAssigner::hungarian(
    std::vector<std::vector<float>> cost)
{
    const int N = static_cast<int>(cost.size());
    const float INF = std::numeric_limits<float>::max() / 2.f;

    for (int i = 0; i < N; ++i) {
        float row_min = *std::min_element(cost[i].begin(), cost[i].end());
        for (int j = 0; j < N; ++j) cost[i][j] -= row_min;
    }

    for (int j = 0; j < N; ++j) {
        float col_min = INF;
        for (int i = 0; i < N; ++i) col_min = std::min(col_min, cost[i][j]);
        for (int i = 0; i < N; ++i) cost[i][j] -= col_min;
    }

    std::vector<int> row_cover(N, 0);
    std::vector<int> col_cover(N, 0);
    std::vector<int> starred(N, -1);
    std::vector<int> primed(N,  -1);
    std::vector<int> col_starred(N, -1);

    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (cost[i][j] == 0.f && col_starred[j] == -1) {
                starred[i]     = j;
                col_starred[j] = i;
                break;
            }
        }
    }

    for (int iter = 0; iter < N * N + N; ++iter) {
        std::fill(col_cover.begin(), col_cover.end(), 0);
        int covered = 0;
        for (int i = 0; i < N; ++i) {
            if (starred[i] >= 0) { col_cover[starred[i]] = 1; ++covered; }
        }
        if (covered >= N) break;

        int pivot_row = -1, pivot_col = -1;
        bool found = false;

        while (!found) {
            for (int i = 0; i < N && !found; ++i) {
                if (row_cover[i]) continue;
                for (int j = 0; j < N && !found; ++j) {
                    if (!col_cover[j] && cost[i][j] == 0.f) {
                        primed[i] = j;
                        pivot_row = i;
                        pivot_col = j;
                        found     = true;
                    }
                }
            }

            if (!found) {
                float min_val = INF;
                for (int i = 0; i < N; ++i)
                    if (!row_cover[i])
                        for (int j = 0; j < N; ++j)
                            if (!col_cover[j])
                                min_val = std::min(min_val, cost[i][j]);

                for (int i = 0; i < N; ++i)
                    for (int j = 0; j < N; ++j) {
                        if (!row_cover[i] && !col_cover[j]) cost[i][j] -= min_val;
                        if ( row_cover[i] &&  col_cover[j]) cost[i][j] += min_val;
                    }
            }
        }

        if (starred[pivot_row] == -1) {
            std::vector<std::pair<int,int>> path;
            path.emplace_back(pivot_row, pivot_col);

            while (true) {
                int r = col_starred[path.back().second];
                if (r == -1) break;
                path.emplace_back(r, path.back().second);
                path.emplace_back(r, primed[r]);
            }

            for (auto& [r, c] : path) {
                if (starred[r] == c) { starred[r] = -1; col_starred[c] = -1; }
                else                 { starred[r] = c;  col_starred[c] = r;  }
            }

            std::fill(row_cover.begin(), row_cover.end(), 0);
            std::fill(col_cover.begin(), col_cover.end(), 0);
            std::fill(primed.begin(),    primed.end(),   -1);
        } else {
            row_cover[pivot_row]          = 1;
            col_cover[starred[pivot_row]] = 0;
        }
    }

    return starred;
}

} // namespace adas
