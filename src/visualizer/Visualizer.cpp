#include "visualizer/Visualizer.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <spdlog/spdlog.h>
#include <map>
#include <iomanip>
#include <sstream>

namespace adas {

Visualizer::Visualizer(int win_w, int win_h)
    : win_w_(win_w), win_h_(win_h), bev_(400, win_h - table_h_ - bar_h_)
{
    bev_w_   = 400;
    bev_h_   = win_h - table_h_ - bar_h_;
    video_w_ = win_w - bev_w_;
    video_h_ = bev_h_;

    cv::namedWindow("ADAS Perception Simulator",
                    cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::resizeWindow("ADAS Perception Simulator", win_w_, win_h_);
    spdlog::info("Visualizer: window created {}x{}", win_w_, win_h_);
}

Visualizer::~Visualizer() {
    cv::destroyAllWindows();
}

int Visualizer::render(const cv::Mat&        video_frame,
                        const SystemSnapshot& snapshot,
                        const std::string&    scene_name,
                        int                   frame_idx,
                        int                   total_frames,
                        PlaybackState         state)
{
    // ── Canvas ─────────────────────────────────────────────────
    cv::Mat canvas(win_h_, win_w_, CV_8UC3, cv::Scalar(10, 10, 18));

    // ── Video panel ────────────────────────────────────────────
    if (!video_frame.empty() &&
        video_frame.rows > 0 && video_frame.cols > 0) {
        cv::Mat resized;
        cv::resize(video_frame, resized, cv::Size(video_w_, video_h_));

        // Scale factors for detection overlay
        float sx = static_cast<float>(video_w_) / video_frame.cols;
        float sy = static_cast<float>(video_h_) / video_frame.rows;

        drawDetections(resized, snapshot, sx, sy);
        resized.copyTo(canvas(cv::Rect(0, 0, video_w_, video_h_)));
    } else {
        cv::putText(canvas, "No video frame",
                    {video_w_/2 - 80, video_h_/2},
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, {100,100,100}, 1);
    }

    // ── BEV panel ──────────────────────────────────────────────
    cv::Mat bev_img = bev_.render(snapshot, snapshot.threats);
    if (!bev_img.empty()) {
        cv::Mat bev_resized;
        cv::resize(bev_img, bev_resized, cv::Size(bev_w_, bev_h_));
        bev_resized.copyTo(canvas(cv::Rect(video_w_, 0, bev_w_, bev_h_)));
    }

    // ── Track table ────────────────────────────────────────────
    drawTrackTable(canvas, snapshot,
                   0, bev_h_, win_w_, table_h_);

    // ── Status bar ─────────────────────────────────────────────
    drawStatusBar(canvas, scene_name, frame_idx, total_frames,
                  snapshot.overall_threat, state,
                  0, bev_h_ + table_h_, win_w_, bar_h_);

    cv::imshow("ADAS Perception Simulator", canvas);
    int key = cv::waitKey(1);
    if (key == 27 || cv::getWindowProperty(
            "ADAS Perception Simulator",
            cv::WND_PROP_VISIBLE) < 1) {
        closed_ = true;
    }
    return key;
}

// ── drawDetections ─────────────────────────────────────────────
void Visualizer::drawDetections(cv::Mat& frame,
                                 const SystemSnapshot& snapshot,
                                 float scale_x, float scale_y) const
{
    // Build threat lookup
    std::map<uint32_t, ThreatLevel> threat_map;
    for (const auto& t : snapshot.threats)
        threat_map[t.track_id] = t.level;

    // ── Build detection lookup by label+position for track matching ──
    // Raw detections have bbox_px — we match them to fused tracks
    // by finding the closest raw detection to each track's world position
    // Scale factors: source frame → display frame
    float src_w = snapshot.raw_detections.empty() ? 1920.f :
                  static_cast<float>(snapshot.raw_detections[0].src_frame_w);
    float src_h = snapshot.raw_detections.empty() ? 1080.f :
                  static_cast<float>(snapshot.raw_detections[0].src_frame_h);
    float rx = static_cast<float>(frame.cols) / src_w;
    float ry = static_cast<float>(frame.rows) / src_h;

    // Draw each fused track using its closest raw detection bbox
    for (const auto& obj : snapshot.fused_objects) {
        ThreatLevel lvl = ThreatLevel::SAFE;
        auto it = threat_map.find(obj.track_id);
        if (it != threat_map.end()) lvl = it->second;
        auto col = threatColorByLevel(lvl);

        // Match raw detection to fused track
        // Strategy 1: exact label + proximity match
        // Strategy 2: if only one detection of this label, use it directly
        // Strategy 3: use closest by distance regardless (large threshold)
        const adas::Detection* best = nullptr;
        float best_dist = 999.f;

        // Count detections with same label
        int same_label_count = 0;
        for (const auto& d : snapshot.raw_detections)
            if (d.label == obj.label) same_label_count++;

        for (const auto& d : snapshot.raw_detections) {
            if (d.label != obj.label) continue;
            // If only one detection of this label — use it directly
            if (same_label_count == 1) {
                best = &d;
                break;
            }
            // Multiple same-label detections — pick closest by world dist
            float dx = d.x - obj.x;
            float dy = d.y - obj.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < best_dist) {
                best_dist = dist;
                best = &d;
            }
        }

        // Last resort: if no label match, use closest detection overall
        if (!best && !snapshot.raw_detections.empty()) {
            for (const auto& d : snapshot.raw_detections) {
                float dx = d.x - obj.x;
                float dy = d.y - obj.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist < best_dist) {
                    best_dist = dist;
                    best = &d;
                }
            }
        }

        int x1, y1, x2, y2, cx, cy;

        if (best && best->bbox_x2 > best->bbox_x1) {
            // Use real pixel bbox — scale to display resolution
            x1 = static_cast<int>(best->bbox_x1 * rx);
            y1 = static_cast<int>(best->bbox_y1 * ry);
            x2 = static_cast<int>(best->bbox_x2 * rx);
            y2 = static_cast<int>(best->bbox_y2 * ry);
            cx = (x1 + x2) / 2;
            cy = (y1 + y2) / 2;
        } else {
            // Fallback: project from world coords
            float focal_px = frame.rows * 1.2f;
            float height_m = (obj.label == "pedestrian") ? 1.75f : 1.5f;
            float dist     = std::max(obj.y, 1.0f);
            int   bh       = static_cast<int>((height_m * focal_px) / dist);
            int   bw       = static_cast<int>(bh * 0.7f);
            cx = static_cast<int>(frame.cols/2.f +
                 (obj.x/1.75f) * (frame.cols/4.f));
            cy = static_cast<int>(frame.rows * 0.6f);
            x1 = cx - bw/2; y1 = cy - bh;
            x2 = cx + bw/2; y2 = cy;
        }

        // Clamp
        x1 = std::max(0,x1); y1 = std::max(0,y1);
        x2 = std::min(frame.cols-1,x2);
        y2 = std::min(frame.rows-1,y2);
        if (x2 <= x1 || y2 <= y1) continue;
        cx = (x1+x2)/2; cy = (y1+y2)/2;

        // Bounding box
        cv::rectangle(frame, {x1,y1}, {x2,y2}, col, 2, cv::LINE_AA);

        // Label pill
        std::string lbl = "#" + std::to_string(obj.track_id) +
                          " " + obj.label + " " +
                          std::to_string(
                              static_cast<int>(obj.confidence*100)) + "%";
        int baseline = 0;
        auto sz = cv::getTextSize(lbl, cv::FONT_HERSHEY_SIMPLEX,
                                  0.45, 1, &baseline);
        int lx = std::max(0, x1);
        int ly = std::max(sz.height + 6, y1);
        cv::rectangle(frame, {lx, ly-sz.height-6},
                      {lx+sz.width+4, ly}, col, -1);
        cv::putText(frame, lbl, {lx+2, ly-4},
                    cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    {10,10,10}, 1, cv::LINE_AA);

        // TTC label below box
        for (const auto& ta : snapshot.threats) {
            if (ta.track_id == obj.track_id && ta.ttc > 0) {
                std::ostringstream ss;
                ss << "TTC:" << std::fixed
                   << std::setprecision(1) << ta.ttc << "s";
                cv::putText(frame, ss.str(), {x1, y2+15},
                            cv::FONT_HERSHEY_SIMPLEX, 0.4,
                            col, 1, cv::LINE_AA);
            }
        }

        // Velocity arrow
        if (std::abs(obj.vy) > 0.5f) {
            int len = static_cast<int>(std::abs(obj.vy) * 4.0f);
            len = std::min(len, 60);
            int dir = (obj.vy < 0) ? -1 : 1;
            cv::arrowedLine(frame, {cx, cy},
                            {cx, cy - dir*len},
                            {255,255,100}, 2, cv::LINE_AA, 0, 0.3);
        }
    }
}

// ── drawTrackTable ─────────────────────────────────────────────
void Visualizer::drawTrackTable(cv::Mat& canvas,
                                 const SystemSnapshot& snapshot,
                                 int x, int y, int w, int h) const
{
    cv::rectangle(canvas, {x, y}, {x+w, y+h},
                  {20, 20, 30}, -1);
    cv::line(canvas, {x, y}, {x+w, y},
             {60, 60, 80}, 1);

    // Header
    const int cols[] = {50, 130, 90, 90, 90, 90, 90, 110};
    const char* hdrs[] = {"ID","Label","Dist(m)","Lat(m)",
                           "Vx(m/s)","Vy(m/s)","TTC(s)","THREAT"};
    int cx_pos = x + 5;
    int hy = y + 18;

    for (int i = 0; i < 8; ++i) {
        cv::putText(canvas, hdrs[i], {cx_pos, hy},
                    cv::FONT_HERSHEY_SIMPLEX, 0.38,
                    {160, 160, 200}, 1);
        cx_pos += cols[i];
    }
    cv::line(canvas, {x, y+22}, {x+w, y+22}, {60,60,80}, 1);

    // Build threat lookup
    std::map<uint32_t, ThreatAssessment> threat_map;
    for (const auto& t : snapshot.threats)
        threat_map[t.track_id] = t;

    int row = 0;
    for (const auto& obj : snapshot.fused_objects) {
        if (row >= 4) break; // max 4 rows in table height

        int ry = y + 40 + row * 20;
        cx_pos = x + 5;

        auto col = cv::Scalar(200, 200, 200);
        ThreatLevel lvl = ThreatLevel::SAFE;
        float ttc = -1.f;

        auto it = threat_map.find(obj.track_id);
        if (it != threat_map.end()) {
            lvl = it->second.level;
            ttc = it->second.ttc;
            col = threatColorByLevel(lvl);
        }

        // Alternate row background
        if (row % 2 == 0)
            cv::rectangle(canvas, {x, ry-14}, {x+w, ry+6},
                          {25,25,35}, -1);

        auto fmt1 = [](float v) {
            std::ostringstream s;
            s << std::fixed << std::setprecision(1) << v;
            return s.str();
        };

        std::vector<std::string> vals = {
            std::to_string(obj.track_id),
            obj.label,
            fmt1(obj.y),
            fmt1(obj.x),
            fmt1(obj.vx),
            fmt1(obj.vy),
            ttc > 0 ? fmt1(ttc) : "--",
            lvl == ThreatLevel::CRITICAL ? "CRITICAL" :
            lvl == ThreatLevel::WARNING  ? "WARNING"  : "SAFE"
        };

        for (int i = 0; i < 8; ++i) {
            cv::Scalar c = (i == 7) ? col : cv::Scalar(200,200,200);
            cv::putText(canvas, vals[i], {cx_pos, ry},
                        cv::FONT_HERSHEY_SIMPLEX, 0.38, c, 1);
            cx_pos += cols[i];
        }
        ++row;
    }

    if (snapshot.fused_objects.empty()) {
        cv::putText(canvas, "No active tracks",
                    {x + w/2 - 60, y + h/2},
                    cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    {80,80,80}, 1);
    }
}

// ── drawStatusBar ──────────────────────────────────────────────
void Visualizer::drawStatusBar(cv::Mat& canvas,
                                const std::string& scene,
                                int frame_idx, int total_frames,
                                const std::string& overall_threat,
                                PlaybackState state,
                                int x, int y, int w, int h) const
{
    auto bg = (overall_threat == "CRITICAL") ? cv::Scalar(0,0,120) :
              (overall_threat == "WARNING")  ? cv::Scalar(0,60,120) :
                                               cv::Scalar(0,60,0);
    cv::rectangle(canvas, {x,y}, {x+w, y+h}, bg, -1);
    cv::line(canvas, {x,y}, {x+w,y}, {80,80,80}, 1);

    // Controls hint
    cv::putText(canvas,
        "[SPACE]=Pause  [S]=Step  [F]=Fast  [W]=Slow  [R]=Reset  [Q]=Quit",
        {x+5, y+14}, cv::FONT_HERSHEY_SIMPLEX, 0.38, {180,180,180}, 1);

    // Scene info
    std::string info = "Scene:" + scene +
                       "  Frame:" + std::to_string(frame_idx) +
                       "/" + std::to_string(total_frames) +
                       "  Tracks:" +
                       std::to_string(0) +
                       "  State:" + stateLabel(state) +
                       "  Threat: " + overall_threat;

    // Simplified — just show key info
    std::string right = "Scene:" + scene +
                        " | Frame " + std::to_string(frame_idx) +
                        "/" + std::to_string(total_frames) +
                        " | " + stateLabel(state) +
                        " | Overall: " + overall_threat;

    auto col = threatColor(overall_threat);
    cv::putText(canvas, right,
                {x+5, y+30}, cv::FONT_HERSHEY_SIMPLEX,
                0.45, col, 1);
}

cv::Scalar Visualizer::threatColor(const std::string& level) {
    if (level == "CRITICAL") return {0,0,255};
    if (level == "WARNING")  return {0,165,255};
    return {0,220,0};
}

cv::Scalar Visualizer::threatColorByLevel(ThreatLevel l) {
    switch (l) {
        case ThreatLevel::CRITICAL: return {0,0,255};
        case ThreatLevel::WARNING:  return {0,165,255};
        default:                    return {0,220,0};
    }
}

std::string Visualizer::stateLabel(PlaybackState s) {
    switch (s) {
        case PlaybackState::PAUSED:  return "PAUSED";
        case PlaybackState::FAST:    return "FAST 3x";
        case PlaybackState::SLOW:    return "SLOW 0.5x";
        case PlaybackState::STEPPING:return "STEP";
        default:                     return "PLAYING";
    }
}

} // namespace adas
