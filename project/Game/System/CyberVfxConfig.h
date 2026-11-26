#pragma once
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"

/// <summary>
/// サイバーVFX系（ボム・武器・スラスター）の数値パラメータと共通カラーパレットを
/// JSONファイルから一括管理・リロード・保存する設定マネージャ
/// </summary>
class CyberVfxConfig {
public:
	/// <summary>
	/// 共通3段階カラーパレット（resources/json/effects/cyber_palette.json）
	/// </summary>
	struct PaletteConfig {
		Vector4 pureWhite      = { 1.30f, 1.40f, 1.60f, 1.00f };
		Vector4 cyberBlue      = { 0.08f, 0.48f, 1.65f, 0.98f };
		Vector4 neonMagenta    = { 1.45f, 0.00f, 0.88f, 0.88f };
		Vector4 neonMagentaEnd = { 1.30f, 0.00f, 0.80f, 0.00f };

		float whiteEnd   = 0.08f; // 純白を維持する割合 (0.0 ~ whiteEnd)
		float blueEnd    = 0.45f; // シアンブルーへ遷移完了する割合
		float magentaEnd = 0.80f; // ネオンマゼンタへ遷移完了する割合（以降は透明へ減衰）
	};

	/// <summary>
	/// 武器＆ボムVFX設定（resources/json/effects/weapon_vfx.json）
	/// </summary>
	struct WeaponVfxConfig {
		// 通常弾・ファンネル マズルフラッシュ
		struct MuzzleFlash {
			float flashWidth = 2.6f;
			float flashHeight = 0.75f;
			float flashVHeight = 2.2f;
			float flashLife = 0.11f;
			Vector4 flashStartColor = { 1.80f, 2.00f, 2.20f, 1.00f };
			Vector4 flashEndColor   = { 0.10f, 0.55f, 1.65f, 0.00f };

			float ringStartScale = 0.45f;
			float ringEndScale   = 1.65f;
			float ringLife       = 0.16f;
			float ringRotSpeed   = 7.0f;
			Vector4 ringStartColor = { 0.08f, 0.52f, 1.65f, 0.95f };
			Vector4 ringEndColor   = { 1.45f, 0.02f, 0.90f, 0.00f };

			int voxelCount = 2;
			float voxelSize = 0.18f;
			float voxelLife = 0.22f;
			float voxelSpeedZ = 6.0f;
		} muzzle;

		// 拡散弾マズルVFX
		struct SpreadMuzzle {
			float coreScaleX = 2.8f;
			float coreScaleY = 1.8f;
			float coreLife   = 0.10f;
			float muzzleFlashScale = 1.0f;

			float ringStartScale = 0.65f;
			float ringEndScale   = 2.40f;
			float ringLife       = 0.18f;
			float ringTiltX      = 0.35f;
			float ringRotSpeed   = -6.0f;
			Vector4 ringStartColor = { 0.08f, 0.52f, 1.70f, 0.90f };
			Vector4 ringEndColor   = { 1.50f, 0.02f, 0.90f, 0.00f };

			int glitchCount = 2;
			float glitchLength = 1.8f;
			float glitchEndLength = 3.2f;
			float glitchThickness = 0.09f;
			float glitchLife = 0.13f;

			int voxelCount = 3;
			float voxelSize = 0.18f;
			float voxelLife = 0.22f;
			float voxelSpeed = 12.0f;
		} spread;

		// ボム爆発VFX
		struct BombExplosion {
			float coreStartScale = 12.0f;
			float coreLife       = 0.16f;
			Vector4 coreStartColor = { 2.20f, 2.30f, 2.50f, 1.00f };
			Vector4 coreEndColor   = { 0.10f, 0.55f, 1.65f, 0.00f };

			float flareHLength    = 18.0f;
			float flareVLength    = 15.0f;
			float flareThickness  = 2.8f;
			float flareLife       = 0.16f;
			Vector4 flareStartColor = { 2.00f, 2.20f, 2.40f, 1.00f };

			float ringStartBase = 1.6f;
			float ringStartStep = 0.6f;
			float ringEndBase   = 8.5f;
			float ringEndStep   = 2.2f;
			float ringLifeBase  = 0.45f;
			float ringLifeStep  = 0.04f;
			Vector4 ringStartColor = { 0.08f, 0.52f, 1.75f, 1.00f };
			Vector4 ringEndColor   = { 1.55f, 0.02f, 0.95f, 0.00f };

			int burstVoxelCount = 96;
			float burstVoxelSpeedMin   = 16.0f;
			float burstVoxelSpeedRange = 46.0f;
			float burstVoxelSizeMin    = 0.45f;
			float burstVoxelSizeRange  = 0.75f;
			float burstVoxelLifeBase   = 0.45f;

			int glitchCount = 12;
		} bombExplosion;

		// ファンネル軌道トレイル（サイバーライン）
		struct FunnelTrail {
			float trailLife    = 0.38f;
			float coreWidth    = 0.18f;
			float glowWidth    = 0.42f;
			float magentaWidth = 0.72f;
			Vector4 coreColor    = { 1.20f, 1.25f, 1.35f, 0.95f };
			Vector4 glowColor    = { 0.14f, 0.78f, 1.65f, 0.85f };
			Vector4 magentaColor = { 1.50f, 0.05f, 0.95f, 0.65f };
		} funnelTrail;
	};

	/// <summary>
	/// プレイヤースラスターVFX設定（resources/json/effects/thruster_vfx.json）
	/// </summary>
	struct ThrusterVfxConfig {
		Vector4 normalCyanGlow    = { 0.14f, 0.72f, 1.50f, 0.48f };
		Vector4 boostMagentaGlow  = { 1.38f, 0.10f, 0.92f, 0.54f };
		Vector4 whiteCoreColor    = { 1.00f, 1.00f, 1.00f, 0.56f };
		Vector4 trailMagentaColor = { 1.35f, 0.06f, 0.90f, 0.24f };

		float normalConeLength = 1.25f;
		float boostConeLength  = 2.25f;
		float coneRadius       = 0.22f;

		float normalTrailLife = 0.06f;
		float boostTrailLife  = 0.09f;
		float rollTrailLife   = 0.13f;

		float coreTrailWidth    = 0.036f;
		float glowTrailWidth    = 0.090f;
		float magentaTrailWidth = 0.155f;

		float normalSparkRate = 70.0f;
		float boostSparkRate  = 130.0f;
		float sparkMinScale   = 0.042f;
		float sparkMaxScale   = 0.072f;
		float sparkMinLife    = 0.20f;
		float sparkMaxLife    = 0.34f;
		float sparkAlpha      = 0.72f;

		int boostRingCount       = 3;
		float boostRingStartBase = 0.45f;
		float boostRingEndBase   = 2.00f;
		float boostRingLifeBase  = 0.45f;
		Vector4 boostRingFirstColor = { 0.35f, 0.92f, 1.55f, 0.60f };
		Vector4 boostRingOtherColor = { 1.40f, 0.08f, 0.92f, 0.52f };
		float boostFlashScaleRatio  = 0.72f;
		float boostFlashAlpha       = 0.75f;
	};

public:
	static CyberVfxConfig* GetInstance();

	/// <summary>
	/// 初回のみJSONを自動ロード（ファイルが無ければデフォルトJSONを生成）
	/// </summary>
	void EnsureLoaded();

	/// <summary>
	/// 3つのJSONファイルすべてを再読み込みする
	/// </summary>
	bool LoadAll();

	/// <summary>
	/// 現在の設定を3つのJSONファイルへ書き出す
	/// </summary>
	bool SaveAll() const;

	/// <summary>
	/// デフォルト値へリセットする
	/// </summary>
	void ResetToDefaults();

	/// <summary>
	/// 共通の3段階カラーカーブ（純白 → シアンブルー → ネオンマゼンタ）を評価する
	/// </summary>
	Vector4 EvaluateThreeStageColor(float ratio, float alphaScale = 1.0f) const;

	/// <summary>
	/// ImGuiによるリアルタイム調整・JSON再読込/保存ウィンドウ描画
	/// </summary>
	void DrawImGui(bool* pOpen = nullptr);

	const PaletteConfig& Palette() const { return palette_; }

	const WeaponVfxConfig& Weapon() const { return weapon_; }

	const ThrusterVfxConfig& Thruster() const { return thruster_; }

private:
	CyberVfxConfig() = default;

	PaletteConfig palette_{};
	WeaponVfxConfig weapon_{};
	ThrusterVfxConfig thruster_{};
	bool isLoaded_ = false;
};
