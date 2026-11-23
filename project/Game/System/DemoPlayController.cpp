#define NOMINMAX
#include "DemoPlayController.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "Game/System/BossBattleController.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <cmath>

void DemoPlayController::ResetAll()
{
	demoTimer_ = 0.0f;
	demoDeathLoopCount_ = 0;
	ResetCycle();
}

void DemoPlayController::ResetCycle()
{
	demoSurvivalTimer_ = 0.0f;
	demoBulletSpawned_ = false;
	demoHitTriggered_ = false;
	demoGameOverWaitTimer_ = 0.0f;
	demoMenuSelectStep_ = 0;
}

void DemoPlayController::Update(
	EngineServices* services,
	float dt,
	bool isPlayingPhase,
	Player* player,
	BossFinisherSequence* finisher,
	BossBattleController* bossBattle,
	ArmoredTrainBoss* boss,
	std::list<std::unique_ptr<PlayerBullet>>& bullets,
	const std::function<void()>& onStartBossWarning)
{
	// デモプレイ中：ゲームクリア＆処刑フィニッシャー演出を自動進行する
	if (services && services->IsDemoMode())
	{
		if (player)
		{
			player->SetGodMode(true);
		}
		if (isPlayingPhase)
		{
			// 生き生きとした自動飛行（左右・上下へのスウェー移動＋バレルロール回避）
			if (player)
			{
				bool inFinisher = (finisher && finisher->IsActive());
				float moveAmpX = inFinisher ? 0.45f : 0.85f;
				float moveAmpY = inFinisher ? 0.25f : 0.45f;
				float moveX = std::sin(demoTimer_ * 2.2f) * moveAmpX;
				float moveY = std::sin(demoTimer_ * 1.5f + 0.6f) * moveAmpY;
				bool rollNow = false;
				if (!inFinisher && bossBattle && bossBattle->GetStep() == BossBattleController::Step::BATTLE && boss && boss->IsCloseToPlayer() && !demoHitTriggered_ && demoSurvivalTimer_ >= 0.5f)
				{
					demoHitTriggered_ = true; // デモ用バレルロール1回トリガーとして使用
					rollNow = true;
				}
				player->SetDemoInputOverride(true, moveX, moveY, rollNow, 1.0f);
			}
			demoTimer_ += dt;

			if (bossBattle && bossBattle->GetStep() == BossBattleController::Step::NOT_ACTIVE)
			{
				// 39%位置から開始後、すぐにボス戦へ移行
				demoSurvivalTimer_ += dt;
				if (demoSurvivalTimer_ >= 0.45f)
				{
					demoSurvivalTimer_ = 0.0f;
					if (onStartBossWarning) onStartBossWarning();
				}
			}
			else if (bossBattle && bossBattle->GetStep() == BossBattleController::Step::BATTLE && boss && boss->IsActive() &&
				(!finisher || (!finisher->IsActive() && !finisher->IsCompleted())))
			{
				// ボスがカメラ前方（並走位置）にしっかり映ってからタイマーを進める
				if (boss->IsCloseToPlayer())
				{
					demoSurvivalTimer_ += dt;
				}
				// ボスがカメラ前に現れてから1.2秒後（ロール回避後）に先頭車両へ向けて通常弾を発射（1発命中でHP1処刑条件へ移行）
				if (demoSurvivalTimer_ >= 1.2f && !demoBulletSpawned_)
				{
					demoBulletSpawned_ = true;
					demoSurvivalTimer_ = 1.0f; // 万一外れた場合は0.5秒後に再射撃
					if (player && services->GetObject3dCommon())
					{
						Vector3 pPos = player->GetWorldPosition();
						Vector3 targetPos = boss->GetCarriagePosition(0) + Vector3{ 0.0f, 7.0f, 0.0f };
						Vector3 dir = targetPos - pPos;
						float len = dir.Length();
						Vector3 vel = (len > 0.001f) ? (dir / len) * 12.0f : Vector3{ 0.0f, 0.0f, 12.0f };
						auto b = std::make_unique<PlayerBullet>();
						b->Initialize(services->GetObject3dCommon(), pPos, vel, nullptr);
						bullets.push_back(std::move(b));
						SoundManager::GetInstance()->PlaySE("shot.wav", 0.5f);
					}
				}
				if (demoBulletSpawned_ && demoSurvivalTimer_ >= 1.5f)
				{
					demoBulletSpawned_ = false;
				}
			}
		}
		else if (player)
		{
			player->SetDemoInputOverride(false);
		}
	}
	else
	{
		if (player)
		{
			if (player->IsGodMode()) player->SetGodMode(false);
			player->SetDemoInputOverride(false);
		}
		demoSurvivalTimer_ = 0.0f;
		demoBulletSpawned_ = false;
		demoHitTriggered_ = false;
	}
}
