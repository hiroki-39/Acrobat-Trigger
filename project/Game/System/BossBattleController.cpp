#define NOMINMAX
#include "BossBattleController.h"
#include "Game/System/Rail.h"
#include "Game/System/RailCameraController.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/2d/SpriteCommon.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/UI/UITextManager.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "externals/imgui/imgui.h"
#include "externals/nlohmann/json.hpp"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstdio>
void BossBattleController::Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, uint32_t whiteTexIndex)
{
	bossTimeOverChargeSphere_ = std::make_unique<Object3d>();
	bossTimeOverChargeSphere_->Initialize(object3dCommon);
	bossTimeOverChargeSphere_->SetModel("collider_sphere_enemy.obj");
	bossTimeOverChargeSphere_->SetEnableLighting(false);
	bossTimeOverChargeSphere_->SetColor({ 1.0f, 0.25f, 0.1f, 1.0f });

	bossTimeOverBeam_ = std::make_unique<Object3d>();
	bossTimeOverBeam_->Initialize(object3dCommon);
	bossTimeOverBeam_->SetModel("beam_enemy.obj");
	bossTimeOverBeam_->SetSelectLightings(6);
	bossTimeOverBeam_->SetEnableLighting(false);
	bossTimeOverBeam_->SetColor({ 1.0f, 0.95f, 0.35f, 1.0f });

	if (spriteCommon)
	{
	float bossBarW = 600.0f;
	float bossBarH = 18.0f;
	float bossBarX = (1280.0f - bossBarW) * 0.5f;
	float bossBarY = 35.0f;

	bossHpBarBgSprite_ = std::make_unique<Sprite>();
	if (bossHpBarBgSprite_)
	{
		bossHpBarBgSprite_->Initialize(spriteCommon, whiteTexIndex);
		bossHpBarBgSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossHpBarBgSprite_->SetPosition(Vector2(bossBarX, bossBarY));
		bossHpBarBgSprite_->SetSize(Vector2(bossBarW, bossBarH));
		bossHpBarBgSprite_->SetColor(Vector4(0.12f, 0.14f, 0.18f, 0.85f));
		bossHpBarBgSprite_->Update();
	}

	bossHpBarDelaySprite_ = std::make_unique<Sprite>();
	if (bossHpBarDelaySprite_)
	{
		bossHpBarDelaySprite_->Initialize(spriteCommon, whiteTexIndex);
		bossHpBarDelaySprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossHpBarDelaySprite_->SetPosition(Vector2(bossBarX, bossBarY));
		bossHpBarDelaySprite_->SetSize(Vector2(bossBarW, bossBarH));
		bossHpBarDelaySprite_->SetColor(Vector4(1.0f, 0.85f, 0.2f, 0.9f));
		bossHpBarDelaySprite_->Update();
	}

	bossHpBarSprite_ = std::make_unique<Sprite>();
	if (bossHpBarSprite_)
	{
		bossHpBarSprite_->Initialize(spriteCommon, whiteTexIndex);
		bossHpBarSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossHpBarSprite_->SetPosition(Vector2(bossBarX, bossBarY));
		bossHpBarSprite_->SetSize(Vector2(bossBarW, bossBarH));
		bossHpBarSprite_->SetColor(Vector4(0.95f, 0.2f, 0.15f, 1.0f));
		bossHpBarSprite_->Update();
	}

	// WARNING演出用スプライト（上下の赤色帯、画面全体フラッシュ）
	bossWarningBandTopSprite_ = std::make_unique<Sprite>();
	if (bossWarningBandTopSprite_)
	{
		bossWarningBandTopSprite_->Initialize(spriteCommon, whiteTexIndex);
		bossWarningBandTopSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossWarningBandTopSprite_->SetPosition(Vector2(0.0f, 0.0f));
		bossWarningBandTopSprite_->SetSize(Vector2(1280.0f, 44.0f));
		bossWarningBandTopSprite_->SetColor(Vector4(0.85f, 0.1f, 0.1f, 0.85f));
		bossWarningBandTopSprite_->Update();
	}

	bossWarningBandBottomSprite_ = std::make_unique<Sprite>();
	if (bossWarningBandBottomSprite_)
	{
		bossWarningBandBottomSprite_->Initialize(spriteCommon, whiteTexIndex);
		bossWarningBandBottomSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossWarningBandBottomSprite_->SetPosition(Vector2(0.0f, 720.0f - 44.0f));
		bossWarningBandBottomSprite_->SetSize(Vector2(1280.0f, 44.0f));
		bossWarningBandBottomSprite_->SetColor(Vector4(0.85f, 0.1f, 0.1f, 0.85f));
		bossWarningBandBottomSprite_->Update();
	}

	bossWarningFlashSprite_ = std::make_unique<Sprite>();
	if (bossWarningFlashSprite_)
	{
		bossWarningFlashSprite_->Initialize(spriteCommon, whiteTexIndex);
		bossWarningFlashSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossWarningFlashSprite_->SetPosition(Vector2(0.0f, 0.0f));
		bossWarningFlashSprite_->SetSize(Vector2(1280.0f, 720.0f));
		bossWarningFlashSprite_->SetColor(Vector4(1.0f, 0.1f, 0.1f, 0.0f));
		bossWarningFlashSprite_->Update();
	}
	}
}

void BossBattleController::Reset()
{
	bossBattleStep_ = Step::NOT_ACTIVE;
	bossWarningTimer_ = 0.0f;
	bossDefeatTimer_ = 0.0f;
	bossDisplayedHpRate_ = 1.0f;
	bossWarningSirenTimer_ = 0.0f;
	bossRemainingTime_ = bossTimeLimit_;
	bossTimeOverTimer_ = 0.0f;
	isBossTimeOverFired_ = false;
}

void BossBattleController::Draw3D()
{
	if (bossBattleStep_ == Step::TIME_OVER_ATTACK)
	{
		if (bossTimeOverChargeSphere_) bossTimeOverChargeSphere_->Draw();
		if (bossTimeOverBeam_) bossTimeOverBeam_->Draw();
	}
}

void BossBattleController::DrawHpSprites(float hpRate)
{
	if (bossBattleStep_ == Step::BATTLE || bossBattleStep_ == Step::DEFEATED_SEQUENCE)
	{
		float barW = 600.0f;
		if (bossHpBarBgSprite_) { bossHpBarBgSprite_->Update(); bossHpBarBgSprite_->Draw(); }
		if (bossHpBarDelaySprite_)
		{
			bossHpBarDelaySprite_->SetSize(Vector2(barW * std::clamp(bossDisplayedHpRate_, 0.0f, 1.0f), 18.0f));
			bossHpBarDelaySprite_->Update();
			bossHpBarDelaySprite_->Draw();
		}
		if (bossHpBarSprite_)
		{
			bossHpBarSprite_->SetSize(Vector2(barW * std::clamp(hpRate, 0.0f, 1.0f), 18.0f));
			bossHpBarSprite_->Update();
			bossHpBarSprite_->Draw();
		}
	}
}

void BossBattleController::TriggerTimeOverCannonSequence(bool nonLethal, const BossBattleContext& ctx)
{
	isTimeOverDebugNonLethal_ = nonLethal;
	(*ctx.isBossSpawned) = true;
	bossBattleStep_ = Step::TIME_OVER_ATTACK;
	bossRemainingTime_ = 0.0f;
	bossTimeOverTimer_ = 0.0f;
	isBossTimeOverFired_ = false;
	(*ctx.cameraShakeTimer) = 0.8f;

	if (ctx.player)
	{
		ctx.player->SetDead(false);
		ctx.player->SetHp(ctx.player->GetMaxHp());
		ctx.player->SetEnableInput(false); // タイムオーバー時：移動キーを完全無効化し拘束！
	}

	if (ctx.armoredTrainBoss)
	{
		if (!ctx.armoredTrainBoss->IsActive())
		{
			if (ctx.bossRail && ctx.bossRail->IsValid())
			{
				ctx.armoredTrainBoss->SetRail(ctx.bossRail);
				ctx.armoredTrainBoss->SetRailProgress(0.0f);
			}
			else if (ctx.railCameraController && !(*ctx.mainRails).empty())
			{
				float bCurProg = ctx.railCameraController->GetProgress();
				float bossProg = (std::min)(1.0f, bCurProg + 0.15f);
				ctx.armoredTrainBoss->SetRailProgress(bossProg);
				ctx.armoredTrainBoss->SetRail((*ctx.mainRails)[0].get());
			}
			ctx.armoredTrainBoss->SetActive(true);
		}
	}
}


void BossBattleController::StartWarningSequence(const BossBattleContext& ctx)
{
	if (bossBattleStep_ != Step::NOT_ACTIVE) return;

	bossBattleStep_ = Step::WARNING_ALERT;
	bossWarningTimer_ = 0.0f;
	bossWarningSirenTimer_ = 0.0f;
	bossRemainingTime_ = bossTimeLimit_;
	bossTimeOverTimer_ = 0.0f;
	isBossTimeOverFired_ = false;
	if (ctx.railCameraController)
	{
		ctx.railCameraController->SetBaseSpeed(50.0f);
	}
	(*ctx.cameraShakeTimer) = 0.6f;


	// ボスをレール上の前方に出現スタンバイ
	if (ctx.armoredTrainBoss)
	{
		ctx.armoredTrainBoss->ResetChaseState();
		if (ctx.bossRail && ctx.bossRail->IsValid())
		{
			// ボス専用レール（BossRail）を走行
			ctx.armoredTrainBoss->SetRail(ctx.bossRail);
			ctx.armoredTrainBoss->SetRailProgress(0.0f);
		}
		else if (ctx.railCameraController && !(*ctx.mainRails).empty())
		{
			// メインレールを前方追走
			float curProg = ctx.railCameraController->GetProgress();
			float bossProg = (std::min)(0.98f, curProg + 0.12f);
			ctx.armoredTrainBoss->SetRailProgress(bossProg);
			ctx.armoredTrainBoss->SetRail((*ctx.mainRails)[0].get());
		}
	}
}


void BossBattleController::Update(float dt, const BossBattleContext& ctx)
{
	// 電脳処刑フィニッシャーが進行中の場合、最優先でフィニッシャーに処理を委譲
	if (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsActive())
	{
		(*ctx.isBossSpawned) = true;
		if (bossBattleStep_ == Step::NOT_ACTIVE)
		{
			bossBattleStep_ = Step::BATTLE;
		}
		ctx.bossFinisherSequence->Update(dt, ctx.armoredTrainBoss, ctx.player, (*ctx.gameSpeed));
		if (ctx.bossFinisherSequence->IsCompleted())
		{
			bossBattleStep_ = Step::FINISHED;
			if (ctx.onClear) ctx.onClear();
		}
		return;
	}

	if (bossBattleStep_ == Step::NOT_ACTIVE) return;

	if (bossBattleStep_ == Step::WARNING_ALERT)
	{
		bossWarningTimer_ += dt;
		bossWarningSirenTimer_ += dt;

		// 0.85秒ごとにサイレンSEを反復再生
		if (bossWarningSirenTimer_ >= 0.85f && bossWarningTimer_ < kBossWarningDuration - 0.4f)
		{
			bossWarningSirenTimer_ = 0.0f;
			SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.85f);
		}

		// 警告時間終了 -> ボス戦開始！
		if (bossWarningTimer_ >= kBossWarningDuration)
		{
			bossBattleStep_ = Step::BATTLE;
			(*ctx.isBossSpawned) = true;
			bossRemainingTime_ = bossTimeLimit_;
			bossTimeOverTimer_ = 0.0f;
			isBossTimeOverFired_ = false;
			if (ctx.railCameraController)
			{
				ctx.railCameraController->SetBaseSpeed(50.0f);
			}
			if (ctx.armoredTrainBoss)
			{
				auto services = EngineServices::GetInstance();
				if (services && services->IsDemoMode() && ctx.player)
				{
					Vector3 pPos = ctx.player->GetWorldPosition();
					Rail* targetRail = (ctx.bossRail && ctx.bossRail->IsValid()) ? ctx.bossRail : (!(*ctx.mainRails).empty() ? (*ctx.mainRails)[0].get() : nullptr);
					if (targetRail && targetRail->IsValid())
					{
						float closestProg = targetRail->GetClosestProgress(pPos);
						Vector3 railClosestPos = targetRail->GetPosition(closestProg);
						float dx = railClosestPos.x - pPos.x;
						float dy = railClosestPos.y - pPos.y;
						float dz = railClosestPos.z - pPos.z;
						float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
						// ctx.bossRail がプレイヤーから遠すぎる場合はメインレール上に配置して即座に視界へ出す
						if (dist > 150.0f && !(*ctx.mainRails).empty() && (*ctx.mainRails)[0] && (*ctx.mainRails)[0]->IsValid())
						{
							targetRail = (*ctx.mainRails)[0].get();
							closestProg = targetRail->GetClosestProgress(pPos);
						}
						float totalLen = (std::max)(1.0f, targetRail->GetTotalLength());
						// 先頭車両がプレイヤーの前方約90m（最後尾が横〜後方）になる位置から一気に追い抜いて登場させる
						float spawnProg = (std::clamp)(closestProg + (90.0f / totalLen), 0.0f, 0.95f);
						ctx.armoredTrainBoss->SetRail(targetRail);
						ctx.armoredTrainBoss->SetRailProgress(spawnProg);
					}
				}
				ctx.armoredTrainBoss->SetActive(true);
			}
			SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.9f);
			(*ctx.cameraShakeTimer) = 0.5f;
		}
	}
	else if (bossBattleStep_ == Step::BATTLE)
	{
		// 1. レール終端接近チェック（0.98f到達で進行不能となるのを完全防止）
		bool reachRailEnd = false;
		if (ctx.railCameraController && ctx.railCameraController->GetProgress() >= 0.98f)
		{
			reachRailEnd = true;
		}

		// 2. ボス戦制限時間のカウントダウン & ボス戦経過時間計測
		bossRemainingTime_ -= dt;
		(*ctx.bossElapsedTime) += dt;

		// 残り10秒以下でのサイレン警告演出
		if (bossRemainingTime_ <= 10.0f && bossRemainingTime_ > 0.0f)
		{
			static float lowTimeAlarmTimer = 0.0f;
			lowTimeAlarmTimer += dt;
			if (lowTimeAlarmTimer >= 1.0f)
			{
				lowTimeAlarmTimer = 0.0f;
				SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.65f);
			}
		}

		// 3. タイムオーバー判定（時間切れ または レール終端到達 -> 即死殲滅攻撃シークエンス発動！）
		if (bossRemainingTime_ <= 0.0f || reachRailEnd)
		{
			bossRemainingTime_ = 0.0f;
			bossBattleStep_ = Step::TIME_OVER_ATTACK;
			bossTimeOverTimer_ = 0.0f;
			isBossTimeOverFired_ = false;
			(*ctx.cameraShakeTimer) = 0.8f;
			SoundManager::GetInstance()->PlaySE("Alarm01.wav", 1.0f);
		}

		if (ctx.armoredTrainBoss)
		{
			// HPゲージのスムーズ遅延追従（格闘ゲーム風のダメージ可視化演出）
			float currentHpRate = ctx.armoredTrainBoss->GetTotalHpRate();
			if (bossDisplayedHpRate_ > currentHpRate)
			{
				bossDisplayedHpRate_ -= 0.35f * dt;
				if (bossDisplayedHpRate_ < currentHpRate)
				{
					bossDisplayedHpRate_ = currentHpRate;
				}
			}
			else
			{
				bossDisplayedHpRate_ = currentHpRate;
			}

			// ★ 電脳処刑フィニッシャーへの自動移行（HP1耐え：総HP率が5%以下または撃破判定時）
			if ((currentHpRate <= 0.05f || ctx.armoredTrainBoss->IsDefeated()) &&
				ctx.bossFinisherSequence && !ctx.bossFinisherSequence->IsActive() && !ctx.bossFinisherSequence->IsCompleted())
			{
				(*ctx.bullets).clear();
				(*ctx.missiles).clear();
				ctx.bossFinisherSequence->StartSequence(ctx.armoredTrainBoss, ctx.player);
			}
		}
	}
	else if (bossBattleStep_ == Step::TIME_OVER_ATTACK)
	{
		bossTimeOverTimer_ += dt;

		// プレイヤーの移動操作入力を完全遮断し拘束
		if (ctx.player)
		{
			ctx.player->SetEnableInput(false);
		}

		// 自機の正真正銘のワールド座標を取得（カメラローカル座標ではなくGetWorldPosition）
		Vector3 playerPos = ctx.player ? ctx.player->GetWorldPosition() : Vector3(0.0f, 5.0f, 0.0f);

		// 1. 3両目の超巨大主砲をプレイヤーへ正確に旋回・照準
		if (ctx.armoredTrainBoss)
		{
			ctx.armoredTrainBoss->UpdateTimeOverCannon(playerPos, bossTimeOverTimer_, dt);
		}

		// 砲口（マズル）のワールド位置を取得
		Vector3 muzzlePos = ctx.armoredTrainBoss ? ctx.armoredTrainBoss->GetBigCannonMuzzlePosition() : Vector3(0.0f, 15.0f, -60.0f);

		// 2. 砲撃照準ロックオン演出（0.0s 〜 2.2s：タイムオーバーUI表示〜消滅中）
		if (bossTimeOverTimer_ < 2.2f)
		{
			float chargeRatio = std::clamp(bossTimeOverTimer_ / 2.2f, 0.0f, 1.0f);
			(*ctx.cameraShakeTimer) = 0.20f + 0.50f * chargeRatio;

			// チャージ球の収縮・脈動は不要なため非表示に設定
			if (bossTimeOverChargeSphere_)
			{
				bossTimeOverChargeSphere_->SetScale({ 0.0f, 0.0f, 0.0f });
				bossTimeOverChargeSphere_->Update();
			}

			// 照準レーザー線（UI表示中、砲口からプレイヤーへ細いロックオンレーザーを照射）
			if (bossTimeOverBeam_)
			{
				Vector3 aimVec = playerPos - muzzlePos;
				float aimDist = aimVec.Length();
				if (aimDist > 0.01f)
				{
					Vector3 aimDir = aimVec.Normalize();
					Vector3 aimCenter = muzzlePos + aimDir * (aimDist * 0.5f);
					float rotY = std::atan2(aimDir.x, aimDir.z);
					float xzLen = std::sqrt(aimDir.x * aimDir.x + aimDir.z * aimDir.z);
					float rotX = std::atan2(-aimDir.y, (std::max)(xzLen, 0.001f));

					// 発射直前（UI消滅時）に向けて徐々に赤熱化
					float aimWidth = 0.45f + 0.25f * chargeRatio;
					bossTimeOverBeam_->SetTranslate(aimCenter);
					bossTimeOverBeam_->SetRotation({ rotX, rotY, 0.0f });
					bossTimeOverBeam_->SetScale({ aimWidth, aimWidth, aimDist / 2.2f });
					bossTimeOverBeam_->SetColor({ 1.0f, 0.2f * (1.0f - chargeRatio), 0.2f * (1.0f - chargeRatio), 0.85f });
					bossTimeOverBeam_->Update();
				}
			}
		}
		else
		{
			// 3. タイムオーバーUIが完全に消滅してから極太主砲発射！（2.2s 〜 3.4s）
			if (!isBossTimeOverFired_)
			{
				isBossTimeOverFired_ = true;
				(*ctx.cameraShakeTimer) = 3.0f; // 画面超大激震

				// プレイヤーHPをゼロにして死亡
				if (ctx.player)
				{
					ctx.player->SetHp(0);
				}

				// プレイヤー位置に特大爆発エフェクトを発生
				(*ctx.explosionEffect).SetPosition(playerPos);
				(*ctx.explosionEffect).Play();
			}

			// 極大主砲ビームの接続描画（砲口からプレイヤーを完全に貫通・直撃）
			Vector3 beamVec = playerPos - muzzlePos;
			float distToPlayer = beamVec.Length();
			if (distToPlayer > 0.01f)
			{
				Vector3 beamDir = beamVec.Normalize();
				float totalBeamLen = distToPlayer + 80.0f; // プレイヤーを大きく貫通
				Vector3 beamCenter = muzzlePos + beamDir * (totalBeamLen * 0.5f);

				float rotY = std::atan2(beamDir.x, beamDir.z);
				float xzLen = std::sqrt(beamDir.x * beamDir.x + beamDir.z * beamDir.z);
				float rotX = std::atan2(-beamDir.y, (std::max)(xzLen, 0.001f));

				float beamFade = std::clamp((3.4f - bossTimeOverTimer_) / 1.2f, 0.0f, 1.0f);
				float beamThickness = (15.0f + 2.0f * std::sin(bossTimeOverTimer_ * 50.0f)) * beamFade; // 直径15m以上の超極太レールガン光柱

				if (bossTimeOverBeam_)
				{
					bossTimeOverBeam_->SetTranslate(beamCenter);
					bossTimeOverBeam_->SetRotation({ rotX, rotY, 0.0f });
					bossTimeOverBeam_->SetScale({ beamThickness, beamThickness, totalBeamLen / 2.2f });
					bossTimeOverBeam_->SetColor({ 1.0f, 0.95f, 0.35f, beamFade });
					bossTimeOverBeam_->Update();
				}

				// 発射時：プレイヤー位置に直撃している巨大エネルギー球頭
				if (bossTimeOverChargeSphere_)
				{
					float hitSphereSize = 18.0f * beamFade;
					bossTimeOverChargeSphere_->SetTranslate(playerPos);
					bossTimeOverChargeSphere_->SetScale({ hitSphereSize, hitSphereSize, hitSphereSize });
					bossTimeOverChargeSphere_->SetColor({ 1.0f, 0.98f, 0.6f, beamFade });
					bossTimeOverChargeSphere_->Update();
				}
			}

			// プレイヤー位置で連続大爆発（ビーム照射中）
			static float timeOverExpSubTimer = 0.0f;
			timeOverExpSubTimer += dt;
			if (timeOverExpSubTimer >= 0.08f && bossTimeOverTimer_ < 3.4f)
			{
				timeOverExpSubTimer = 0.0f;
				Vector3 jitter = {
					(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 9.0f,
					(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 9.0f,
					(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 9.0f
				};
				(*ctx.explosionEffect).SetPosition(playerPos + jitter);
				(*ctx.explosionEffect).Play();
			}
		}
		if (bossTimeOverTimer_ >= kBossTimeOverDuration)
		{
			if (isTimeOverDebugLoop_)
			{
				// ループデバッグ：再度最初から即座にリプレイ
				bossTimeOverTimer_ = 0.0f;
				isBossTimeOverFired_ = false;
				(*ctx.cameraShakeTimer) = 0.8f;
				if (ctx.player)
				{
					ctx.player->SetDead(false);
					ctx.player->SetHp(ctx.player->GetMaxHp());
					ctx.player->SetEnableInput(false);
				}
			}
			else if (isTimeOverDebugNonLethal_)
			{
				// 非致死デバッグ：演出完了後、プレイヤーの操作を復帰させて通常戦闘へ戻す（何度でも再確認可能）
				bossBattleStep_ = Step::BATTLE;
				bossRemainingTime_ = 60.0f;
				bossTimeOverTimer_ = 0.0f;
				isBossTimeOverFired_ = false;
				if (ctx.player)
				{
					ctx.player->SetDead(false);
					ctx.player->SetHp(ctx.player->GetMaxHp());
					ctx.player->SetEnableInput(true);
				}
			}
			else
			{
				bossBattleStep_ = Step::FINISHED;
				if (ctx.player)
				{
					ctx.player->SetHp(0);
				}
				if (ctx.onGameOver)
				{
					if (ctx.onGameOver) ctx.onGameOver();
				}
			}
		}
	}
	else if (bossBattleStep_ == Step::DEFEATED_SEQUENCE)
	{
		// 繝懊せ謦・ｴ譎ゅ・繝ｬ繝ｼ繝ｫ騾溷ｺｦ繧帝壼ｸｸ騾溷ｺｦ縺ｸ蠕ｩ蟶ｰ
		if (ctx.railCameraController)
		{
			ctx.railCameraController->SetBaseSpeed(50.0f);
		}

		bossDefeatTimer_ += dt;

		// 連続大爆発演出（0.12秒ごと）
		static float defeatExplodeSubTimer = 0.0f;
		defeatExplodeSubTimer += dt;
		if (defeatExplodeSubTimer >= 0.12f && ctx.armoredTrainBoss)
		{
			defeatExplodeSubTimer = 0.0f;
			const auto& cars = ctx.armoredTrainBoss->GetCarriages();
			if (!cars.empty())
			{
				int randCar = rand() % cars.size();
				if (cars[randCar].object)
				{
					Vector3 explodePos = cars[randCar].object->GetTranslate();
					explodePos.x += ((rand() % 100) / 50.0f - 1.0f) * 4.0f;
					explodePos.y += ((rand() % 100) / 50.0f - 1.0f) * 2.0f;
					explodePos.z += ((rand() % 100) / 50.0f - 1.0f) * 6.0f;
					(*ctx.explosionEffect).SetPosition(explodePos);
					(*ctx.explosionEffect).Play();
					SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.85f);
					(*ctx.cameraShakeTimer) = 0.25f;
				}
			}
		}

		// 撃破演出終了 -> ゲームクリア画面へ移行！
		if (bossDefeatTimer_ >= kBossDefeatDuration)
		{
			bossBattleStep_ = Step::FINISHED;
			(*ctx.isBossSpawned) = false;
			if (ctx.onClear) ctx.onClear();
		}
	}
}


void BossBattleController::DrawUI(bool isStartCutscene, bool isGameOver, bool isClear, const BossBattleContext& ctx)
{
#ifdef USE_IMGUI
	// 処刑ミサイル発射後のムービー演出中はボスHPゲージ等の戦闘UIをすべて非表示
	if (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsMoviePhase()) return;

	if (ImGui::GetCurrentContext() == nullptr) return;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) return;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	if (!drawList) return;

	// ゲーム画面領域（エディタモード／フルスクリーン対応）
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
	float scale = screenH / 720.0f;
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
	auto textMgr = UITextManager::GetInstance();
	ImFont* fontMplus = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::Japanese_MPLUS) : ImGui::GetFont();
	if (!fontMplus) fontMplus = ImGui::GetFont();

	// 1. WARNING警告演出（赤フラッシュ、警告ストライプ、点滅テキスト）
	if (bossBattleStep_ == Step::WARNING_ALERT)
	{
		float warnT = bossWarningTimer_;
		float pulse = 0.5f + 0.5f * std::sin(warnT * 12.0f); // 高速明滅パルス

		// 画面全体の赤色フラッシュ
		int flashAlpha = static_cast<int>(50.0f + 60.0f * pulse);
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(255, 20, 20, flashAlpha));

		// 上下の警告ストライプバー（高さ 44px）
		float barH = 44.0f * scale;
		// 上の警告帯
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barH), IM_COL32(200, 20, 20, 230));
		// 下の警告帯
		drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barH), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(200, 20, 20, 230));

		// 警告帯の斜線ストライプパターン
		float stripeW = 30.0f * scale;
		float stripeOffset = std::fmod(warnT * 80.0f * scale, stripeW * 2.0f);
		for (float x = -stripeW * 2.0f; x < screenW + stripeW * 2.0f; x += stripeW * 2.0f)
		{
			// 上部ストライプ
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x + stripeOffset, viewPos.y),
				ImVec2(viewPos.x + x + stripeOffset + stripeW * 0.6f, viewPos.y),
				ImVec2(viewPos.x + x + stripeOffset - stripeW * 0.4f, viewPos.y + barH),
				IM_COL32(20, 20, 20, 220)
			);
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x + stripeOffset + stripeW * 0.6f, viewPos.y),
				ImVec2(viewPos.x + x + stripeOffset + stripeW * 0.2f, viewPos.y + barH),
				ImVec2(viewPos.x + x + stripeOffset - stripeW * 0.4f, viewPos.y + barH),
				IM_COL32(20, 20, 20, 220)
			);
			// 下部ストライプ
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x - stripeOffset, viewPos.y + screenH - barH),
				ImVec2(viewPos.x + x - stripeOffset + stripeW * 0.6f, viewPos.y + screenH - barH),
				ImVec2(viewPos.x + x - stripeOffset - stripeW * 0.4f, viewPos.y + screenH),
				IM_COL32(20, 20, 20, 220)
			);
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x - stripeOffset + stripeW * 0.6f, viewPos.y + screenH - barH),
				ImVec2(viewPos.x + x - stripeOffset + stripeW * 0.2f, viewPos.y + screenH),
				ImVec2(viewPos.x + x - stripeOffset - stripeW * 0.4f, viewPos.y + screenH),
				IM_COL32(20, 20, 20, 220)
			);
		}

		// 中央の「WARNING」テキスト描画（JSON設定連動）
		const UITextItem* warnItem = textMgr ? textMgr->GetTextItem("BossWarningTitle", "GAMEPLAY") : nullptr;
		const char* warnTitle = (warnItem && !warnItem->text.empty()) ? warnItem->text.c_str() : "WARNING";
		float titleFontSize = (warnItem ? warnItem->fontSize : 58.0f) * scale + (6.0f * pulse * scale);
		ImFont* warnFont = (warnItem && imguiManager) ? imguiManager->GetFont(warnItem->fontType) : fontMplus;
		if (!warnFont) warnFont = fontMplus;

		ImVec2 titleSize = warnFont->CalcTextSizeA(titleFontSize, FLT_MAX, -1.0f, warnTitle);
		float warnX = viewPos.x + (screenW - titleSize.x) * 0.5f;
		float warnY = viewPos.y + screenH * 0.40f - titleSize.y * 0.5f;
		if (warnItem)
		{
			warnX = viewPos.x + warnItem->position.x * (screenW / 1280.0f) - titleSize.x * 0.5f;
			warnY = viewPos.y + warnItem->position.y * (screenH / 720.0f) - titleSize.y * 0.5f;
		}
		ImVec2 titlePos(warnX, warnY);

		// テキストシャドウ・アウトライン
		float thick = 3.0f * scale;
		for (float ox = -thick; ox <= thick; ox += thick)
		{
			for (float oy = -thick; oy <= thick; oy += thick)
			{
				drawList->AddText(warnFont, titleFontSize, ImVec2(titlePos.x + ox, titlePos.y + oy), IM_COL32(0, 0, 0, 255), warnTitle);
			}
		}
				ImU32 warnCol = warnItem ? (ImU32)ImColor(warnItem->color.x, warnItem->color.y, warnItem->color.z, warnItem->color.w * (0.65f + 0.35f * pulse)) : (pulse > 0.3f ? IM_COL32(255, 40, 40, 255) : IM_COL32(255, 220, 50, 255));
		if (warnItem && warnItem->hasShadow)
		{
			ImU32 sCol = (ImU32)ImColor(warnItem->shadowColor.x, warnItem->shadowColor.y, warnItem->shadowColor.z, warnItem->shadowColor.w);
			drawList->AddText(warnFont, titleFontSize, ImVec2(titlePos.x + warnItem->shadowOffset.x * scale, titlePos.y + warnItem->shadowOffset.y * scale), sCol, warnTitle);
		}
		drawList->AddText(warnFont, titleFontSize, titlePos, warnCol, warnTitle);

		// サブテキスト「EMERGENCY: MASSIVE ARMORED TRAIN DETECTED」（JSON設定連動）
		const UITextItem* subItem = textMgr ? textMgr->GetTextItem("BossWarningSub", "GAMEPLAY") : nullptr;
		const char* subText = (subItem && !subItem->text.empty()) ? subItem->text.c_str() : "EMERGENCY: MASSIVE ARMORED TRAIN DETECTED";
		float subFontSize = (subItem ? subItem->fontSize : 22.0f) * scale;
		ImFont* subFont = (subItem && imguiManager) ? imguiManager->GetFont(subItem->fontType) : fontMplus;
		if (!subFont) subFont = fontMplus;

		ImVec2 subSize = subFont->CalcTextSizeA(subFontSize, FLT_MAX, -1.0f, subText);
		float subX = viewPos.x + (screenW - subSize.x) * 0.5f;
		float subY = titlePos.y + titleSize.y + 12.0f * scale;
		if (subItem)
		{
			subX = viewPos.x + subItem->position.x * (screenW / 1280.0f) - subSize.x * 0.5f;
			subY = viewPos.y + subItem->position.y * (screenH / 720.0f);
		}
		ImVec2 subPos(subX, subY);

		for (float ox = -2.0f; ox <= 2.0f; ox += 2.0f)
		{
			for (float oy = -2.0f; oy <= 2.0f; oy += 2.0f)
			{
				drawList->AddText(subFont, subFontSize, ImVec2(subPos.x + ox, subPos.y + oy), IM_COL32(0, 0, 0, 220), subText);
			}
		}
				if (subItem && subItem->hasShadow)
		{
			ImU32 sCol = (ImU32)ImColor(subItem->shadowColor.x, subItem->shadowColor.y, subItem->shadowColor.z, subItem->shadowColor.w);
			drawList->AddText(subFont, subFontSize, ImVec2(subPos.x + subItem->shadowOffset.x * scale, subPos.y + subItem->shadowOffset.y * scale), sCol, subText);
		}
		ImU32 subCol = subItem ? (ImU32)ImColor(subItem->color.x, subItem->color.y, subItem->color.z, subItem->color.w) : IM_COL32(255, 240, 240, 240);
		drawList->AddText(subFont, subFontSize, subPos, subCol, subText);
	}

	// 2. ボスHPゲージUI（戦闘中・撃破演出中）
	if ((bossBattleStep_ == Step::BATTLE || bossBattleStep_ == Step::TIME_OVER_ATTACK || bossBattleStep_ == Step::DEFEATED_SEQUENCE) && ctx.armoredTrainBoss)
	{
		float barW = 600.0f * scale;
		float barH = 18.0f * scale;
		float barX = viewPos.x + (screenW - barW) * 0.5f;
		float barY = viewPos.y + 36.0f * scale;

		float realHpRate = (std::clamp)(ctx.armoredTrainBoss->GetTotalHpRate(), 0.0f, 1.0f);
		float delayHpRate = (std::clamp)(bossDisplayedHpRate_, 0.0f, 1.0f);

		// ボス名タイトル（JSON設定連動、デフォルト: BOSS: ARMORED TRAIN "BEHEMOTH"）
		const UITextItem* titleItem = textMgr ? textMgr->GetTextItem("BossTitle", "GAMEPLAY") : nullptr;
		const char* bossTitle = (titleItem && !titleItem->text.empty()) ? titleItem->text.c_str() : "BOSS: ARMORED TRAIN \"BEHEMOTH\"";
		float bossTitleFontSize = (titleItem ? titleItem->fontSize : 20.0f) * scale;
		ImFont* titleFont = (titleItem && imguiManager) ? imguiManager->GetFont(titleItem->fontType) : fontMplus;
		if (!titleFont) titleFont = fontMplus;

		ImVec2 bossTitlePos(barX, barY - 26.0f * scale);
		if (titleItem)
		{
			bossTitlePos.x = viewPos.x + titleItem->position.x * (screenW / 1280.0f);
			bossTitlePos.y = viewPos.y + titleItem->position.y * (screenH / 720.0f);
		}

		// 黒アウトライン
		drawList->AddText(titleFont, bossTitleFontSize, ImVec2(bossTitlePos.x + 1.5f, bossTitlePos.y + 1.5f), IM_COL32(0, 0, 0, 255), bossTitle);
		drawList->AddText(titleFont, bossTitleFontSize, ImVec2(bossTitlePos.x - 1.5f, bossTitlePos.y - 1.5f), IM_COL32(0, 0, 0, 255), bossTitle);
		ImU32 titleCol = titleItem ? (ImU32)ImColor(titleItem->color.x, titleItem->color.y, titleItem->color.z, titleItem->color.w) : IM_COL32(255, 90, 80, 255);
		drawList->AddText(titleFont, bossTitleFontSize, bossTitlePos, titleCol, bossTitle);

		// HPパーセント表示（右揃え）
		char hpPercentBuf[32];
		snprintf(hpPercentBuf, sizeof(hpPercentBuf), "HP %3.0f%%", realHpRate * 100.0f);
		ImVec2 hpTxtSize = titleFont->CalcTextSizeA(bossTitleFontSize, FLT_MAX, -1.0f, hpPercentBuf);
		ImVec2 hpTxtPos(barX + barW - hpTxtSize.x, barY - 26.0f * scale);
		drawList->AddText(titleFont, bossTitleFontSize, ImVec2(hpTxtPos.x + 1.5f, hpTxtPos.y + 1.5f), IM_COL32(0, 0, 0, 255), hpPercentBuf);
		drawList->AddText(titleFont, bossTitleFontSize, hpTxtPos, IM_COL32(255, 230, 200, 255), hpPercentBuf);

		// 外枠シャドウ
		drawList->AddRectFilled(ImVec2(barX - 3.0f, barY - 3.0f), ImVec2(barX + barW + 3.0f, barY + barH + 3.0f), IM_COL32(0, 0, 0, 200), 3.0f);
		// 背景
		drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(30, 32, 38, 240), 2.0f);

		// 遅延追従バー（黄色）
		if (delayHpRate > 0.001f)
		{
			drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * delayHpRate, barY + barH), IM_COL32(255, 200, 60, 220), 2.0f);
		}

		// メインHPバー（深紅〜赤グラデーション）
		if (realHpRate > 0.001f)
		{
			drawList->AddRectFilledMultiColor(
				ImVec2(barX, barY),
				ImVec2(barX + barW * realHpRate, barY + barH),
				IM_COL32(255, 50, 40, 255),
				IM_COL32(210, 20, 20, 255),
				IM_COL32(180, 10, 10, 255),
				IM_COL32(230, 40, 30, 255)
			);
		}

		// バー枠線
		drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(180, 180, 190, 220), 2.0f, 0, 1.5f);

		// 3. 各車両部位破壊インジケーター（4車両）
		const auto& cars = ctx.armoredTrainBoss->GetCarriages();
		if (!cars.empty())
		{
			float indGap = 8.0f * scale;
			float indW = (barW - indGap * (cars.size() - 1)) / static_cast<float>(cars.size());
			float indH = 18.0f * scale;
			float indY = barY + barH + 6.0f * scale;

			for (size_t c = 0; c < cars.size(); ++c)
			{
				float cx = barX + c * (indW + indGap);
				bool destroyed = cars[c].isDestroyed;

				// 各車両枠背景
				ImU32 bgCol = destroyed ? IM_COL32(40, 20, 20, 180) : IM_COL32(25, 40, 50, 200);
				drawList->AddRectFilled(ImVec2(cx, indY), ImVec2(cx + indW, indY + indH), bgCol, 2.0f);

				// 車両個別HP率
				float carHpRate = cars[c].maxHp > 0 ? static_cast<float>(cars[c].hp) / cars[c].maxHp : 0.0f;
				if (!destroyed && carHpRate > 0.001f)
				{
					ImU32 carBarCol = (c == 0) ? IM_COL32(255, 120, 40, 200) : IM_COL32(50, 190, 220, 200);
					drawList->AddRectFilled(ImVec2(cx, indY), ImVec2(cx + indW * carHpRate, indY + indH), carBarCol, 2.0f);
				}

				// 車両枠線
				ImU32 borderCol = destroyed ? IM_COL32(150, 40, 40, 150) : IM_COL32(100, 180, 200, 200);
				drawList->AddRect(ImVec2(cx, indY), ImVec2(cx + indW, indY + indH), borderCol, 2.0f);

				// 車両名称略称表示
				const char* carShortName = "";
				if (c == 0) carShortName = "1:CORE 機関車";
				else if (c == 1) carShortName = "2:重砲塔車";
				else if (c == 2) carShortName = "3:ミサイル車";
				else if (c == 3) carShortName = "4:動力発電車";

				float cFontSize = 11.0f * scale;
				ImVec2 cTxtSize = fontMplus->CalcTextSizeA(cFontSize, FLT_MAX, -1.0f, carShortName);
				ImVec2 cTxtPos(cx + (indW - cTxtSize.x) * 0.5f, indY + (indH - cTxtSize.y) * 0.5f);

				if (destroyed)
				{
					const char* destText = "DESTROYED";
					ImVec2 dSize = fontMplus->CalcTextSizeA(cFontSize, FLT_MAX, -1.0f, destText);
					ImVec2 dPos(cx + (indW - dSize.x) * 0.5f, indY + (indH - dSize.y) * 0.5f);
					drawList->AddText(fontMplus, cFontSize, dPos, IM_COL32(255, 60, 60, 220), destText);
				}
				else
				{
					drawList->AddText(fontMplus, cFontSize, ImVec2(cTxtPos.x + 1.0f, cTxtPos.y + 1.0f), IM_COL32(0, 0, 0, 220), carShortName);
					drawList->AddText(fontMplus, cFontSize, cTxtPos, IM_COL32(230, 240, 250, 240), carShortName);
				}
			}
		}

		// 4. ボス戦制限時間タイマー表示（戦闘中・タイムオーバー時：JSON設定連動 ＆ 超大型ハイビジビリティ表示！）
		if (bossBattleStep_ == Step::BATTLE || bossBattleStep_ == Step::TIME_OVER_ATTACK)
		{
			const UITextItem* timerItem = textMgr ? textMgr->GetTextItem("BossTimer", "GAMEPLAY") : nullptr;

			int minutes = static_cast<int>(bossRemainingTime_) / 60;
			float seconds = std::fmod(bossRemainingTime_, 60.0f);
			if (seconds < 0.0f) seconds = 0.0f;

			char timerDigits[32];
			snprintf(timerDigits, sizeof(timerDigits), "%02d:%04.1f", minutes, seconds);

			std::string label = (timerItem && !timerItem->text.empty()) ? timerItem->text : "TIME LIMIT";
			std::string fullTimerStr = label + "  " + timerDigits;

			// フォントサイズは設定ファイルから取得（デフォルト 28px で大きく見やすい！）
			float tFontSize = (timerItem ? timerItem->fontSize : 28.0f) * scale;
			ImFont* timerFont = (timerItem && imguiManager) ? imguiManager->GetFont(timerItem->fontType) : (imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::English_FiraMono) : fontMplus);
			if (!timerFont) timerFont = fontMplus;

			ImVec2 tSize = timerFont->CalcTextSizeA(tFontSize, FLT_MAX, -1.0f, fullTimerStr.c_str());

			float timeY = barY + barH + 34.0f * scale;
			float timeX = barX + (barW - tSize.x) * 0.5f;
			if (timerItem)
			{
				timeX = viewPos.x + timerItem->position.x * (screenW / 1280.0f) - tSize.x * 0.5f;
				timeY = viewPos.y + timerItem->position.y * (screenH / 720.0f);
			}
			ImVec2 tPos(timeX, timeY);

			// 色の決定（通常: 設定ファイル色 or シアン、20秒以下: オレンジ、10秒以下: 赤点滅パルス）
			ImU32 timerCol = timerItem ? (ImU32)ImColor(timerItem->color.x, timerItem->color.y, timerItem->color.z, timerItem->color.w) : IM_COL32(0, 235, 215, 240);
			if (bossRemainingTime_ <= 10.0f)
			{
				float pulse = 0.5f + 0.5f * std::sin(bossTimeOverTimer_ * 16.0f + bossRemainingTime_ * 12.0f);
				timerCol = (pulse > 0.4f) ? IM_COL32(255, 30, 30, 255) : IM_COL32(255, 200, 40, 200);
			}
			else if (bossRemainingTime_ <= 20.0f)
			{
				timerCol = IM_COL32(255, 160, 40, 255);
			}

			// タイマー背景ピル枠（フォント拡大に合わせてパディングも大きく）
			float pillPadX = 22.0f * scale;
			float pillPadY = 6.0f * scale;
			drawList->AddRectFilled(
				ImVec2(tPos.x - pillPadX, tPos.y - pillPadY),
				ImVec2(tPos.x + tSize.x + pillPadX, tPos.y + tSize.y + pillPadY),
				IM_COL32(10, 16, 22, 225), 6.0f * scale
			);
			drawList->AddRect(
				ImVec2(tPos.x - pillPadX, tPos.y - pillPadY),
				ImVec2(tPos.x + tSize.x + pillPadX, tPos.y + tSize.y + pillPadY),
				timerCol, 6.0f * scale, 0, 1.8f * scale
			);

			// タイマーアウトライン（黒極太で高コントラスト）
			float thick = 2.4f * scale;
			for (float ox = -thick; ox <= thick; ox += thick)
			{
				for (float oy = -thick; oy <= thick; oy += thick)
				{
					drawList->AddText(timerFont, tFontSize, ImVec2(tPos.x + ox, tPos.y + oy), IM_COL32(0, 0, 0, 255), fullTimerStr.c_str());
				}
			}
			drawList->AddText(timerFont, tFontSize, tPos, timerCol, fullTimerStr.c_str());
		}

		// 6. 撃破演出中の「TARGET DESTROYED」テキスト表示（JSON設定連動）
		if (bossBattleStep_ == Step::DEFEATED_SEQUENCE)
		{
			const UITextItem* defItem = textMgr ? textMgr->GetTextItem("BossDefeatedTitle", "GAMEPLAY") : nullptr;
			const char* clearText = (defItem && !defItem->text.empty()) ? defItem->text.c_str() : "TARGET DESTROYED";
			float dFontSize = (defItem ? defItem->fontSize : 48.0f) * scale;
			ImFont* defFont = (defItem && imguiManager) ? imguiManager->GetFont(defItem->fontType) : fontMplus;
			if (!defFont) defFont = fontMplus;

			ImVec2 dSize = defFont->CalcTextSizeA(dFontSize, FLT_MAX, -1.0f, clearText);
			float dX = viewPos.x + (screenW - dSize.x) * 0.5f;
			float dY = viewPos.y + screenH * 0.42f;
			if (defItem)
			{
				dX = viewPos.x + defItem->position.x * (screenW / 1280.0f) - dSize.x * 0.5f;
				dY = viewPos.y + defItem->position.y * (screenH / 720.0f);
			}
			ImVec2 dPos(dX, dY);

			for (float ox = -3.0f; ox <= 3.0f; ox += 3.0f)
			{
				for (float oy = -3.0f; oy <= 3.0f; oy += 3.0f)
				{
					drawList->AddText(defFont, dFontSize, ImVec2(dPos.x + ox, dPos.y + oy), IM_COL32(0, 0, 0, 255), clearText);
				}
			}
			ImU32 defCol = defItem ? (ImU32)ImColor(defItem->color.x, defItem->color.y, defItem->color.z, defItem->color.w) : IM_COL32(255, 215, 0, 255);
			drawList->AddText(defFont, dFontSize, dPos, defCol, clearText);
		}
	}

	drawList->PopClipRect();
#endif
}






void BossBattleController::LoadSettings(ArmoredTrainBoss* armoredTrainBoss_)
{
	std::string filepath = "resources/json/settings/boss_settings.json";
	std::ifstream file(filepath);
	if (!file.is_open()) return;

	try
	{
		nlohmann::json root;
		file >> root;
		if (root.contains("boss_spawn_progress"))
		{
			bossSpawnProgressThreshold_ = root["boss_spawn_progress"].get<float>();
		}
		if (armoredTrainBoss_)
		{
			if (root.contains("is_follow_player"))
			{
				armoredTrainBoss_->SetFollowPlayer(root["is_follow_player"].get<bool>());
			}
			if (root.contains("boss_scale"))
			{
				armoredTrainBoss_->SetScaleMultiplier(root["boss_scale"].get<float>());
			}
			if (root.contains("desired_lead_distance"))
			{
				armoredTrainBoss_->SetDesiredLeadDistance(root["desired_lead_distance"].get<float>());
			}
			if (root.contains("is_random_lead_enabled"))
			{
				armoredTrainBoss_->SetRandomLeadDistanceEnabled(root["is_random_lead_enabled"].get<bool>());
			}
			if (root.contains("rush_speed_bonus"))
			{
				armoredTrainBoss_->SetRushSpeedBonus(root["rush_speed_bonus"].get<float>());
			}
			if (root.contains("is_ground_snap_enabled"))
			{
				armoredTrainBoss_->SetGroundSnapEnabled(root["is_ground_snap_enabled"].get<bool>());
			}
			if (root.contains("ground_snap_offset"))
			{
				armoredTrainBoss_->SetGroundSnapOffset(root["ground_snap_offset"].get<float>());
			}
		}
	}
	catch (...)
	{
	}
}


void BossBattleController::SaveSettings(ArmoredTrainBoss* armoredTrainBoss_)
{
	std::string dir = "resources/json/settings";
	std::filesystem::create_directories(dir);
	std::string filepath = dir + "/boss_settings.json";
	std::ofstream file(filepath);
	if (!file.is_open()) return;

	nlohmann::json root;
	root["boss_spawn_progress"] = bossSpawnProgressThreshold_;
	if (armoredTrainBoss_)
	{
		root["boss_scale"] = armoredTrainBoss_->GetScaleMultiplier();
		root["is_follow_player"] = armoredTrainBoss_->IsFollowPlayer();
		root["desired_lead_distance"] = armoredTrainBoss_->GetDesiredLeadDistance();
		root["is_random_lead_enabled"] = armoredTrainBoss_->IsRandomLeadDistanceEnabled();
		root["rush_speed_bonus"] = armoredTrainBoss_->GetRushSpeedBonus();
		root["is_ground_snap_enabled"] = armoredTrainBoss_->IsGroundSnapEnabled();
		root["ground_snap_offset"] = armoredTrainBoss_->GetGroundSnapOffset();
	}

	file << root.dump(4);
}
