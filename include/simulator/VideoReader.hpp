#pragma once
#include <string>
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>

namespace adas {

class VideoReader {
public:
    explicit VideoReader(const std::string& video_path);

    // Seek to nearest frame for given timestamp (ms)
    // Returns true if frame retrieved successfully
    bool getFrameAt(int64_t timestamp_ms, cv::Mat& frame);

    // Get next sequential frame
    bool nextFrame(cv::Mat& frame);

    double fps()         const { return fps_; }
    int    frameCount()  const { return frame_count_; }
    int    width()       const { return width_; }
    int    height()      const { return height_; }
    bool   isOpen()      const { return cap_.isOpened(); }
    void   reset();

private:
    cv::VideoCapture cap_;
    double           fps_;
    int              frame_count_;
    int              width_;
    int              height_;
};

} // namespace adas
