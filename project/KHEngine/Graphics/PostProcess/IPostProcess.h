#pragma once
#include <string>

// 今後増えるエフェクト用パラメータをすべて詰め込む定数バッファ構造体
struct PostProcessData {
	int enableGrayscale;
	int enableSepia;
	float sepiaStrength;
	int enableVignette;

	float vignetteIntensity;
	float vignettePower;
	int enableSmoothing;
	float smoothingKernelSize;

	int enableGaussian;
	float gaussianSigma;
	int enableOutline;
	float outlineThreshold;

	int enableRadialBlur;
	float radialBlurCenterX;
	float radialBlurCenterY;
	float radialBlurIntensity;

	int enableDissolve;
	float dissolveThreshold;
	float dissolveEdgeWidth;
	float padding1;

	float dissolveEdgeColor[3];
	int enableRandom;

	float randomTime;
	float glitchStrength;
	float noiseStrength;
	float damagePulse;      // 被弾瞬間パルス (1.0 -> 0.0)

	float lowHpIntensity;   // HP減少・危機時の外周赤脈動強度 (0.0 ~ 1.0)
	float padding2[3];
};

class IPostProcess {
public:
  virtual ~IPostProcess() = default;

  virtual void Initialize() = 0;
  virtual void DrawImGui() = 0;

  bool isActive_ = false;
  std::string name_ = "None";
};
