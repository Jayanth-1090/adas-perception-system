#include "fusion/KalmanTracker.hpp"
#include "fusion/HungarianAssigner.hpp"
#include <spdlog/spdlog.h>
#include <cmath>

namespace adas {

// ── Constructor ────────────────────────────────────────────────
KalmanTracker::KalmanTracker() : next_id_(1) {}

// ── update ────────────────────────────────────────────────────
// This is the main entry point called every frame.
// Flow: predict all tracks → build cost matrix → Hungarian assign
//       → update matched tracks → init new tracks → prune dead ones
//       → return confirmed FusedObjects
std::vector<FusedObject> KalmanTracker::update(
    const std::vector<Detection>& detections, float dt)
{
    // ── 1. Predict all existing tracks forward by dt ───────────
    for (auto& t : tracks_)
        predict(t, dt);

    // ── 2. Build cost matrix ───────────────────────────────────
    // cost[i][j] = Euclidean distance between track i predicted
    //              position and detection j measured position
    const int n_tracks = static_cast<int>(tracks_.size());
    const int n_dets   = static_cast<int>(detections.size());

    std::vector<std::vector<float>> cost(
        n_tracks, std::vector<float>(n_dets, 0.f));

    for (int i = 0; i < n_tracks; ++i)
        for (int j = 0; j < n_dets; ++j)
            cost[i][j] = euclidean_dist(tracks_[i], detections[j]);

    // ── 3. Hungarian assignment ────────────────────────────────
    auto result = HungarianAssigner::assign(cost, n_dets, MAX_MATCH_DIST);

    // ── 4. Update matched tracks with their detection ──────────
    for (auto& [ti, di] : result.matched) {
        correct(tracks_[ti], detections[di]);
        tracks_[ti].hits++;
        tracks_[ti].misses = 0;
        tracks_[ti].age++;
        tracks_[ti].label      = detections[di].label;
        tracks_[ti].confidence = detections[di].confidence;
        if (tracks_[ti].hits >= MIN_HITS)
            tracks_[ti].confirmed = true;
    }

    // ── 5. Increment misses for unmatched tracks ───────────────
    for (int ti : result.unmatched_tracks) {
        tracks_[ti].misses++;
        tracks_[ti].age++;
    }

    // ── 6. Create new tracks for unmatched detections ─────────
    for (int di : result.unmatched_detections)
        tracks_.push_back(initTrack(detections[di]));

    // ── 7. Remove dead tracks ──────────────────────────────────
    pruneDeadTracks();

    // ── 8. Return confirmed tracks as FusedObjects ────────────
    std::vector<FusedObject> output;
    for (const auto& t : tracks_) {
        if (!t.confirmed) continue;
        FusedObject obj;
        obj.track_id   = t.track_id;
        obj.x          = t.x();
        obj.y          = t.y();
        obj.vx         = t.vx();
        obj.vy         = t.vy();
        obj.label      = t.label;
        obj.confidence = t.confidence;
        obj.contributing_sensors = { t.sensor };
        output.push_back(obj);
    }
    return output;
}

// ── predict ───────────────────────────────────────────────────
// Constant velocity model:
//   x  ← x  + vx * dt
//   y  ← y  + vy * dt
//   vx ← vx
//   vy ← vy
//
// Also propagates covariance: P = F*P*Fᵀ + Q
// F = state transition matrix, Q = process noise
void KalmanTracker::predict(Track& t, float dt) {
    // State transition matrix F
    Mat44 F = mat44_identity();
    F[0*4+2] = dt;   // x  += vx*dt
    F[1*4+3] = dt;   // y  += vy*dt

    // Predicted state: x = F * x
    Vec4 s = t.state;
    t.state[0] = s[0] + s[2]*dt;
    t.state[1] = s[1] + s[3]*dt;
    t.state[2] = s[2];
    t.state[3] = s[3];

    // Process noise Q — models uncertainty in our constant velocity assumption
    // Larger values = we trust the model less, measurements more
    Mat44 Q = {};
    float q  = 0.5f;           // position noise
    float qv = 1.0f;           // velocity noise (higher — velocity is less predictable)
    Q[0*4+0] = q;
    Q[1*4+1] = q;
    Q[2*4+2] = qv;
    Q[3*4+3] = qv;

    // P = F*P*Fᵀ + Q
    Mat44 Pmat;
    std::copy(t.P.begin(), t.P.end(), Pmat.begin());
    Mat44 FP   = mat44_mul(F, Pmat);
    Mat44 Ft   = mat44_transpose(F);
    Mat44 FPFt = mat44_mul(FP, Ft);
    Mat44 Pnew = mat44_add(FPFt, Q);
    std::copy(Pnew.begin(), Pnew.end(), t.P.begin());
}

// ── correct ───────────────────────────────────────────────────
// Kalman update step using measurement z = [x, y]
//
// Innovation:    y = z - H*x
// Innovation cov: S = H*P*Hᵀ + R
// Kalman gain:   K = P*Hᵀ * S⁻¹
// Updated state: x = x + K*y
// Updated cov:   P = (I - K*H)*P
void KalmanTracker::correct(Track& t, const Detection& d) {
    // Observation matrix H: extracts [x, y] from [x, y, vx, vy]
    // H = [1 0 0 0]
    //     [0 1 0 0]
    Mat24 H = mat24_from_H();

    // Measurement noise R — how much we trust the detection
    // Larger = less trust in measurements, smoother but slower response
    Mat22 R = {};
    R[0] = 2.0f;   // x measurement noise (m²)
    R[3] = 2.0f;   // y measurement noise (m²)

    Mat44 Pmat;
    std::copy(t.P.begin(), t.P.end(), Pmat.begin());

    // PHᵀ (4x2)
    Mat42 PHt = mat44_mul_mat42(Pmat, []() -> Mat42 {
        // Hᵀ = [1 0]  [0 1]  [0 0]  [0 0]
        Mat42 Ht = {};
        Ht[0*2+0] = 1.f;
        Ht[1*2+1] = 1.f;
        return Ht;
    }());

    // S = H*P*Hᵀ + R (2x2)
    Mat22 HPHt = mat24_mul_mat42(H, PHt);
    Mat22 S    = { HPHt[0]+R[0], HPHt[1]+R[1],
                   HPHt[2]+R[2], HPHt[3]+R[3] };

    // K = PHᵀ * S⁻¹ (4x2)
    Mat22 Sinv = mat22_inverse(S);
    Mat42 K    = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 2; ++k)
                K[i*2+j] += PHt[i*2+k] * Sinv[k*2+j];

    // Innovation y = z - H*x
    Vec2 z    = { d.x, d.y };
    Vec2 Hx   = { t.state[0], t.state[1] };
    Vec2 innov = { z[0]-Hx[0], z[1]-Hx[1] };

    // x = x + K*y
    Vec4 Ky = mat42_mul_vec2(K, innov);
    for (int i = 0; i < 4; ++i)
        t.state[i] += Ky[i];

    // P = (I - K*H) * P
    // K*H (4x4)
    Mat44 KH = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 2; ++k)
                KH[i*4+j] += K[i*2+k] * H[k*4+j];

    Mat44 IminusKH = mat44_identity();
    for (int i = 0; i < 16; ++i)
        IminusKH[i] -= KH[i];

    Mat44 Pnew = mat44_mul(IminusKH, Pmat);
    std::copy(Pnew.begin(), Pnew.end(), t.P.begin());
}

// ── initTrack ─────────────────────────────────────────────────
Track KalmanTracker::initTrack(const Detection& d) {
    Track t;
    t.track_id  = next_id_++;
    t.label     = d.label;
    t.confidence= d.confidence;
    t.sensor    = d.sensor;
    t.age       = 1;
    t.hits      = 1;
    t.misses    = 0;
    t.confirmed = false;

    // Initialize state from first detection
    // Velocity starts at 0 — will be estimated after a few frames
    t.state = { d.x, d.y, 0.f, 0.f };

    // Initial covariance — high uncertainty at birth, especially in velocity
    t.P = {};
    t.P[0*4+0] = 4.0f;    // x uncertainty
    t.P[1*4+1] = 4.0f;    // y uncertainty
    t.P[2*4+2] = 16.0f;   // vx uncertainty (high — we don't know velocity yet)
    t.P[3*4+3] = 16.0f;   // vy uncertainty

    spdlog::debug("Track #{} born: {} at ({:.1f}, {:.1f})",
                  t.track_id, t.label, d.x, d.y);
    return t;
}

// ── pruneDeadTracks ───────────────────────────────────────────
void KalmanTracker::pruneDeadTracks() {
    tracks_.erase(
        std::remove_if(tracks_.begin(), tracks_.end(),
            [](const Track& t) { return t.misses > MAX_MISSES; }),
        tracks_.end()
    );
}

// ── euclidean_dist ────────────────────────────────────────────
float KalmanTracker::euclidean_dist(const Track& t, const Detection& d) {
    float dx = t.x() - d.x;
    float dy = t.y() - d.y;
    return std::sqrt(dx*dx + dy*dy);
}

// ════════════════════════════════════════════════════════════════
// Matrix math — all manual, no external library
// ════════════════════════════════════════════════════════════════

KalmanTracker::Mat44 KalmanTracker::mat44_identity() {
    Mat44 I = {};
    I[0*4+0] = I[1*4+1] = I[2*4+2] = I[3*4+3] = 1.f;
    return I;
}

KalmanTracker::Mat44 KalmanTracker::mat44_add(const Mat44& A, const Mat44& B) {
    Mat44 C;
    for (int i = 0; i < 16; ++i) C[i] = A[i] + B[i];
    return C;
}

KalmanTracker::Mat44 KalmanTracker::mat44_mul(const Mat44& A, const Mat44& B) {
    Mat44 C = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                C[i*4+j] += A[i*4+k] * B[k*4+j];
    return C;
}

KalmanTracker::Mat44 KalmanTracker::mat44_transpose(const Mat44& A) {
    Mat44 T;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            T[i*4+j] = A[j*4+i];
    return T;
}

KalmanTracker::Mat22 KalmanTracker::mat22_inverse(const Mat22& A) {
    // [a b]⁻¹ = 1/(ad-bc) * [ d -b]
    // [c d]                  [-c  a]
    float det = A[0]*A[3] - A[1]*A[2];
    if (std::abs(det) < 1e-9f) det = 1e-9f;  // guard against singular matrix
    float inv = 1.f / det;
    return { A[3]*inv, -A[1]*inv, -A[2]*inv, A[0]*inv };
}

KalmanTracker::Mat24 KalmanTracker::mat24_from_H() {
    // H = [1 0 0 0]
    //     [0 1 0 0]
    Mat24 H = {};
    H[0*4+0] = 1.f;
    H[1*4+1] = 1.f;
    return H;
}

KalmanTracker::Mat42 KalmanTracker::mat44_mul_mat42(const Mat44& A, const Mat42& B) {
    Mat42 C = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 4; ++k)
                C[i*2+j] += A[i*4+k] * B[k*2+j];
    return C;
}

KalmanTracker::Mat22 KalmanTracker::mat24_mul_mat42(const Mat24& H, const Mat42& PHt) {
    Mat22 C = {};
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 4; ++k)
                C[i*2+j] += H[i*4+k] * PHt[k*2+j];
    return C;
}

KalmanTracker::Vec4 KalmanTracker::vec4_add(const Vec4& a, const Vec4& b) {
    return { a[0]+b[0], a[1]+b[1], a[2]+b[2], a[3]+b[3] };
}

KalmanTracker::Vec4 KalmanTracker::vec4_sub(const Vec4& a, const Vec4& b) {
    return { a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3] };
}

KalmanTracker::Vec4 KalmanTracker::mat42_mul_vec2(const Mat42& K, const Vec2& v) {
    Vec4 r = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 2; ++j)
            r[i] += K[i*2+j] * v[j];
    return r;
}

} // namespace adas
