#include "Rail.h"
#include <algorithm>
#include <cmath>

Rail::Rail() {}
Rail::~Rail() {}

void Rail::Initialize(const std::vector<LevelCurvePoint>& points) {
    points_ = points;
    arcLengthSamples_.clear();
    totalLength_ = 0.0f;

    if (points_.size() < 2) return;

    // 弧長（Arc-Length）サンプリングテーブルを構築
    // 制御点の間隔が不均等でも、一定速度（等速）で走るようにする
    int segmentCount = static_cast<int>(points_.size()) - 1;
    const int samplesPerSegment = 40;
    int totalSamples = segmentCount * samplesPerSegment;

    arcLengthSamples_.reserve(totalSamples + 1);

    Vector3 prevPos = EvaluateRawPosition(0.0f);
    arcLengthSamples_.push_back({ 0.0f, 0.0f });

    float accumDist = 0.0f;
    for (int s = 1; s <= totalSamples; ++s) {
        float rawT = static_cast<float>(s) / static_cast<float>(totalSamples);
        Vector3 currPos = EvaluateRawPosition(rawT);
        float stepDist = (currPos - prevPos).Length();
        accumDist += stepDist;
        arcLengthSamples_.push_back({ rawT, accumDist });
        prevPos = currPos;
    }
    totalLength_ = accumDist;

    // 制御点のイベントからトンネル/狭窄区間を自動登録
    narrowZones_.clear();
    int narrowStart = -1;
    for (int i = 0; i < (int)points_.size(); ++i) {
        const std::string& ev = points_[i].event;
        bool isNarrow = (ev.find("tunnel") != std::string::npos || ev.find("narrow") != std::string::npos);
        if (isNarrow && narrowStart == -1) {
            narrowStart = i;
        } else if (!isNarrow && narrowStart != -1) {
            float sT = (float)narrowStart / segmentCount;
            float eT = (float)i / segmentCount;
            AddNarrowZone(sT, eT, 10.0f, -3.0f, 8.0f);
            narrowStart = -1;
        }
    }
    if (narrowStart != -1) {
        float sT = (float)narrowStart / segmentCount;
        AddNarrowZone(sT, 1.0f, 10.0f, -3.0f, 8.0f);
    }
}

float Rail::GetTFromDistance(float distance) const {
    if (arcLengthSamples_.empty()) return 0.0f;
    if (distance <= 0.0f) return 0.0f;
    if (distance >= totalLength_) return 1.0f;

    // 二分探索で distance が収まるサンプル区間を特定
    auto it = std::lower_bound(arcLengthSamples_.begin(), arcLengthSamples_.end(), distance,
        [](const ArcLengthSample& sample, float dist) {
            return sample.distance < dist;
        });

    if (it == arcLengthSamples_.begin()) return it->rawT;
    if (it == arcLengthSamples_.end()) return 1.0f;

    auto prev = it - 1;
    float segmentDist = it->distance - prev->distance;
    if (segmentDist <= 0.0001f) return prev->rawT;

    float ratio = (distance - prev->distance) / segmentDist;
    return prev->rawT + (it->rawT - prev->rawT) * ratio;
}

void Rail::GetSegment(float rawT, int& outIndex, float& outLocalT) const {
    if (points_.size() < 2) {
        outIndex = 0;
        outLocalT = 0.0f;
        return;
    }

    rawT = std::clamp(rawT, 0.0f, 1.0f);
    int segmentCount = static_cast<int>(points_.size()) - 1;
    if (segmentCount <= 0) {
        outIndex = 0;
        outLocalT = 0.0f;
        return;
    }

    float scaledT = rawT * segmentCount;
    outIndex = static_cast<int>(scaledT);
    
    if (outIndex >= segmentCount) {
        outIndex = segmentCount - 1;
        outLocalT = 1.0f;
    } else if (outIndex < 0) {
        outIndex = 0;
        outLocalT = 0.0f;
    } else {
        outLocalT = scaledT - static_cast<float>(outIndex);
    }

    outIndex = std::clamp(outIndex, 0, (std::max)(0, segmentCount - 1));
    outLocalT = std::clamp(outLocalT, 0.0f, 1.0f);
}

Vector3 Rail::EvaluateRawPosition(float rawT) const {
    if (points_.empty()) return Vector3();
    if (points_.size() == 1) return points_[0].position;

    int i = 0;
    float localT = 0.0f;
    GetSegment(rawT, i, localT);

    int maxIdx = static_cast<int>(points_.size()) - 1;
    i = std::clamp(i, 0, (std::max)(0, maxIdx - 1));

    Vector3 P0 = (i - 1 >= 0) ? points_[i - 1].position : points_[i].position;
    Vector3 P1 = points_[i].position;
    Vector3 P2 = (i + 1 <= maxIdx) ? points_[i + 1].position : points_[maxIdx].position;
    Vector3 P3 = (i + 2 <= maxIdx) ? points_[i + 2].position : P2;

    float t2 = localT * localT;
    float t3 = t2 * localT;

    Vector3 result = (P1 * 2.0f) + 
                     (P2 - P0) * localT + 
                     (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * t2 + 
                     (P1 * 3.0f - P0 - P2 * 3.0f + P3) * t3;
                     
    return result * 0.5f;
}

Vector3 Rail::EvaluateRawForward(float rawT) const {
    if (points_.empty()) return Vector3(0.0f, 0.0f, 1.0f);
    if (points_.size() == 1) return Vector3(0.0f, 0.0f, 1.0f);

    int i = 0;
    float localT = 0.0f;
    GetSegment(rawT, i, localT);

    int maxIdx = static_cast<int>(points_.size()) - 1;
    i = std::clamp(i, 0, (std::max)(0, maxIdx - 1));

    Vector3 P0 = (i - 1 >= 0) ? points_[i - 1].position : points_[i].position;
    Vector3 P1 = points_[i].position;
    Vector3 P2 = (i + 1 <= maxIdx) ? points_[i + 1].position : points_[maxIdx].position;
    Vector3 P3 = (i + 2 <= maxIdx) ? points_[i + 2].position : P2;

    float t2 = localT * localT;

    Vector3 derivative = (P2 - P0) + 
                         (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * (2.0f * localT) + 
                         (P1 * 3.0f - P0 - P2 * 3.0f + P3) * (3.0f * t2);
                         
    derivative = derivative * 0.5f;

    float len = derivative.Length();
    if (len > 0.0001f) {
        return derivative.Normalize();
    }
    return (P2 - P1).Normalize();
}

Vector3 Rail::GetPosition(float t) const {
    if (points_.empty()) return Vector3();
    if (arcLengthSamples_.empty()) return EvaluateRawPosition(t);

    float distance = std::clamp(t, 0.0f, 1.0f) * totalLength_;
    float rawT = GetTFromDistance(distance);
    return EvaluateRawPosition(rawT);
}

Vector3 Rail::GetForward(float t) const {
    if (points_.empty()) return Vector3(0.0f, 0.0f, 1.0f);
    if (arcLengthSamples_.empty()) return EvaluateRawForward(t);

    float distance = std::clamp(t, 0.0f, 1.0f) * totalLength_;
    float rawT = GetTFromDistance(distance);
    return EvaluateRawForward(rawT);
}

float Rail::GetTilt(float t) const {
    if (points_.empty()) return 0.0f;
    if (points_.size() == 1) return points_[0].tilt;

    float rawT = t;
    if (!arcLengthSamples_.empty()) {
        float distance = std::clamp(t, 0.0f, 1.0f) * totalLength_;
        rawT = GetTFromDistance(distance);
    }

    int i = 0;
    float localT = 0.0f;
    GetSegment(rawT, i, localT);

    // 単純な線形補間
    return points_[i].tilt * (1.0f - localT) + points_[i + 1].tilt * localT;
}

float Rail::GetSpeed(float t) const {
    if (useCustomSpeed_) {
        return customSpeed_;
    }

    if (points_.empty()) return 20.0f;
    if (points_.size() == 1) return points_[0].speed;

    int i = 0;
    float localT = 0.0f;
    GetSegment(t, i, localT);

    // 単純な線形補間
    return points_[i].speed * (1.0f - localT) + points_[i + 1].speed * localT;
}

std::string Rail::GetEvent(float t) const {
    if (points_.empty()) return "none";
    if (points_.size() == 1) return points_[0].event;

    int i = 0;
    float localT = 0.0f;
    GetSegment(t, i, localT);

    // イベントは補間できないため、現在の区間（始点）のイベントを返す
    return points_[i].event;
}

void Rail::AddNarrowZone(float startT, float endT, float limitX, float limitYMin, float limitYMax) {
    NarrowZone zone;
    zone.startT = startT;
    zone.endT = endT;
    zone.limitX = limitX;
    zone.limitYMin = limitYMin;
    zone.limitYMax = limitYMax;
    narrowZones_.push_back(zone);
}

void Rail::GetMoveLimits(float t, float& outLimitX, float& outLimitYMin, float& outLimitYMax) const {
    outLimitX = defaultLimitX_;
    outLimitYMin = defaultLimitYMin_;
    outLimitYMax = defaultLimitYMax_;

    // 前後0.03（進行度の3%）を手前からの導入・脱出フェード区間とする
    const float fadeDist = 0.03f;

    for (const auto& zone : narrowZones_) {
        float effectiveStart = zone.startT - fadeDist;
        float effectiveEnd = zone.endT + fadeDist;

        if (t >= effectiveStart && t <= effectiveEnd) {
            float weight = 1.0f;
            if (t < zone.startT) {
                // 入口手前フェードイン
                weight = (t - effectiveStart) / fadeDist;
            } else if (t > zone.endT) {
                // 出口後フェードアウト
                weight = (effectiveEnd - t) / fadeDist;
            }

            // スムーズステップ (3w^2 - 2w^3)
            weight = weight * weight * (3.0f - 2.0f * weight);

            outLimitX = (1.0f - weight) * defaultLimitX_ + weight * zone.limitX;
            outLimitYMin = (1.0f - weight) * defaultLimitYMin_ + weight * zone.limitYMin;
            outLimitYMax = (1.0f - weight) * defaultLimitYMax_ + weight * zone.limitYMax;
            return;
        }
    }
}

float Rail::GetClosestProgress(const Vector3& worldPos, int sampleCount) const {
    if (points_.empty()) return 0.0f;
    if (sampleCount <= 1) return 0.0f;

    float bestT = 0.0f;
    float minSqDist = (std::numeric_limits<float>::max)();

    // 1段階目: 粗いサンプリングで最近傍候補を探索
    for (int i = 0; i <= sampleCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(sampleCount);
        Vector3 p = GetPosition(t);
        Vector3 diff = worldPos - p;
        float sqDist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (sqDist < minSqDist) {
            minSqDist = sqDist;
            bestT = t;
        }
    }

    // 2段階目: 候補周辺を細かくサンプリングして精度向上
    float step = 1.0f / static_cast<float>(sampleCount);
    float fineStart = (std::max)(0.0f, bestT - step);
    float fineEnd = (std::min)(1.0f, bestT + step);
    const int fineSteps = 20;
    for (int j = 0; j <= fineSteps; ++j) {
        float t = fineStart + (fineEnd - fineStart) * (static_cast<float>(j) / fineSteps);
        Vector3 p = GetPosition(t);
        Vector3 diff = worldPos - p;
        float sqDist = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (sqDist < minSqDist) {
            minSqDist = sqDist;
            bestT = t;
        }
    }

    return std::clamp(bestT, 0.0f, 1.0f);
}
