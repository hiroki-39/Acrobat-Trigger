#define NOMINMAX
#include "CyberVfxConfig.h"
#include "externals/nlohmann/json.hpp"
#ifdef ENABLE_EDITOR
#include "externals/imgui/imgui.h"
#endif
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace {
	constexpr const char* kDirPath         = "resources/json/effects";
	constexpr const char* kPalettePath     = "resources/json/effects/cyber_palette.json";
	constexpr const char* kWeaponVfxPath   = "resources/json/effects/weapon_vfx.json";
	constexpr const char* kThrusterVfxPath = "resources/json/effects/thruster_vfx.json";

	nlohmann::json Vec4ToJson(const Vector4& v) {
		return nlohmann::json::array({ v.x, v.y, v.z, v.w });
	}

	void JsonToVec4(const nlohmann::json& j, const char* key, Vector4& out) {
		if (j.contains(key) && j[key].is_array() && j[key].size() == 4) {
			out.x = j[key][0].get<float>();
			out.y = j[key][1].get<float>();
			out.z = j[key][2].get<float>();
			out.w = j[key][3].get<float>();
		}
	}

	template <typename T>
	void JsonToVal(const nlohmann::json& j, const char* key, T& out) {
		if (j.contains(key)) {
			out = j[key].get<T>();
		}
	}

	Vector4 LerpVec4(const Vector4& a, const Vector4& b, float t) {
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}
}

CyberVfxConfig* CyberVfxConfig::GetInstance() {
	static CyberVfxConfig instance;
	return &instance;
}

void CyberVfxConfig::EnsureLoaded() {
	if (isLoaded_) return;
	isLoaded_ = true;
	if (!LoadAll()) {
		SaveAll();
	}
}

void CyberVfxConfig::ResetToDefaults() {
	palette_  = PaletteConfig{};
	weapon_   = WeaponVfxConfig{};
	thruster_ = ThrusterVfxConfig{};
}

bool CyberVfxConfig::LoadAll() {
	bool allLoaded = true;

	// 1. 共通カラーパレット
	{
		std::ifstream ifs(kPalettePath);
		if (ifs.is_open()) {
			nlohmann::json j;
			ifs >> j;
			JsonToVec4(j, "pureWhite", palette_.pureWhite);
			JsonToVec4(j, "cyberBlue", palette_.cyberBlue);
			JsonToVec4(j, "neonMagenta", palette_.neonMagenta);
			JsonToVec4(j, "neonMagentaEnd", palette_.neonMagentaEnd);
			JsonToVal(j, "whiteEnd", palette_.whiteEnd);
			JsonToVal(j, "blueEnd", palette_.blueEnd);
			JsonToVal(j, "magentaEnd", palette_.magentaEnd);
		} else {
			allLoaded = false;
		}
	}

	// 2. 武器＆ボムVFX
	{
		std::ifstream ifs(kWeaponVfxPath);
		if (ifs.is_open()) {
			nlohmann::json root;
			ifs >> root;

			if (root.contains("muzzle")) {
				const auto& m = root["muzzle"];
				JsonToVal(m, "flashWidth", weapon_.muzzle.flashWidth);
				JsonToVal(m, "flashHeight", weapon_.muzzle.flashHeight);
				JsonToVal(m, "flashVHeight", weapon_.muzzle.flashVHeight);
				JsonToVal(m, "flashLife", weapon_.muzzle.flashLife);
				JsonToVec4(m, "flashStartColor", weapon_.muzzle.flashStartColor);
				JsonToVec4(m, "flashEndColor", weapon_.muzzle.flashEndColor);
				JsonToVal(m, "ringStartScale", weapon_.muzzle.ringStartScale);
				JsonToVal(m, "ringEndScale", weapon_.muzzle.ringEndScale);
				JsonToVal(m, "ringLife", weapon_.muzzle.ringLife);
				JsonToVal(m, "ringRotSpeed", weapon_.muzzle.ringRotSpeed);
				JsonToVec4(m, "ringStartColor", weapon_.muzzle.ringStartColor);
				JsonToVec4(m, "ringEndColor", weapon_.muzzle.ringEndColor);
				JsonToVal(m, "voxelCount", weapon_.muzzle.voxelCount);
				JsonToVal(m, "voxelSize", weapon_.muzzle.voxelSize);
				JsonToVal(m, "voxelLife", weapon_.muzzle.voxelLife);
				JsonToVal(m, "voxelSpeedZ", weapon_.muzzle.voxelSpeedZ);
			}

			if (root.contains("spread")) {
				const auto& s = root["spread"];
				JsonToVal(s, "coreScaleX", weapon_.spread.coreScaleX);
				JsonToVal(s, "coreScaleY", weapon_.spread.coreScaleY);
				JsonToVal(s, "coreLife", weapon_.spread.coreLife);
				JsonToVal(s, "muzzleFlashScale", weapon_.spread.muzzleFlashScale);
				JsonToVal(s, "ringStartScale", weapon_.spread.ringStartScale);
				JsonToVal(s, "ringEndScale", weapon_.spread.ringEndScale);
				JsonToVal(s, "ringLife", weapon_.spread.ringLife);
				JsonToVal(s, "ringTiltX", weapon_.spread.ringTiltX);
				JsonToVal(s, "ringRotSpeed", weapon_.spread.ringRotSpeed);
				JsonToVec4(s, "ringStartColor", weapon_.spread.ringStartColor);
				JsonToVec4(s, "ringEndColor", weapon_.spread.ringEndColor);
				JsonToVal(s, "glitchCount", weapon_.spread.glitchCount);
				JsonToVal(s, "glitchLength", weapon_.spread.glitchLength);
				JsonToVal(s, "glitchEndLength", weapon_.spread.glitchEndLength);
				JsonToVal(s, "glitchThickness", weapon_.spread.glitchThickness);
				JsonToVal(s, "glitchLife", weapon_.spread.glitchLife);
				JsonToVal(s, "voxelCount", weapon_.spread.voxelCount);
				JsonToVal(s, "voxelSize", weapon_.spread.voxelSize);
				JsonToVal(s, "voxelLife", weapon_.spread.voxelLife);
				JsonToVal(s, "voxelSpeed", weapon_.spread.voxelSpeed);
			}

			if (root.contains("bombExplosion")) {
				const auto& b = root["bombExplosion"];
				JsonToVal(b, "coreStartScale", weapon_.bombExplosion.coreStartScale);
				JsonToVal(b, "coreLife", weapon_.bombExplosion.coreLife);
				JsonToVec4(b, "coreStartColor", weapon_.bombExplosion.coreStartColor);
				JsonToVec4(b, "coreEndColor", weapon_.bombExplosion.coreEndColor);
				JsonToVal(b, "flareHLength", weapon_.bombExplosion.flareHLength);
				JsonToVal(b, "flareVLength", weapon_.bombExplosion.flareVLength);
				JsonToVal(b, "flareThickness", weapon_.bombExplosion.flareThickness);
				JsonToVal(b, "flareLife", weapon_.bombExplosion.flareLife);
				JsonToVec4(b, "flareStartColor", weapon_.bombExplosion.flareStartColor);
				JsonToVal(b, "ringStartBase", weapon_.bombExplosion.ringStartBase);
				JsonToVal(b, "ringStartStep", weapon_.bombExplosion.ringStartStep);
				JsonToVal(b, "ringEndBase", weapon_.bombExplosion.ringEndBase);
				JsonToVal(b, "ringEndStep", weapon_.bombExplosion.ringEndStep);
				JsonToVal(b, "ringLifeBase", weapon_.bombExplosion.ringLifeBase);
				JsonToVal(b, "ringLifeStep", weapon_.bombExplosion.ringLifeStep);
				JsonToVec4(b, "ringStartColor", weapon_.bombExplosion.ringStartColor);
				JsonToVec4(b, "ringEndColor", weapon_.bombExplosion.ringEndColor);
				JsonToVal(b, "burstVoxelCount", weapon_.bombExplosion.burstVoxelCount);
				JsonToVal(b, "burstVoxelSpeedMin", weapon_.bombExplosion.burstVoxelSpeedMin);
				JsonToVal(b, "burstVoxelSpeedRange", weapon_.bombExplosion.burstVoxelSpeedRange);
				JsonToVal(b, "burstVoxelSizeMin", weapon_.bombExplosion.burstVoxelSizeMin);
				JsonToVal(b, "burstVoxelSizeRange", weapon_.bombExplosion.burstVoxelSizeRange);
				JsonToVal(b, "burstVoxelLifeBase", weapon_.bombExplosion.burstVoxelLifeBase);
				JsonToVal(b, "glitchCount", weapon_.bombExplosion.glitchCount);
			}

			if (root.contains("funnelTrail")) {
				const auto& ft = root["funnelTrail"];
				JsonToVal(ft, "trailLife", weapon_.funnelTrail.trailLife);
				JsonToVal(ft, "coreWidth", weapon_.funnelTrail.coreWidth);
				JsonToVal(ft, "glowWidth", weapon_.funnelTrail.glowWidth);
				JsonToVal(ft, "magentaWidth", weapon_.funnelTrail.magentaWidth);
				JsonToVec4(ft, "coreColor", weapon_.funnelTrail.coreColor);
				JsonToVec4(ft, "glowColor", weapon_.funnelTrail.glowColor);
				JsonToVec4(ft, "magentaColor", weapon_.funnelTrail.magentaColor);
			}
		} else {
			allLoaded = false;
		}
	}

	// 3. スラスターVFX
	{
		std::ifstream ifs(kThrusterVfxPath);
		if (ifs.is_open()) {
			nlohmann::json t;
			ifs >> t;
			JsonToVec4(t, "normalCyanGlow", thruster_.normalCyanGlow);
			JsonToVec4(t, "boostMagentaGlow", thruster_.boostMagentaGlow);
			JsonToVec4(t, "whiteCoreColor", thruster_.whiteCoreColor);
			JsonToVec4(t, "trailMagentaColor", thruster_.trailMagentaColor);
			JsonToVal(t, "normalConeLength", thruster_.normalConeLength);
			JsonToVal(t, "boostConeLength", thruster_.boostConeLength);
			JsonToVal(t, "coneRadius", thruster_.coneRadius);
			JsonToVal(t, "normalTrailLife", thruster_.normalTrailLife);
			JsonToVal(t, "boostTrailLife", thruster_.boostTrailLife);
			JsonToVal(t, "rollTrailLife", thruster_.rollTrailLife);
			JsonToVal(t, "coreTrailWidth", thruster_.coreTrailWidth);
			JsonToVal(t, "glowTrailWidth", thruster_.glowTrailWidth);
			JsonToVal(t, "magentaTrailWidth", thruster_.magentaTrailWidth);
			JsonToVal(t, "normalSparkRate", thruster_.normalSparkRate);
			JsonToVal(t, "boostSparkRate", thruster_.boostSparkRate);
			JsonToVal(t, "sparkMinScale", thruster_.sparkMinScale);
			JsonToVal(t, "sparkMaxScale", thruster_.sparkMaxScale);
			JsonToVal(t, "sparkMinLife", thruster_.sparkMinLife);
			JsonToVal(t, "sparkMaxLife", thruster_.sparkMaxLife);
			JsonToVal(t, "sparkAlpha", thruster_.sparkAlpha);
			JsonToVal(t, "boostRingCount", thruster_.boostRingCount);
			JsonToVal(t, "boostRingStartBase", thruster_.boostRingStartBase);
			JsonToVal(t, "boostRingEndBase", thruster_.boostRingEndBase);
			JsonToVal(t, "boostRingLifeBase", thruster_.boostRingLifeBase);
			JsonToVec4(t, "boostRingFirstColor", thruster_.boostRingFirstColor);
			JsonToVec4(t, "boostRingOtherColor", thruster_.boostRingOtherColor);
			JsonToVal(t, "boostFlashScaleRatio", thruster_.boostFlashScaleRatio);
			JsonToVal(t, "boostFlashAlpha", thruster_.boostFlashAlpha);
		} else {
			allLoaded = false;
		}
	}

	isLoaded_ = true;
	return allLoaded;
}

bool CyberVfxConfig::SaveAll() const {
	std::filesystem::create_directories(kDirPath);

	// 1. 共通カラーパレット
	{
		nlohmann::json j = {
			{ "pureWhite",      Vec4ToJson(palette_.pureWhite) },
			{ "cyberBlue",      Vec4ToJson(palette_.cyberBlue) },
			{ "neonMagenta",    Vec4ToJson(palette_.neonMagenta) },
			{ "neonMagentaEnd", Vec4ToJson(palette_.neonMagentaEnd) },
			{ "whiteEnd",       palette_.whiteEnd },
			{ "blueEnd",        palette_.blueEnd },
			{ "magentaEnd",     palette_.magentaEnd }
		};
		std::ofstream ofs(kPalettePath);
		if (!ofs.is_open()) return false;
		ofs << j.dump(4);
	}

	// 2. 武器＆ボムVFX
	{
		nlohmann::json root;
		root["muzzle"] = {
			{ "flashWidth",      weapon_.muzzle.flashWidth },
			{ "flashHeight",     weapon_.muzzle.flashHeight },
			{ "flashVHeight",    weapon_.muzzle.flashVHeight },
			{ "flashLife",       weapon_.muzzle.flashLife },
			{ "flashStartColor", Vec4ToJson(weapon_.muzzle.flashStartColor) },
			{ "flashEndColor",   Vec4ToJson(weapon_.muzzle.flashEndColor) },
			{ "ringStartScale",  weapon_.muzzle.ringStartScale },
			{ "ringEndScale",    weapon_.muzzle.ringEndScale },
			{ "ringLife",        weapon_.muzzle.ringLife },
			{ "ringRotSpeed",    weapon_.muzzle.ringRotSpeed },
			{ "ringStartColor",  Vec4ToJson(weapon_.muzzle.ringStartColor) },
			{ "ringEndColor",    Vec4ToJson(weapon_.muzzle.ringEndColor) },
			{ "voxelCount",      weapon_.muzzle.voxelCount },
			{ "voxelSize",       weapon_.muzzle.voxelSize },
			{ "voxelLife",       weapon_.muzzle.voxelLife },
			{ "voxelSpeedZ",     weapon_.muzzle.voxelSpeedZ }
		};

		root["spread"] = {
			{ "coreScaleX",       weapon_.spread.coreScaleX },
			{ "coreScaleY",       weapon_.spread.coreScaleY },
			{ "coreLife",         weapon_.spread.coreLife },
			{ "muzzleFlashScale", weapon_.spread.muzzleFlashScale },
			{ "ringStartScale",   weapon_.spread.ringStartScale },
			{ "ringEndScale",     weapon_.spread.ringEndScale },
			{ "ringLife",         weapon_.spread.ringLife },
			{ "ringTiltX",        weapon_.spread.ringTiltX },
			{ "ringRotSpeed",     weapon_.spread.ringRotSpeed },
			{ "ringStartColor",   Vec4ToJson(weapon_.spread.ringStartColor) },
			{ "ringEndColor",     Vec4ToJson(weapon_.spread.ringEndColor) },
			{ "glitchCount",      weapon_.spread.glitchCount },
			{ "glitchLength",     weapon_.spread.glitchLength },
			{ "glitchEndLength",  weapon_.spread.glitchEndLength },
			{ "glitchThickness",  weapon_.spread.glitchThickness },
			{ "glitchLife",       weapon_.spread.glitchLife },
			{ "voxelCount",       weapon_.spread.voxelCount },
			{ "voxelSize",        weapon_.spread.voxelSize },
			{ "voxelLife",        weapon_.spread.voxelLife },
			{ "voxelSpeed",       weapon_.spread.voxelSpeed }
		};

		root["bombExplosion"] = {
			{ "coreStartScale",       weapon_.bombExplosion.coreStartScale },
			{ "coreLife",             weapon_.bombExplosion.coreLife },
			{ "coreStartColor",       Vec4ToJson(weapon_.bombExplosion.coreStartColor) },
			{ "coreEndColor",         Vec4ToJson(weapon_.bombExplosion.coreEndColor) },
			{ "flareHLength",         weapon_.bombExplosion.flareHLength },
			{ "flareVLength",         weapon_.bombExplosion.flareVLength },
			{ "flareThickness",       weapon_.bombExplosion.flareThickness },
			{ "flareLife",            weapon_.bombExplosion.flareLife },
			{ "flareStartColor",      Vec4ToJson(weapon_.bombExplosion.flareStartColor) },
			{ "ringStartBase",        weapon_.bombExplosion.ringStartBase },
			{ "ringStartStep",        weapon_.bombExplosion.ringStartStep },
			{ "ringEndBase",          weapon_.bombExplosion.ringEndBase },
			{ "ringEndStep",          weapon_.bombExplosion.ringEndStep },
			{ "ringLifeBase",         weapon_.bombExplosion.ringLifeBase },
			{ "ringLifeStep",         weapon_.bombExplosion.ringLifeStep },
			{ "ringStartColor",       Vec4ToJson(weapon_.bombExplosion.ringStartColor) },
			{ "ringEndColor",         Vec4ToJson(weapon_.bombExplosion.ringEndColor) },
			{ "burstVoxelCount",      weapon_.bombExplosion.burstVoxelCount },
			{ "burstVoxelSpeedMin",   weapon_.bombExplosion.burstVoxelSpeedMin },
			{ "burstVoxelSpeedRange", weapon_.bombExplosion.burstVoxelSpeedRange },
			{ "burstVoxelSizeMin",    weapon_.bombExplosion.burstVoxelSizeMin },
			{ "burstVoxelSizeRange",  weapon_.bombExplosion.burstVoxelSizeRange },
			{ "burstVoxelLifeBase",   weapon_.bombExplosion.burstVoxelLifeBase },
			{ "glitchCount",          weapon_.bombExplosion.glitchCount }
		};

		root["funnelTrail"] = {
			{ "trailLife",    weapon_.funnelTrail.trailLife },
			{ "coreWidth",    weapon_.funnelTrail.coreWidth },
			{ "glowWidth",    weapon_.funnelTrail.glowWidth },
			{ "magentaWidth", weapon_.funnelTrail.magentaWidth },
			{ "coreColor",    Vec4ToJson(weapon_.funnelTrail.coreColor) },
			{ "glowColor",    Vec4ToJson(weapon_.funnelTrail.glowColor) },
			{ "magentaColor", Vec4ToJson(weapon_.funnelTrail.magentaColor) }
		};

		std::ofstream ofs(kWeaponVfxPath);
		if (!ofs.is_open()) return false;
		ofs << root.dump(4);
	}

	// 3. スラスターVFX
	{
		nlohmann::json t = {
			{ "normalCyanGlow",       Vec4ToJson(thruster_.normalCyanGlow) },
			{ "boostMagentaGlow",     Vec4ToJson(thruster_.boostMagentaGlow) },
			{ "whiteCoreColor",       Vec4ToJson(thruster_.whiteCoreColor) },
			{ "trailMagentaColor",    Vec4ToJson(thruster_.trailMagentaColor) },
			{ "normalConeLength",     thruster_.normalConeLength },
			{ "boostConeLength",      thruster_.boostConeLength },
			{ "coneRadius",           thruster_.coneRadius },
			{ "normalTrailLife",      thruster_.normalTrailLife },
			{ "boostTrailLife",       thruster_.boostTrailLife },
			{ "rollTrailLife",        thruster_.rollTrailLife },
			{ "coreTrailWidth",       thruster_.coreTrailWidth },
			{ "glowTrailWidth",       thruster_.glowTrailWidth },
			{ "magentaTrailWidth",    thruster_.magentaTrailWidth },
			{ "normalSparkRate",      thruster_.normalSparkRate },
			{ "boostSparkRate",       thruster_.boostSparkRate },
			{ "sparkMinScale",        thruster_.sparkMinScale },
			{ "sparkMaxScale",        thruster_.sparkMaxScale },
			{ "sparkMinLife",         thruster_.sparkMinLife },
			{ "sparkMaxLife",         thruster_.sparkMaxLife },
			{ "sparkAlpha",           thruster_.sparkAlpha },
			{ "boostRingCount",       thruster_.boostRingCount },
			{ "boostRingStartBase",   thruster_.boostRingStartBase },
			{ "boostRingEndBase",     thruster_.boostRingEndBase },
			{ "boostRingLifeBase",    thruster_.boostRingLifeBase },
			{ "boostRingFirstColor",  Vec4ToJson(thruster_.boostRingFirstColor) },
			{ "boostRingOtherColor",  Vec4ToJson(thruster_.boostRingOtherColor) },
			{ "boostFlashScaleRatio", thruster_.boostFlashScaleRatio },
			{ "boostFlashAlpha",      thruster_.boostFlashAlpha }
		};
		std::ofstream ofs(kThrusterVfxPath);
		if (!ofs.is_open()) return false;
		ofs << t.dump(4);
	}

	return true;
}

Vector4 CyberVfxConfig::EvaluateThreeStageColor(float ratio, float alphaScale) const {
	float r = std::clamp(ratio, 0.0f, 1.0f);
	float wEnd = std::clamp(palette_.whiteEnd, 0.01f, 0.90f);
	float bEnd = std::clamp(palette_.blueEnd, wEnd + 0.01f, 0.95f);
	float mEnd = std::clamp(palette_.magentaEnd, bEnd + 0.01f, 0.99f);

	Vector4 result{};
	if (r < wEnd) {
		result = palette_.pureWhite;
	} else if (r < bEnd) {
		float t = (r - wEnd) / (bEnd - wEnd);
		result = LerpVec4(palette_.pureWhite, palette_.cyberBlue, t);
	} else if (r < mEnd) {
		float t = (r - bEnd) / (mEnd - bEnd);
		result = LerpVec4(palette_.cyberBlue, palette_.neonMagenta, t);
	} else {
		float t = (r - mEnd) / (1.0f - mEnd);
		result = LerpVec4(palette_.neonMagenta, palette_.neonMagentaEnd, t);
	}
	result.w *= alphaScale;
	return result;
}

void CyberVfxConfig::DrawImGui(bool* pOpen) {
#ifdef ENABLE_EDITOR
	EnsureLoaded();

	if (!ImGui::Begin("Cyber VFX Config (JSON)", pOpen)) {
		ImGui::End();
		return;
	}

	if (ImGui::Button("JSON再読込 (Reload)")) {
		LoadAll();
	}
	ImGui::SameLine();
	if (ImGui::Button("JSON保存 (Save)")) {
		SaveAll();
	}
	ImGui::SameLine();
	if (ImGui::Button("初期値へ戻す")) {
		ResetToDefaults();
	}
	ImGui::Separator();

	if (ImGui::CollapsingHeader("共通カラーパレット (cyber_palette.json)", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("純白 (PureWhite)", &palette_.pureWhite.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::ColorEdit4("シアンブルー (CyberBlue)", &palette_.cyberBlue.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::ColorEdit4("ネオンマゼンタ (NeonMagenta)", &palette_.neonMagenta.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::ColorEdit4("消え際 (MagentaEnd)", &palette_.neonMagentaEnd.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::SliderFloat("純白終了比率", &palette_.whiteEnd, 0.01f, 0.40f);
		ImGui::SliderFloat("青遷移比率", &palette_.blueEnd, 0.15f, 0.75f);
		ImGui::SliderFloat("マゼンタ遷移比率", &palette_.magentaEnd, 0.40f, 0.96f);
	}

	if (ImGui::CollapsingHeader("武器＆ボムVFX (weapon_vfx.json)")) {
		if (ImGui::TreeNode("マズルフラッシュ (通常・ファンネル)")) {
			ImGui::DragFloat("フラッシュ横幅", &weapon_.muzzle.flashWidth, 0.05f, 0.1f, 20.0f);
			ImGui::DragFloat("フラッシュ寿命(s)", &weapon_.muzzle.flashLife, 0.01f, 0.02f, 1.0f);
			ImGui::DragFloat("リング終了サイズ", &weapon_.muzzle.ringEndScale, 0.05f, 0.2f, 10.0f);
			ImGui::DragFloat("リング寿命(s)", &weapon_.muzzle.ringLife, 0.01f, 0.02f, 1.0f);
			ImGui::SliderInt("ボクセル数", &weapon_.muzzle.voxelCount, 0, 12);
			ImGui::DragFloat("ボクセルサイズ", &weapon_.muzzle.voxelSize, 0.01f, 0.02f, 2.0f);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("拡散弾マズル")) {
			ImGui::DragFloat("リング終了サイズ##Spread", &weapon_.spread.ringEndScale, 0.05f, 0.5f, 15.0f);
			ImGui::DragFloat("リング寿命(s)##Spread", &weapon_.spread.ringLife, 0.01f, 0.05f, 1.0f);
			ImGui::SliderInt("グリッチ数##Spread", &weapon_.spread.glitchCount, 0, 16);
			ImGui::SliderInt("ボクセル数##Spread", &weapon_.spread.voxelCount, 0, 24);
			ImGui::TreePop();
		}
		if (ImGui::TreeNode("ボム爆発")) {
			ImGui::DragFloat("コア閃光サイズ", &weapon_.bombExplosion.coreStartScale, 0.2f, 1.0f, 40.0f);
			ImGui::DragFloat("リング最大サイズベース", &weapon_.bombExplosion.ringEndBase, 0.2f, 1.0f, 30.0f);
			ImGui::DragFloat("リング寿命ベース(s)", &weapon_.bombExplosion.ringLifeBase, 0.01f, 0.1f, 2.0f);
			ImGui::ColorEdit4("リング開始色", &weapon_.bombExplosion.ringStartColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
			ImGui::ColorEdit4("リング終了色", &weapon_.bombExplosion.ringEndColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
			ImGui::SliderInt("爆発ボクセル数", &weapon_.bombExplosion.burstVoxelCount, 10, 160);
			ImGui::SliderInt("グリッチ線数", &weapon_.bombExplosion.glitchCount, 0, 24);
			ImGui::TreePop();
		}
	}

	if (ImGui::CollapsingHeader("スラスターVFX (thruster_vfx.json)")) {
		ImGui::ColorEdit4("通常シアングロー", &thruster_.normalCyanGlow.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::ColorEdit4("ブーストマゼンタ", &thruster_.boostMagentaGlow.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::ColorEdit4("尾アウターマゼンタ", &thruster_.trailMagentaColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		ImGui::DragFloat("通常コーン長", &thruster_.normalConeLength, 0.05f, 0.2f, 6.0f);
		ImGui::DragFloat("ブーストコーン長", &thruster_.boostConeLength, 0.05f, 0.5f, 8.0f);
		ImGui::DragFloat("通常トレイル寿命(s)", &thruster_.normalTrailLife, 0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("スパーク発生レート(通常)", &thruster_.normalSparkRate, 1.0f, 0.0f, 200.0f);
		ImGui::DragFloat("スパーク発生レート(ブースト)", &thruster_.boostSparkRate, 1.0f, 0.0f, 300.0f);
		ImGui::DragFloat("ブースト衝撃波終了サイズ", &thruster_.boostRingEndBase, 0.05f, 0.5f, 8.0f);
	}

	ImGui::End();
#else
	(void)pOpen;
#endif
}
