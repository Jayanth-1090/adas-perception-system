#include "simulator/VideoReader.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace adas {

VideoReader::VideoReader(const std::string& video_path) {
    cap_.open(video_path);
    if (!cap_.isOpened())
        throw std::runtime_error("VideoReader: cannot open: " + video_path);

    fps_         = cap_.get(cv::CAP_PROP_FPS);
    frame_count_ = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_COUNT));
    width_       = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    height_      = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));

    spdlog::info("VideoReader: opened {} | {}x{} @ {:.1f}fps | {} frames",
                 video_path, width_, height_, fps_, frame_count_);
}

bool VideoReader::getFrameAt(int64_t timestamp_ms, cv::Mat& frame) {
    // Convert timestamp to frame index
    double frame_idx = (timestamp_ms / 1000.0) * fps_;
    cap_.set(cv::CAP_PROP_POS_FRAMES, frame_idx);
    return cap_.read(frame);
}

bool VideoReader::nextFrame(cv::Mat& frame) {
    return cap_.read(frame);
}

void VideoReader::reset() {
    cap_.set(cv::CAP_PROP_POS_FRAMES, 0);
}

} // namespace adas
