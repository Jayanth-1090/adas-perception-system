#pragma once
#include "common/Types.hpp"
#include <opencv2/core.hpp>
#include <vector>

namespace adas {

class BirdsEyeView {
public:
    // size: pixel dimensions of the BEV panel
    // range_m: how many meters forward/sideways to show
    BirdsEyeView(int width = 400, int height = 500, float range_m = 60.0f);

    // Render BEV frame from current snapshot
    cv::Mat render(const SystemSnapshot& snapshot,
                   const std::vector<ThreatAssessment>& threats);

private:
    int   width_, height_;
    float range_m_;

    // World → pixel coordinate conversion
    cv::Point worldToPixel(float x_m, float y_m) const;

    // Get color for threat level
    static cv::Scalar threatColor(ThreatLevel level);
    static cv::Scalar threatColorByString(const std::string& s);

    void drawGrid(cv::Mat& img) const;
    void drawEgoVehicle(cv::Mat& img) const;
    void drawObject(cv::Mat& img, const FusedObject& obj,
                    ThreatLevel level) const;
    void drawVelocityArrow(cv::Mat& img, const FusedObject& obj) const;
};

} // namespace adas
