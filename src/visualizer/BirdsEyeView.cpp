#include "visualizer/BirdsEyeView.hpp"
#include <opencv2/imgproc.hpp>
#include <map>
#include <algorithm>

namespace adas {

BirdsEyeView::BirdsEyeView(int w, int h, float range)
    : width_(w), height_(h), range_m_(range) {}

cv::Mat BirdsEyeView::render(const SystemSnapshot& snapshot,
                              const std::vector<ThreatAssessment>& threats) {
    cv::Mat img(height_, width_, CV_8UC3, cv::Scalar(15, 15, 25));

    drawGrid(img);
    drawEgoVehicle(img);

    // Build threat lookup: track_id → level
    std::map<uint32_t, ThreatLevel> threat_map;
    for (const auto& t : threats)
        threat_map[t.track_id] = t.level;

    // Draw each fused object
    for (const auto& obj : snapshot.fused_objects) {
        ThreatLevel lvl = ThreatLevel::SAFE;
        auto it = threat_map.find(obj.track_id);
        if (it != threat_map.end()) lvl = it->second;

        drawObject(img, obj, lvl);
        drawVelocityArrow(img, obj);
    }

    // Legend
    cv::putText(img, "BIRD'S EYE VIEW",
                {10, 20}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                {180, 180, 180}, 1);
    cv::putText(img, "EGO",
                {width_/2 - 10, height_ - 35},
                cv::FONT_HERSHEY_SIMPLEX, 0.4, {255,255,255}, 1);

    return img;
}

cv::Point BirdsEyeView::worldToPixel(float x_m, float y_m) const {
    // x=0 → center of image (ego lane center)
    // y=0 → bottom of image (ego front bumper)
    // y increases upward in world → decreases in image
    int px = static_cast<int>(width_  / 2.0f + (x_m / range_m_) * (width_  / 2.0f));
    int py = static_cast<int>(height_ - 60    - (y_m / range_m_) * (height_ - 80.0f));
    return {px, py};
}

cv::Scalar BirdsEyeView::threatColor(ThreatLevel level) {
    switch (level) {
        case ThreatLevel::CRITICAL: return {0, 0, 255};    // red
        case ThreatLevel::WARNING:  return {0, 165, 255};  // orange
        default:                    return {0, 220, 0};    // green
    }
}

void BirdsEyeView::drawGrid(cv::Mat& img) const {
    // Horizontal range rings every 10m
    for (int d = 10; d <= static_cast<int>(range_m_); d += 10) {
        auto pt = worldToPixel(0, static_cast<float>(d));
        cv::line(img, {0, pt.y}, {width_, pt.y},
                 {40, 40, 55}, 1, cv::LINE_AA);
        cv::putText(img, std::to_string(d) + "m",
                    {5, pt.y - 3}, cv::FONT_HERSHEY_SIMPLEX,
                    0.35, {80, 80, 100}, 1);
    }
    // Ego lane center line
    cv::line(img, {width_/2, 0}, {width_/2, height_},
             {50, 50, 70}, 1, cv::LINE_AA);
    // Lane boundaries at ±1.75m
    auto lL = worldToPixel(-1.75f, 0);
    auto lR = worldToPixel( 1.75f, 0);
    cv::line(img, {lL.x, 0}, {lL.x, height_}, {60, 60, 30}, 1, cv::LINE_AA);
    cv::line(img, {lR.x, 0}, {lR.x, height_}, {60, 60, 30}, 1, cv::LINE_AA);
}

void BirdsEyeView::drawEgoVehicle(cv::Mat& img) const {
    auto center = worldToPixel(0, 0);
    cv::rectangle(img,
        {center.x - 8, center.y - 15},
        {center.x + 8, center.y + 5},
        {100, 200, 255}, -1);
    // Direction arrow
    cv::arrowedLine(img,
        {center.x, center.y - 15},
        {center.x, center.y - 30},
        {255, 255, 255}, 2, cv::LINE_AA, 0, 0.4);
}

void BirdsEyeView::drawObject(cv::Mat& img, const FusedObject& obj,
                               ThreatLevel level) const {
    auto pt  = worldToPixel(obj.x, obj.y);
    auto col = threatColor(level);

    // Filled circle
    cv::circle(img, pt, 8, col, -1, cv::LINE_AA);
    cv::circle(img, pt, 8, {255,255,255}, 1, cv::LINE_AA);

    // Label: track ID + label
    std::string lbl = "#" + std::to_string(obj.track_id) +
                      " " + obj.label.substr(0,3);
    cv::putText(img, lbl, {pt.x + 10, pt.y + 4},
                cv::FONT_HERSHEY_SIMPLEX, 0.35, col, 1);
}

void BirdsEyeView::drawVelocityArrow(cv::Mat& img,
                                      const FusedObject& obj) const {
    auto from = worldToPixel(obj.x, obj.y);

    // Scale velocity for display (1 m/s = 5px)
    float scale = 5.0f;
    int   dx    = static_cast<int>( obj.vx * scale);
    int   dy    = static_cast<int>(-obj.vy * scale);  // flip y for image coords

    if (std::abs(dx) < 2 && std::abs(dy) < 2) return;  // too small to draw

    cv::Point to = {from.x + dx, from.y + dy};
    cv::arrowedLine(img, from, to, {255, 255, 100},
                    1, cv::LINE_AA, 0, 0.3);
}

} // namespace adas
