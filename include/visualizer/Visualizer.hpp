#pragma once
#include "common/Types.hpp"
#include "visualizer/BirdsEyeView.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace adas {

class Visualizer {
public:
    Visualizer(int win_w = 1600, int win_h = 900);
    ~Visualizer();

    // Render one frame — returns keyboard input char (or -1)
    int render(const cv::Mat&          video_frame,
               const SystemSnapshot&   snapshot,
               const std::string&      scene_name,
               int                     frame_idx,
               int                     total_frames,
               PlaybackState           state);

    // Returns true if window was closed
    bool windowClosed() const { return closed_; }

private:
    int          win_w_, win_h_;
    bool         closed_ = false;
    BirdsEyeView bev_;

    // Panel dimensions
    int video_w_, video_h_;
    int bev_w_,   bev_h_;
    int table_h_ = 120;
    int bar_h_   = 40;

    void drawDetections(cv::Mat& frame,
                        const SystemSnapshot& snapshot,
                        float scale_x, float scale_y) const;

    void drawTrackTable(cv::Mat& canvas,
                        const SystemSnapshot& snapshot,
                        int x, int y, int w, int h) const;

    void drawStatusBar(cv::Mat& canvas,
                       const std::string& scene,
                       int frame_idx, int total_frames,
                       const std::string& overall_threat,
                       PlaybackState state,
                       int x, int y, int w, int h) const;

    static cv::Scalar threatColor(const std::string& level);
    static cv::Scalar threatColorByLevel(ThreatLevel l);
    static std::string stateLabel(PlaybackState s);
};

} // namespace adas
