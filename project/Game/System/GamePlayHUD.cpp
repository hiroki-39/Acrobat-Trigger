#define NOMINMAX
#include "GamePlayHUD.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "Game/Actor/Weapon/MemoryType.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/2d/SpriteCommon.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/UI/UITextManager.h"
#include "KHEngine/Math/Matrix4x4.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>

void GamePlayHUD::Initialize(SpriteCommon* spriteCommon, uint32_t whiteTexIndex, uint32_t ringIconTex, uint32_t ringIconOutlineTex)
{
	if (!spriteCommon) return;
	// HPバー
	hpBarBgSprite_ = std::make_unique<Sprite>();
	if (hpBarBgSprite_)
	{
		hpBarBgSprite_->Initialize(spriteCommon, whiteTexIndex);
		hpBarBgSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		hpBarBgSprite_->SetPosition(Vector2(20.0f, 720.0f - 32.0f - 20.0f));
		hpBarBgSprite_->SetSize(Vector2(400.0f, 32.0f));
		hpBarBgSprite_->SetColor(Vector4(0.8f, 0.8f, 0.8f, 0.8f));
		hpBarBgSprite_->Update();
	}

	hpBarSprite_ = std::make_unique<Sprite>();
	if (hpBarSprite_)
	{
		hpBarSprite_->Initialize(spriteCommon, whiteTexIndex);
		hpBarSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		hpBarSprite_->SetPosition(Vector2(20.0f, 720.0f - 32.0f - 20.0f));
		hpBarSprite_->SetSize(Vector2(400.0f, 32.0f));
		hpBarSprite_->SetColor(Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		hpBarSprite_->Update();
	}

	// ブーストゲージバー（HPバーの上側に配置: Y = 668 - 14 - 6 = 648）
	boostBarBgSprite_ = std::make_unique<Sprite>();
	if (boostBarBgSprite_)
	{
		boostBarBgSprite_->Initialize(spriteCommon, whiteTexIndex);
		boostBarBgSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		boostBarBgSprite_->SetPosition(Vector2(20.0f, 646.0f));
		boostBarBgSprite_->SetSize(Vector2(400.0f, 14.0f));
		boostBarBgSprite_->SetColor(Vector4(0.2f, 0.25f, 0.35f, 0.75f));
		boostBarBgSprite_->Update();
	}

	boostBarSprite_ = std::make_unique<Sprite>();
	if (boostBarSprite_)
	{
		boostBarSprite_->Initialize(spriteCommon, whiteTexIndex);
		boostBarSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		boostBarSprite_->SetPosition(Vector2(20.0f, 646.0f));
		boostBarSprite_->SetSize(Vector2(400.0f, 14.0f));
		boostBarSprite_->SetColor(Vector4(0.0f, 0.75f, 1.0f, 1.0f));
		boostBarSprite_->Update();
	}

	// 強化リング獲得アイコン（ブーストゲージの上部に横2つ配置）
	// ブーストバー: Y = 646.0f -> アイコンサイズ 36x36、Y = 646 - 36 - 6 = 604.0f
	float ringIconSize = 36.0f;
	float ringIconY = 646.0f - ringIconSize - 6.0f; // 604.0f
	float ringIconSpacing = 8.0f;
	float ringIconStartX = 20.0f;

	for (int i = 0; i < kMaxEnhanceRingIcons; ++i)
	{
		float iconX = ringIconStartX + i * (ringIconSize + ringIconSpacing);

		// 中身アイコン（獲得時に表示）
		ringGetIconSprites_[i] = std::make_unique<Sprite>();
		if (ringGetIconSprites_[i])
		{
			ringGetIconSprites_[i]->Initialize(spriteCommon, ringIconTex);
			ringGetIconSprites_[i]->SetAnchorPoint(Vector2(0.0f, 0.0f));
			ringGetIconSprites_[i]->SetPosition(Vector2(iconX, ringIconY));
			ringGetIconSprites_[i]->SetSize(Vector2(ringIconSize, ringIconSize));
			ringGetIconSprites_[i]->SetColor(ringIconColor_);
			ringGetIconSprites_[i]->Update();
		}

		// 枠アイコン（常時表示、描画順は中身の上）
		ringGetOutlineSprites_[i] = std::make_unique<Sprite>();
		if (ringGetOutlineSprites_[i])
		{
			ringGetOutlineSprites_[i]->Initialize(spriteCommon, ringIconOutlineTex);
			ringGetOutlineSprites_[i]->SetAnchorPoint(Vector2(0.0f, 0.0f));
			ringGetOutlineSprites_[i]->SetPosition(Vector2(iconX, ringIconY));
			ringGetOutlineSprites_[i]->SetSize(Vector2(ringIconSize, ringIconSize));
			ringGetOutlineSprites_[i]->SetColor(ringOutlineColor_);
			ringGetOutlineSprites_[i]->Update();
		}
	}
	acquiredEnhanceRingCount_ = 0;
}

void GamePlayHUD::AddScorePopup(const Vector3& worldPos, int score, float duration)
{
	scorePopups_.push_back({ worldPos, score, 0.0f, duration });
}

void GamePlayHUD::UpdateScorePopups(float dt)
{
	for (auto it = scorePopups_.begin(); it != scorePopups_.end();)
	{
		it->timer += dt;
		if (it->timer >= it->duration)
		{
			it = scorePopups_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void GamePlayHUD::UpdateBars(float dt, Player* player)
{
	if (!player) return;
		if (hpBarSprite_)
		{
			float hpRate = (float)player->GetHp() / player->GetMaxHp();
			if (hpRate < 0.0f) hpRate = 0.0f;
			hpBarSprite_->SetSize(Vector2(400.0f * hpRate, 32.0f));

			// HPに応じて色を変更
			if (hpRate > 0.5f)
			{
				hpBarSprite_->SetColor(Vector4(0.0f, 1.0f, 0.0f, 1.0f)); // 緑
			}
			else if (hpRate > 0.2f)
			{
				hpBarSprite_->SetColor(Vector4(1.0f, 1.0f, 0.0f, 1.0f)); // 黄色
			}
			else
			{
				hpBarSprite_->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤
			}
			hpBarSprite_->Update();
		}

		if (boostBarSprite_)
		{
			float boostRatio = player->GetBoostRatio();
			boostBarSprite_->SetSize(Vector2(400.0f * boostRatio, 14.0f));

			if (player->IsBoostOverheated())
			{
				// オーバーヒート中は警告赤点滅
								overheatBlinkTimer_ += dt;
				float blink = 0.5f + 0.5f * std::sin(overheatBlinkTimer_ * 14.0f);
				boostBarSprite_->SetColor(Vector4(1.0f, 0.2f * blink, 0.2f * blink, 0.95f));
			}
			else if (player->IsBoosting())
			{
				// ブースト発動中は高出力発光シアン
				boostBarSprite_->SetColor(Vector4(0.3f, 0.95f, 1.0f, 1.0f));
			}
			else if (boostRatio < 0.25f)
			{
				// 残量低下時はオレンジ
				boostBarSprite_->SetColor(Vector4(1.0f, 0.6f, 0.1f, 1.0f));
			}
			else
			{
				// 通常・回復中は綺麗なブーストシアン
				boostBarSprite_->SetColor(Vector4(0.0f, 0.75f, 1.0f, 1.0f));
			}
			boostBarSprite_->Update();
		}
		if (boostBarBgSprite_)
		{
			boostBarBgSprite_->Update();
		}
}

void GamePlayHUD::UpdateHudTexts(Player* player, BossFinisherSequence* finisher, int score, bool isCutscene, bool isGameOver, bool isClear, float cutsceneTimer)
{
	// ゲーム内通常HUD（スコア、ロックオン、HP、操作説明等）の表示・フェード制御
	if (isCutscene)
	{
		// 黒帯が出ている間（0.0s〜2.6s）はゲーム中HUDを完全非表示。黒帯が引くタイミング（2.6s〜3.0s）で滑らかにフェードイン
		float hudAlpha = 0.0f;
		if (cutsceneTimer >= 2.6f)
		{
			hudAlpha = std::clamp((cutsceneTimer - 2.6f) / 0.4f, 0.0f, 1.0f);
		}
		UITextManager::GetInstance()->SetHudAlpha(hudAlpha);
	}
	else if (isGameOver || isClear || (finisher && finisher->IsMoviePhase()))
	{
		// ゲームオーバー・ゲームクリア・処刑黒帯ムービー中はゲーム中HUDを完全非表示
		UITextManager::GetInstance()->SetHudAlpha(0.0f);
	}
	else
	{
		UITextManager::GetInstance()->SetHudAlpha(1.0f);
	}

	// UIテキストの動的更新 (スコア、ロックオン数、HP)
	if (player)
	{
		bool isFinisherMovie = (finisher && finisher->IsMoviePhase());
		if (isGameOver || isClear || isCutscene || isFinisherMovie)
		{
			// ゲームオーバー・ゲームクリア・スタート演出中・処刑ムービー中は通常HUDテキストをクリア（非表示）
			UITextManager::GetInstance()->SetText("Score", "");
			UITextManager::GetInstance()->SetText("LockOn", "");
			UITextManager::GetInstance()->SetText("PlayerHP", "");
		}
		else
		{
			char scoreStr[64];
			snprintf(scoreStr, sizeof(scoreStr), "SCORE: %06d", score);
			UITextManager::GetInstance()->SetText("Score", scoreStr);

			// 武器形態（メモリタイプ）に応じた残弾テキストおよびカラー表示
			char weaponStr[64] = "";
			Vector4 weaponColor = { 1.0f, 1.0f, 1.0f, 1.0f };

			if (finisher && finisher->IsActive() && finisher->IsRedMemoryAbsorbed())
			{
				int execAmmo = finisher->HasFiredMissiles() ? 0 : 4;
				snprintf(weaponStr, sizeof(weaponStr), "処刑弾: %d/4", execAmmo);
				weaponColor = { 1.0f, 0.15f, 0.35f, 1.0f }; // 処刑赤メモリカラー
			}
			else
			{
				MemoryType memType = player->GetEquippedMemoryType();
				int currentAmmo = player->GetWeaponAmmo();
				int maxAmmo = player->GetWeaponMaxAmmo();

				switch (memType)
				{
				case MemoryType::BOMB:
					snprintf(weaponStr, sizeof(weaponStr), "ボム: %d/%d", currentAmmo, maxAmmo);
					weaponColor = { 1.0f, 0.2f, 0.2f, 1.0f }; // ネオンレッド
					break;
				case MemoryType::CYBER_BIT:
				case MemoryType::FUNNEL:
					snprintf(weaponStr, sizeof(weaponStr), "ファンネル: %d/%d", currentAmmo, maxAmmo);
					weaponColor = { 1.0f, 0.65f, 0.12f, 1.0f }; // エレクトリックオレンジ/ゴールド
					break;
				case MemoryType::SPREAD:
					snprintf(weaponStr, sizeof(weaponStr), "拡散弾: %d/%d", currentAmmo, maxAmmo);
					weaponColor = { 0.2f, 1.0f, 0.35f, 1.0f }; // ネオングリーン
					break;
				case MemoryType::CHARGE_SHOT:
					snprintf(weaponStr, sizeof(weaponStr), "チャージ: %d/%d", currentAmmo, maxAmmo);
					weaponColor = { 1.0f, 0.95f, 0.15f, 1.0f }; // ネオンイエロー
					break;
				case MemoryType::LASER_BEAM:
					snprintf(weaponStr, sizeof(weaponStr), "レーザー: %d/%d", currentAmmo, maxAmmo);
					weaponColor = { 0.2f, 0.6f, 1.0f, 1.0f }; // ディープブルー
					break;
				case MemoryType::NONE:
				default:
					snprintf(weaponStr, sizeof(weaponStr), "通常弾: ∞/∞");
					weaponColor = { 0.4f, 0.9f, 1.0f, 1.0f }; // ネオンシアン
					break;
				}
			}

			UITextManager::GetInstance()->SetText("LockOn", weaponStr);
			UITextManager::GetInstance()->SetColor("LockOn", weaponColor, "GAMEPLAY");

			char hpStr[64];
			snprintf(hpStr, sizeof(hpStr), "HP: %d / %d", player->GetHp(), player->GetMaxHp());
			UITextManager::GetInstance()->SetText("PlayerHP", hpStr);
		}
}

}
void GamePlayHUD::DrawSprites()
{
		// 強化リング獲得アイコン（ブーストゲージの上部に横2つ配置）
		// 描画順: 中身 (ringGet_icon.png) を先に描き、その上から枠 (ringGet_icon_outline.png) を重ねて描画
		for (int i = 0; i < kMaxEnhanceRingIcons; ++i)
		{
			// 1. 中身（強化リング獲得時のみ表示）
			if (i < acquiredEnhanceRingCount_ && ringGetIconSprites_[i])
			{
				ringGetIconSprites_[i]->SetColor(ringIconColor_);
				ringGetIconSprites_[i]->Update();
				ringGetIconSprites_[i]->Draw();
			}

			// 2. 枠（常時表示。描画順は枠が上）
			if (ringGetOutlineSprites_[i])
			{
				ringGetOutlineSprites_[i]->SetColor(ringOutlineColor_);
				ringGetOutlineSprites_[i]->Update();
				ringGetOutlineSprites_[i]->Draw();
			}
		}

		// ブーストバー（HPバーの上）
		if (boostBarBgSprite_) { boostBarBgSprite_->Update(); boostBarBgSprite_->Draw(); }
		if (boostBarSprite_) { boostBarSprite_->Update(); boostBarSprite_->Draw(); }

		// HPバー
		if (hpBarBgSprite_) { hpBarBgSprite_->Update(); hpBarBgSprite_->Draw(); }
		if (hpBarSprite_) { hpBarSprite_->Update(); hpBarSprite_->Draw(); }
}

void GamePlayHUD::DrawOverlayUI(bool isPlayingPhase, bool isFinisherMovie, Player* player, Camera* activeCamera)
{
#ifdef USE_IMGUI
	if (ImGui::GetCurrentContext() == nullptr) return;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) return;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	if (!drawList) return;

	ImVec2 viewPos(0.0f, 0.0f);
	ImVec2 viewSize = viewport->Size;
#ifdef ENABLE_EDITOR
	if (EngineServices::GetInstance()->GetEditorMode())
	{
		viewPos = EditorSystem::GetInstance()->GetViewportPos();
		viewSize = EditorSystem::GetInstance()->GetViewportSize();
		if (viewSize.x <= 10.0f || viewSize.y <= 10.0f) return;
	}
#endif

	float screenW = viewSize.x;
	float screenH = viewSize.y;
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);
	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.3f) scale = 0.3f;

	// ===== 弾道予告ガイドライン（手前照準 / 銃口 -> 奥照準） =====
		if (isPlayingPhase && !isFinisherMovie && player && !player->IsDead() && activeCamera)
	{
		Vector3 pStart = player->GetFrontReticleWorldPosition();
		Vector3 pEnd = player->GetReticleWorldPosition();

		const Matrix4x4& vMat = activeCamera->GetViewMatrix();
		const Matrix4x4& pMat = activeCamera->GetProjectionMatrix();
		Matrix4x4 vpMat = vMat * pMat;

		auto WorldToScreen = [&](const Vector3& world) -> std::pair<bool, ImVec2> {
			float w = world.x * vpMat.m[0][3] + world.y * vpMat.m[1][3] + world.z * vpMat.m[2][3] + vpMat.m[3][3];
			if (w < 0.1f) return { false, ImVec2(0, 0) };
			float x = (world.x * vpMat.m[0][0] + world.y * vpMat.m[1][0] + world.z * vpMat.m[2][0] + vpMat.m[3][0]) / w;
			float y = (world.x * vpMat.m[0][1] + world.y * vpMat.m[1][1] + world.z * vpMat.m[2][1] + vpMat.m[3][1]) / w;
			float sx = viewPos.x + (x * 0.5f + 0.5f) * screenW;
			float sy = viewPos.y + (-y * 0.5f + 0.5f) * screenH;
			return { true, ImVec2(sx, sy) };
		};

		auto [okStart, ptStart] = WorldToScreen(pStart);
		auto [okEnd, ptEnd] = WorldToScreen(pEnd);
		Vector3 pShip = player->GetWorldPosition();
		auto [okShip, ptShip] = WorldToScreen(pShip);

		Vector3 pLeftWing = player->GetLeftWingPosition();
		Vector3 pRightWing = player->GetRightWingPosition();
		auto [okLeft, ptLeft] = WorldToScreen(pLeftWing);
		auto [okRight, ptRight] = WorldToScreen(pRightWing);

		// 1. 弾道予告線（ユーザー指示により非表示）
		/*
		if (okEnd)
		{
			bool isLocked = player->IsTargetInReticle();
			ImU32 lineCol = isLocked ? IM_COL32(255, 235, 50, 76) : IM_COL32(220, 245, 255, 56);
			ImU32 shadowCol = IM_COL32(0, 0, 0, 36);

			auto DrawMuzzleRay = [&](const ImVec2& ptMuzzle) {
				float dist = std::hypot(ptEnd.x - ptMuzzle.x, ptEnd.y - ptMuzzle.y);
				if (dist > 8.0f)
				{
					int segs = (std::max)(2, (int)(dist / 16.0f));
					for (int i = 0; i < segs; ++i)
					{
						if (i % 2 == 0)
						{
							float t0 = (float)i / segs;
							float t1 = (float)(i + 1) / segs;
							ImVec2 pA(ptMuzzle.x + (ptEnd.x - ptMuzzle.x) * t0, ptMuzzle.y + (ptEnd.y - ptMuzzle.y) * t0);
							ImVec2 pB(ptMuzzle.x + (ptEnd.x - ptMuzzle.x) * t1, ptMuzzle.y + (ptEnd.y - ptMuzzle.y) * t1);
							drawList->AddLine(pA, pB, shadowCol, 2.2f * scale);
							drawList->AddLine(pA, pB, lineCol, 1.2f * scale);
						}
					}
				}
			};

			if (okLeft) DrawMuzzleRay(ptLeft);
			if (okRight) DrawMuzzleRay(ptRight);
		}
		*/

		// 2. ダブルレティクル【スターフォックス風デュアル・スクエア・レティクル】
		// ① 手前照準（インナー四角フレーム / 自機銃口HUD）
		// 自機機首の正面方向に完全同期し、自機の主翼ロール角に合わせてフレームがリアルタイムに傾く！
		if (okShip || okStart)
		{
			// 自機のZ軸回転（ロール角・主翼傾き）を画面上の角度として直接算出！
			// 自機が右に30度傾いたら（右バンク）、右翼が下がり左翼が上がるため dy > 0 となり、手前レティクルも右に30度傾く！
			float rollAngle = 0.0f;
			if (okLeft && okRight)
			{
				float dx = ptRight.x - ptLeft.x;
				float dy = ptRight.y - ptLeft.y;
				rollAngle = std::atan2(dy, dx);
			}
			else
			{
				rollAngle = player->GetFinalBank();
			}

			float cosR = std::cos(rollAngle);
			float sinR = std::sin(rollAngle);
			// 画面上での自機ローカル基底ベクトル: u=(cos, sin), v=(-sin, cos)
			ImVec2 axisU(cosR, sinR);
			ImVec2 axisV(-sinR, cosR);

			float innerSize = 21.0f * scale;      // インナーフレーム半幅（奥照準28.0fに対して程よいサイズ感）
			float cornerLen = 8.5f * scale;       // L字の角の長さ（四角枠としてしっかり視認できる長さ）
			float stroke = 2.4f * scale;          // 線の太さ
			float shadowStroke = stroke + 2.6f;   // 黒縁アウトライン太さ（背景遮断用）

			// 2つの照準の色を統一（通常時: 純白ホワイト + 黒縁アウトライン、ロック時: 高輝度ゴールド）
			bool isLocked = player->IsTargetInReticle();
			ImU32 innerColor = isLocked ? IM_COL32(255, 230, 45, 255) : IM_COL32(255, 255, 255, 255);
			ImU32 shadowColor = IM_COL32(0, 0, 0, 230);

			// Position front reticle firmly in front of ship in 3D space (Z+14m)
			ImVec2 c = okStart ? ptStart : (okShip ? ptShip : ptEnd);

			// コーナーブラケット描画ヘルパーラムダ（回転対応）
			auto DrawRotatedCorner = [&](float uDist, float vDist, float du, float dv, ImU32 col, float th) {
				ImVec2 cornerPos(c.x + axisU.x * uDist + axisV.x * vDist, c.y + axisU.y * uDist + axisV.y * vDist);
				ImVec2 armU(cornerPos.x + axisU.x * du, cornerPos.y + axisU.y * du);
				ImVec2 armV(cornerPos.x + axisV.x * dv, cornerPos.y + axisV.y * dv);
				drawList->AddLine(cornerPos, armU, col, th);
				drawList->AddLine(cornerPos, armV, col, th);
			};

			// 四隅のL字ブラケット [ ] （極太黒アウトライン付きでシアン背景・オーロラでも完全視認）
			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 col = (pass == 0) ? shadowColor : innerColor;
				float th = (pass == 0) ? shadowStroke : stroke;

				// 左上
				DrawRotatedCorner(-innerSize, -innerSize, cornerLen, cornerLen, col, th);
				// 右上
				DrawRotatedCorner(innerSize, -innerSize, -cornerLen, cornerLen, col, th);
				// 左下
				DrawRotatedCorner(-innerSize, innerSize, cornerLen, -cornerLen, col, th);
				// 右下
				DrawRotatedCorner(innerSize, innerSize, -cornerLen, -cornerLen, col, th);
			}

			// 中心精密クロスヘア & 銃口ドット（自機正面の射線中心、自機ロール連動）
			float crossLen = 4.0f * scale;
			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 col = (pass == 0) ? shadowColor : innerColor;
				float th = (pass == 0) ? stroke + 2.0f : stroke * 0.85f;

				ImVec2 uA(c.x - axisU.x * crossLen, c.y - axisU.y * crossLen);
				ImVec2 uB(c.x + axisU.x * crossLen, c.y + axisU.y * crossLen);
				ImVec2 vA(c.x - axisV.x * crossLen, c.y - axisV.y * crossLen);
				ImVec2 vB(c.x + axisV.x * crossLen, c.y + axisV.y * crossLen);
				drawList->AddLine(uA, uB, col, th);
				drawList->AddLine(vA, vB, col, th);
			}
			drawList->AddCircleFilled(c, 2.5f * scale, shadowColor);
			drawList->AddCircleFilled(c, 1.4f * scale, innerColor);
		}

		// ② 奥照準（アウター四角フレーム / ターゲット枠） 2D UIスクリーン描画
		// PosTarget3D = PosShip + (DirAim * DistFar)
		// PosScreen2D = WorldToScreen(PosTarget3D)
		// UI描画のため地形による遮蔽（Z埋もれ）が発生せず、常にシャープにターゲットを捕捉
		if (okEnd)
		{
			bool isLocked = player->IsTargetInReticle();
			float reticleBaseSize = 28.0f * scale; // 基準サイズ（UI解像度スケール対応）
			float cornerLen = 10.0f * scale;       // L字の角の長さ
			float stroke = 2.4f * scale;          // 線の太さ
			float shadowStroke = stroke + 2.4f;   // 黒縁シャドウの太さ

			// 手前照準と同一の白色（ロック時は揃ってゴールド）で統一
			ImU32 frameColor = isLocked ? IM_COL32(255, 230, 45, 255) : IM_COL32(255, 255, 255, 255);
			ImU32 shadowColor = IM_COL32(0, 0, 0, 230);

			if (isLocked)
			{
				reticleBaseSize *= 0.88f; // スナップ収縮
			}

			float halfW = reticleBaseSize;
			float halfH = reticleBaseSize;
			ImVec2 c = ptEnd; // 奥照準の2D画面座標

			// コーナーブラケット描画ヘルパーラムダ
			auto DrawCorner = [&](float x, float y, float dx, float dy, ImU32 col, float th) {
				drawList->AddLine(ImVec2(x, y), ImVec2(x + dx, y), col, th);
				drawList->AddLine(ImVec2(x, y), ImVec2(x, y + dy), col, th);
			};

			// 四隅のL字ブラケット [ ] （黒アウトライン付きで雪原や空でも背景同化を完全防止）
			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 col = (pass == 0) ? shadowColor : frameColor;
				float th = (pass == 0) ? shadowStroke : stroke;

				// 左上
				DrawCorner(c.x - halfW, c.y - halfH, cornerLen, cornerLen, col, th);
				// 右上
				DrawCorner(c.x + halfW, c.y - halfH, -cornerLen, cornerLen, col, th);
				// 左下
				DrawCorner(c.x - halfW, c.y + halfH, cornerLen, -cornerLen, col, th);
				// 右下
				DrawCorner(c.x + halfW, c.y + halfH, -cornerLen, -cornerLen, col, th);
			}

			// 中央精密クロスヘア（十字マーク）
			float crossSize = 5.0f * scale;
			drawList->AddLine(ImVec2(c.x - crossSize, c.y), ImVec2(c.x + crossSize, c.y), shadowColor, stroke + 2.0f);
			drawList->AddLine(ImVec2(c.x, c.y - crossSize), ImVec2(c.x, c.y + crossSize), shadowColor, stroke + 2.0f);
			drawList->AddLine(ImVec2(c.x - crossSize, c.y), ImVec2(c.x + crossSize, c.y), frameColor, stroke);
			drawList->AddLine(ImVec2(c.x, c.y - crossSize), ImVec2(c.x, c.y + crossSize), frameColor, stroke);

			// 敵ロックオン時のサークルインジケーター演出
			if (isLocked)
			{
				float ringRadius = halfW * 1.35f;
				drawList->AddCircle(c, ringRadius, IM_COL32(255, 210, 35, 120), 24, 1.8f * scale);
			}
		}

		// =========================================================================
		// 【2D ロックオン HUD レティクル】
		// 3Dオブジェクトから2D最前面HUD描画へ完全移行。
		// 地形潜り込み・Z遮蔽・解像度低下を根絶し、常に鮮明に敵の上に追従表示。
		// =========================================================================
		std::vector<LockOnTargetHUD> hudLockTargets = player->GetLockOnTargets();

		// 通常照準での単体ロックオン時（マルチロックオン武器非装備時、または単体ロックオン中）
		if (hudLockTargets.empty() && player->IsLockOn())
		{
			LockOnTargetHUD singleLock;
			singleLock.targetPos = player->GetLockOnTargetPos();
			singleLock.lockedTime = 20.0f;
			singleLock.completeTime = 20.0f;
			singleLock.isCompleted = true;
			singleLock.lockIndex = 1;
			hudLockTargets.push_back(singleLock);
		}

		for (const auto& lt : hudLockTargets)
		{
			auto [okLock, ptLock] = WorldToScreen(lt.targetPos);
			if (!okLock) continue;

			// 画面境界クリッピング（画面外ならスキップ）
			if (ptLock.x < viewPos.x - 50.0f || ptLock.x > viewPos.x + screenW + 50.0f ||
				ptLock.y < viewPos.y - 50.0f || ptLock.y > viewPos.y + screenH + 50.0f)
			{
				continue;
			}

			float progress = std::clamp(lt.lockedTime / (std::max)(1.0f, lt.completeTime), 0.0f, 1.0f);
			bool isDone = lt.isCompleted || (progress >= 1.0f);

			float baseSize = 22.0f * scale;
			float curSize = isDone ? baseSize : (baseSize * (1.6f - 0.6f * progress));
			float cornerL = 7.0f * scale;
			float strokeW = 2.0f * scale;
			float shadowW = strokeW + 2.2f;

			// ロック完了時はゴールド/オレンジパルス、進行中はサイバーシアン
			static float lockPulseTimer = 0.0f;
			lockPulseTimer += 0.05f;
			float pulse = 0.5f + 0.5f * std::sin(lockPulseTimer * 10.0f);

			ImU32 markColor = isDone ?
				IM_COL32(255, (int)(180 + 55 * pulse), 30, 255) :
				IM_COL32(0, 220, 255, (int)(180 + 70 * progress));
			ImU32 shadowColor = IM_COL32(0, 0, 0, 220);

			// コーナーブラケット [ ] 描画
			auto DrawLockCorner = [&](float x, float y, float dx, float dy, ImU32 col, float th) {
				drawList->AddLine(ImVec2(x, y), ImVec2(x + dx, y), col, th);
				drawList->AddLine(ImVec2(x, y), ImVec2(x, y + dy), col, th);
			};

			float hx = curSize;
			float hy = curSize;

			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 c = (pass == 0) ? shadowColor : markColor;
				float th = (pass == 0) ? shadowW : strokeW;

				// 左上
				DrawLockCorner(ptLock.x - hx, ptLock.y - hy, cornerL, cornerL, c, th);
				// 右上
				DrawLockCorner(ptLock.x + hx, ptLock.y - hy, -cornerL, cornerL, c, th);
				// 左下
				DrawLockCorner(ptLock.x - hx, ptLock.y + hy, cornerL, -cornerL, c, th);
				// 右下
				DrawLockCorner(ptLock.x + hx, ptLock.y + hy, -cornerL, -cornerL, c, th);
			}

			// 中心ターゲットドット（完了時）または進行度円弧ゲージ（チャージ中）
			if (isDone)
			{
				drawList->AddCircleFilled(ptLock, 3.0f * scale, shadowColor);
				drawList->AddCircleFilled(ptLock, 1.8f * scale, markColor);

				// 四隅の突起ライン
				float crossArm = 3.5f * scale;
				drawList->AddLine(ImVec2(ptLock.x - hx - crossArm, ptLock.y), ImVec2(ptLock.x - hx, ptLock.y), markColor, strokeW);
				drawList->AddLine(ImVec2(ptLock.x + hx, ptLock.y), ImVec2(ptLock.x + hx + crossArm, ptLock.y), markColor, strokeW);
				drawList->AddLine(ImVec2(ptLock.x, ptLock.y - hy - crossArm), ImVec2(ptLock.x, ptLock.y - hy), markColor, strokeW);
				drawList->AddLine(ImVec2(ptLock.x, ptLock.y + hy), ImVec2(ptLock.x, ptLock.y + hy + crossArm), markColor, strokeW);
			}
			else
			{
				// ロックオンチャージ中の円弧ゲージ（0〜360度）
				float gaugeRadius = curSize * 1.25f;
				int segments = 24;
				drawList->PathClear();
				drawList->PathArcTo(ptLock, gaugeRadius, -1.57079f, -1.57079f + 6.28318f * progress, segments);
				drawList->PathStroke(shadowColor, 0, strokeW + 2.0f);
				drawList->PathClear();
				drawList->PathArcTo(ptLock, gaugeRadius, -1.57079f, -1.57079f + 6.28318f * progress, segments);
				drawList->PathStroke(markColor, 0, strokeW);
			}

			// ロックオン番号ラベル（L1, L2 ...）
			if (lt.lockIndex > 0)
			{
				char idxBuf[16];
				snprintf(idxBuf, sizeof(idxBuf), "L%d", lt.lockIndex);
				float fontS = 13.0f * scale;
				ImFont* f = ImGui::GetFont();
				ImVec2 sz = f->CalcTextSizeA(fontS, FLT_MAX, 0.0f, idxBuf);
				ImVec2 lblPos(ptLock.x + hx + 3.0f * scale, ptLock.y - hy - 2.0f * scale);
				drawList->AddText(f, fontS, ImVec2(lblPos.x + 1.0f, lblPos.y + 1.0f), shadowColor, idxBuf);
				drawList->AddText(f, fontS, lblPos, markColor, idxBuf);
			}
		}

		// 3. 敵撃破時のスコア加算ポップアップ演出（3D座標に「+100」などが一瞬ポップして消える）
		for (const auto& popup : scorePopups_)
		{
			float t = popup.timer / popup.duration;
			if (t >= 1.0f) continue;

			// 時間経過でふわっと少し上へ上昇
			Vector3 popWorld = popup.worldPos + Vector3(0.0f, popup.timer * 3.5f, 0.0f);
			auto [okPop, ptPop] = WorldToScreen(popWorld);
			if (okPop)
			{
				// 透明度（後半25%で滑らかにフェードアウト）
				float alpha = (t < 0.75f) ? 1.0f : (1.0f - (t - 0.75f) / 0.25f);
				int a = std::clamp((int)(alpha * 255), 0, 255);
				int shadowA = std::clamp((int)(alpha * 230), 0, 230);

				// 出現時の一瞬のポップ拡大弾性演出
				float popScale = (t < 0.12f) ? (1.0f + (0.12f - t) * 2.2f) : 1.0f;
				float fontSize = 22.0f * scale * popScale;

				char buf[32];
				snprintf(buf, sizeof(buf), "+%d", popup.score);

				ImFont* font = ImGui::GetFont();
				ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
				ImVec2 textPos(ptPop.x - textSize.x * 0.5f, ptPop.y - textSize.y * 0.5f);

				// デジタルネオンイエロー / ゴールド
				ImU32 textCol = IM_COL32(255, 235, 45, a);
				ImU32 shadowCol = IM_COL32(0, 0, 0, shadowA);

				// 8方向の濃い黒縁シャドウ（雪原やオーロラでも完璧に視認できるデジタル輪郭）
				float o = 1.8f * scale;
				drawList->AddText(font, fontSize, ImVec2(textPos.x - o, textPos.y), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x + o, textPos.y), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x, textPos.y - o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x, textPos.y + o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x - o, textPos.y - o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x + o, textPos.y - o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x - o, textPos.y + o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x + o, textPos.y + o), shadowCol, buf);

				// メインテキスト
				drawList->AddText(font, fontSize, textPos, textCol, buf);
			}
		}
	}

	drawList->PopClipRect();
#endif
}

