#pragma once

#include "common/Types.hpp"
#include <vector>
#include <array>
#include <cstdint>

namespace adas {

// ── State vector indices ───────────────────────────────────────
// state = [x, y, vx, vy]
//          0  1   2   3
constexpr int STATE_DIM = 4;  // x, y, vx, vy
constexpr int MEAS_DIM  = 2;  // x, y  (we only measure position, not velocity)

// ── Single tracked object ──────────────────────────────────────
struct Track {
    uint32_t    track_id;
    std::string label;
    float       confidence;
    SensorType  sensor;

    // Kalman state: [x, y, vx, vy]
    std::array<float, STATE_DIM> state;

    // Covariance matrix P (4x4) — stored row-major as flat array
    // Represents uncertainty in our state estimate
    std::array<float, STATE_DIM * STATE_DIM> P;

    int   age;           // how many frames this track has existed
    int   hits;          // how many times it was matched to a detection
    int   misses;        // consecutive frames with no matching detection
    bool  confirmed;     // true once hits >= MIN_HITS

    // Convenience accessors
    float x()  const { return state[0]; }
    float y()  const { return state[1]; }
    float vx() const { return state[2]; }
    float vy() const { return state[3]; }
};

// ── Kalman Tracker ─────────────────────────────────────────────
class KalmanTracker {
public:
    // Tuning parameters
    static constexpr int   MIN_HITS       = 2;    // frames before track is confirmed
    static constexpr int   MAX_MISSES     = 3;    // frames before track is deleted
    static constexpr float MAX_MATCH_DIST = 8.0f; // meters — max distance for valid match

    KalmanTracker();

    // Main entry point — call once per frame with new detections
    // Returns confirmed FusedObjects ready for threat classification
    std::vector<FusedObject> update(const std::vector<Detection>& detections, float dt);

    // All active tracks (including unconfirmed) — for debugging
    const std::vector<Track>& tracks() const { return tracks_; }

private:
    std::vector<Track> tracks_;
    uint32_t           next_id_;

    // ── Kalman steps ───────────────────────────────────────────
    void  predict(Track& t, float dt);
    void  correct(Track& t, const Detection& d);

    // ── Track lifecycle ────────────────────────────────────────
    Track initTrack(const Detection& d);
    void  pruneDeadTracks();

    // ── Matrix helpers (manual 4x4 and 2x4 math) ──────────────
    // We implement these manually — no Eigen dependency
    // This is intentional: shows you understand the math, not just a library
    using Mat44 = std::array<float, 16>;  // 4x4 row-major
    using Mat22 = std::array<float,  4>;  // 2x2 row-major
    using Mat42 = std::array<float,  8>;  // 4x2 row-major
    using Mat24 = std::array<float,  8>;  // 2x4 row-major
    using Vec4  = std::array<float,  4>;
    using Vec2  = std::array<float,  2>;

    static Mat44 mat44_identity();
    static Mat44 mat44_add(const Mat44& A, const Mat44& B);
    static Mat44 mat44_mul(const Mat44& A, const Mat44& B);
    static Mat44 mat44_transpose(const Mat44& A);
    static Mat22 mat22_inverse(const Mat22& A);
    static Mat24 mat24_from_H();           // observation matrix H
    static Mat42 mat44_mul_mat42(const Mat44& A, const Mat42& B);
    static Mat22 mat24_mul_mat42(const Mat24& H, const Mat42& PHt);
    static Vec4  vec4_add(const Vec4& a, const Vec4& b);
    static Vec4  vec4_sub(const Vec4& a, const Vec4& b);
    static Vec4  mat42_mul_vec2(const Mat42& K, const Vec2& v);
    static float euclidean_dist(const Track& t, const Detection& d);
};

} // namespace adas
