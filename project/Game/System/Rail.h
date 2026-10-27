#pragma once
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Math/Vector3.h"
#include <vector>

class Rail {
public:
    Rail();
    ~Rail();

    /// <summary>
    /// レールの制御点を初期化する
    /// </summary>
    void Initialize(const std::vector<LevelCurvePoint>& points);

    /// <summary>
    /// 進行度(t) における3D座標を取得する
    /// </summary>
    /// <param name="t">進行度 (0.0f = 始点, 1.0f = 終点)</param>
    Vector3 GetPosition(float t) const;

    /// <summary>
    /// 進行度(t) における進行方向(接線ベクトル)を取得する
    /// </summary>
    Vector3 GetForward(float t) const;

    /// <summary>
    /// 進行度(t) におけるロール角（Tilt）を取得する
    /// </summary>
    float GetTilt(float t) const;

    /// <summary>
    /// 進行度(t) における速度（Speed）を取得する
    /// </summary>
    float GetSpeed(float t) const;

    /// <summary>
    /// ゲーム側で一括設定する基準速度を設定する
    /// </summary>
    void SetCustomSpeed(float speed) { customSpeed_ = speed; }
    float GetCustomSpeed() const { return customSpeed_; }
    void SetUseCustomSpeed(bool use) { useCustomSpeed_ = use; }
    bool IsUsingCustomSpeed() const { return useCustomSpeed_; }

    /// <summary>
    /// 進行度(t) におけるイベント名を取得する
    /// </summary>
    std::string GetEvent(float t) const;

    /// <summary>
    /// レールの総長を取得する（近似値）
    /// </summary>
    float GetTotalLength() const { return totalLength_; }

    /// <summary>
    /// 制御点がセットされているか
    /// </summary>
    bool IsValid() const { return points_.size() >= 2; }

    /// <summary>
    /// 指定ワールド座標に最も近いレール進行度(0.0f〜1.0f)を取得する
    /// </summary>
    float GetClosestProgress(const Vector3& worldPos, int sampleCount = 100) const;

    // 狭窄区間（トンネル・建物隙間）の設定
    struct NarrowZone {
        float startT;
        float endT;
        float limitX = 10.0f;
        float limitYMin = -3.0f;
        float limitYMax = 8.0f;
    };

    void AddNarrowZone(float startT, float endT, float limitX = 10.0f, float limitYMin = -3.0f, float limitYMax = 8.0f);
    void GetMoveLimits(float t, float& outLimitX, float& outLimitYMin, float& outLimitYMax) const;
    void SetDefaultMoveLimits(float limitX, float limitYMin, float limitYMax) {
        defaultLimitX_ = limitX;
        defaultLimitYMin_ = limitYMin;
        defaultLimitYMax_ = limitYMax;
    }
    const std::vector<NarrowZone>& GetNarrowZones() const { return narrowZones_; }
    void ClearNarrowZones() { narrowZones_.clear(); }

    // 弧長サンプリングによる距離ベースパラメータ化
    struct ArcLengthSample {
        float rawT = 0.0f;
        float distance = 0.0f;
    };

    float GetTFromDistance(float distance) const;

private:
    std::vector<LevelCurvePoint> points_;
    std::vector<NarrowZone> narrowZones_;
    std::vector<ArcLengthSample> arcLengthSamples_;
    float totalLength_ = 0.0f;
    float customSpeed_ = 50.0f;
    bool useCustomSpeed_ = true;

    // デフォルト移動制限（幅±35m, 下限-4m, 上限+20m）
    float defaultLimitX_ = 35.0f;
    float defaultLimitYMin_ = -4.0f;
    float defaultLimitYMax_ = 20.0f;

    // 区間と区間内進行度を計算する補助関数
    void GetSegment(float rawT, int& outIndex, float& outLocalT) const;
    Vector3 EvaluateRawPosition(float rawT) const;
    Vector3 EvaluateRawForward(float rawT) const;
};
