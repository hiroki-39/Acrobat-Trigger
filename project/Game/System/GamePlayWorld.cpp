#define NOMINMAX
#include "GamePlayWorld.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/PlayerBullet.h"
#include "Game/Actor/Bullet/PlayerMissile.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/Actor/Item/EnhanceRing.h"
#include "Game/Actor/Item/MemoryItem.h"
#include "Game/System/Rail.h"
#include "Game/System/RailCameraController.h"
#include "Game/System/CollisionManager.h"
#include "Game/System/BossBattleController.h"
#include "Game/System/GamePlayHUD.h"
#include "Game/System/GamePlayVfxManager.h"
#include "Game/System/TerrainCollisionDebugger.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "Game/Sequence/OpeningCutsceneSequence.h"
#include "Game/Sequence/PauseMenu.h"
#include "Game/Sequence/GameOverSequence.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Skybox/Skybox.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Math/CollisionMath.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
void GamePlayWorld::UpdateActorsAndCollisions(float dt, float unscaledGameSpeed, const GamePlayWorldContext& ctx)
{
	if (ctx.player)
	{
		if (ctx.isPlaying && !ctx.pauseMenu->IsPaused() && !ctx.isGameOverPhase && !ctx.isClearPhase)
		{

			ctx.player->SetAssistTarget(nullptr);

			// レール進行度に応じた動的移動制限の適用
			if (!(*ctx.mainRails).empty() && (*ctx.mainRails)[0]->IsValid() && ctx.railCameraController)
			{
				float p = ctx.railCameraController->GetProgress();
				float limitX, limitYMin, limitYMax;
				(*ctx.mainRails)[0]->GetMoveLimits(p, limitX, limitYMin, limitYMax);
				ctx.player->SetTargetMoveLimits(limitX, limitYMin, limitYMax);
			}

			ctx.player->Update((*ctx.bullets), (*ctx.missiles), (*ctx.enemies), ctx.cameraObject, unscaledGameSpeed, ctx.armoredTrainBoss);


			if (ctx.player->ConsumeDodgeTrigger())
			{
				ctx.vfx->Dodge().SetParentMatrix(&ctx.player->GetObject3d()->GetmatWorld());
				ctx.vfx->Dodge().SetPosition({ 0.0f, 0.0f, -1.5f });
				ctx.vfx->Dodge().Play();
			}
		}
		else
		{

			ctx.player->Update3DObjectOnly();
		}
	}


	if (ctx.isPlaying && !ctx.pauseMenu->IsPaused())
	{
		for (auto it = (*ctx.bullets).begin(); it != (*ctx.bullets).end(); )
		{
			(*it)->Update(ctx.gameSpeed);
			if ((*it)->IsDead())
			{
				it = (*ctx.bullets).erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto it = (*ctx.missiles).begin(); it != (*ctx.missiles).end(); )
		{
			(*it)->Update(ctx.gameSpeed);

			if ((*it)->GetCurrentPhase() == PlayerMissile::Phase::FLIGHT)
			{
				ctx.vfx->MissileSmoke().SetPosition((*it)->GetPosition());
				ctx.vfx->MissileSmoke().Play();
			}

			if ((*it)->IsDead())
			{
				it = (*ctx.missiles).erase(it);
			}
			else
			{
				++it;
			}
		}

		Vector3 cameraPos = ctx.activeCamera->GetTranslate();
		Vector3 rot = ctx.activeCamera->GetRotation();
		Vector3 cameraForward = { std::sinf(rot.y), 0.0f, std::cosf(rot.y) };


		// 各種アクターの更新
		for (auto& enemy : (*ctx.enemies))
		{
			enemy->Update(cameraPos, cameraForward, ctx.player, (*ctx.enemyBullets), ctx.gameSpeed);
		}

		if (ctx.armoredTrainBoss && ((*ctx.isBossSpawned) || (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsActive())))
		{
			Vector3 camPos = ctx.activeCamera ? ctx.activeCamera->GetTranslate() : Vector3{ 0,0,0 };
			float playerSpeed = 50.0f;
			if (ctx.railCameraController)
			{
				playerSpeed = ctx.railCameraController->GetBaseSpeed() * ctx.railCameraController->GetSpeedMultiplier();
			}
			// 処刑ムービー中はレール進行停止のため ctx.gameSpeed が 0 になるが、
			// ボスのグリッチ演出と瀕死パーティクル（criticalParticles_）は動かし続ける
			float bossGameSpeed = (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsMoviePhase()) ? (dt * 60.0f) : ctx.gameSpeed;
			ctx.armoredTrainBoss->Update(camPos, cameraForward, playerSpeed, ctx.player, (*ctx.enemyBullets), bossGameSpeed);
		}

		#if 1
		Vector3 bombPos = { 0.0f, 0.0f, 0.0f };
		float bombRadius = 0.0f;
		bool isBombClearing = ctx.player && ctx.player->HasActiveBombExplosion(&bombPos, &bombRadius);

		for (auto it = (*ctx.enemyBullets).begin(); it != (*ctx.enemyBullets).end(); )
		{
			if (isBombClearing)
			{
				Vector3 ebPos = (*it)->GetPosition();
				float bdx = ebPos.x - bombPos.x;
				float bdy = ebPos.y - bombPos.y;
				float bdz = ebPos.z - bombPos.z;
				if (bdx * bdx + bdy * bdy + bdz * bdz <= bombRadius * bombRadius)
				{
					it = (*ctx.enemyBullets).erase(it);
					continue;
				}
			}
			(*it)->Update(ctx.gameSpeed);
			if ((*it)->IsDead())
			{
				it = (*ctx.enemyBullets).erase(it);
			}
			else
			{
				++it;
			}
		}
#endif
		for (auto& bullet : (*ctx.enemyBullets))
		{
			// bullet->Update(ctx.gameSpeed);
		}

		for (auto& ring : (*ctx.enhanceRings))
		{
			ring->Update();
		}

		// メモリドロップアイテムの更新・マグネット吸引・回収判定
		for (auto it = (*ctx.memoryItems).begin(); it != (*ctx.memoryItems).end(); )
		{
			(*it)->Update(ctx.gameSpeed, ctx.player, ctx.vfx);
			if ((*it)->IsDead())
			{
				it = (*ctx.memoryItems).erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto& obstacle : (*ctx.obstacles))
		{
			obstacle->Update();
		}

		// 地形コリジョンデバッガーのフレーム開始
		TerrainCollisionDebugger::GetInstance()->BeginFrame();

		// 衝突判定管理クラス (CollisionManager) による一括判定
		CollisionContext colCtx;
		colCtx.player = ctx.player;
		colCtx.playerBullets = &(*ctx.bullets);
		colCtx.playerMissiles = &(*ctx.missiles);
		colCtx.enemies = &(*ctx.enemies);
		colCtx.boss = ctx.armoredTrainBoss;
		colCtx.isBossSpawned = (*ctx.isBossSpawned);
		colCtx.enemyBullets = &(*ctx.enemyBullets);
		colCtx.obstacles = &(*ctx.obstacles);
		colCtx.enhanceRings = &(*ctx.enhanceRings);
		colCtx.terrainModels = &(*ctx.modelInstances);
		colCtx.cameraObject = ctx.cameraObject;

		colCtx.hitEffect = &ctx.vfx->Hit();
		colCtx.explosionEffect = &ctx.vfx->Explosion();
		colCtx.onSpawnVoxelHit = [ctx, this](const Vector3& pos, int count, float minSz, float maxSz) {
			ctx.vfx->SpawnVoxelHitParticles(pos, ctx.activeCamera, count, minSz, maxSz);
		};
		colCtx.onSpawnBulletHit = [ctx](const Vector3& pos, const Vector3& vel) {
			ctx.vfx->SpawnBulletHitVoxelParticles(pos, vel);
		};
		colCtx.onSpawnEnemyBulletTerrainHit = [ctx](const Vector3& pos) {
			ctx.vfx->SpawnPlayerDamageParticles(pos);
		};
		colCtx.onSpawnEnemyDissolve = [ctx](const Vector3& pos) {
			ctx.vfx->SpawnEnemyDissolveShards(pos);
		};
		colCtx.onSpawnRingPass = [ctx](const Vector3& pos, bool isHeal) {
			ctx.vfx->SpawnRingPassParticles(pos, isHeal);
		};
		colCtx.dodgeEffect = &ctx.vfx->Dodge();
		colCtx.ringEffect = &ctx.vfx->Ring();
		colCtx.healRingEffect = &ctx.vfx->HealRing();

		colCtx.isDemoMode = EngineServices::GetInstance()->IsDemoMode();
		colCtx.score = &(*ctx.score);
		colCtx.cameraShakeTimer = &(*ctx.cameraShakeTimer);
		colCtx.isJustDodgeActive = &(*ctx.isJustDodgeActive);
		colCtx.justDodgeTimer = &(*ctx.justDodgeTimer);
		colCtx.justDodgeMaxTime = ctx.justDodgeMaxTime;
		colCtx.acquiredEnhanceRingCount = &ctx.hud->AcquiredEnhanceRingCount();
		colCtx.maxEnhanceRingIcons = GamePlayHUD::kMaxEnhanceRingIcons;

		colCtx.onBossDefeated = [ctx, this]() {
			if (ctx.bossBattle->GetStep() == BossBattleController::Step::BATTLE)
			{
				ctx.bossBattle->SetDefeated();
				(*ctx.score) += 5000; // ボス撃破ボーナス
				SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
				(*ctx.cameraShakeTimer) = 1.0f;
			}
		};

		colCtx.onEnemyKilled = [ctx, this](const Vector3& pos, int addScore) {
			++(*ctx.killedEnemiesCount);
			// 1. 敵撃破時のごくわずかな微小カメラシェイク（1〜2フレームだけ数ピクセル振動）
			(*ctx.enemyKillShakeFrames) = 2;
			// 2. 敵撃破座標にスコア加算ポップアップ演出を登録
			ctx.hud->AddScorePopup(pos, addScore, 0.65f);
		};

		// 撃破エフェクト（ディゾルブ）完了後にメモリをドロップ
		colCtx.onEnemyDissolveFinished = [ctx, this](const Vector3& pos) {
			static std::mt19937 memRng(1337);
			std::uniform_real_distribution<float> memDist(0.0f, 1.0f);
			if (memDist(memRng) < 0.40f) {
				const MemoryType kDropTypes[] = {
					MemoryType::CYBER_BIT,
					MemoryType::BOMB,
					MemoryType::SPREAD,
				};
				std::uniform_int_distribution<size_t> typeDist(0, 2);
				SpawnMemoryItem(pos, kDropTypes[typeDist(memRng)], (*ctx.memoryItems));
			}
		};

		if (ctx.collisionManager)
		{
			ctx.collisionManager->ProcessAllCollisions(colCtx);
		}

		TerrainCollisionDebugger::GetInstance()->EndFrame();

		bool isLockOn = false;
		if (ctx.player && ctx.player->GetObject3d() && ctx.player->GetReticle() && ctx.activeCamera)
		{
			Vector3 cameraPos = ctx.activeCamera->GetTranslate();
			Vector3 reticlePos = ctx.player->GetReticleWorldPosition();


			Vector3 rayDir = {
				reticlePos.x - cameraPos.x,
				reticlePos.y - cameraPos.y,
				reticlePos.z - cameraPos.z
			};
			float length = std::sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);

			Vector3 lockOnPos = { 0, 0, 0 };
			Enemy* lockOnEnemy = nullptr;

			if (length > 0.0001f)
			{
				rayDir.x /= length;
				rayDir.y /= length;
				rayDir.z /= length;

				Ray ray = { cameraPos, rayDir };

				float bestScore = 1000000.0f;


				for (auto& enemy : (*ctx.enemies))
				{
					if (enemy->IsDead()) continue;

					float dist = 0.0f;
					bool hit = enemy->CheckRaycast(ray, &dist);

					float maxLockDist = ctx.player ? ctx.player->GetLockOnMaxDistance() : ctx.lockOnMaxDistance;
					// 最大射程距離以内の敵のみロックオン対象とする
					if (hit && dist <= maxLockDist)
					{
						Vector3 enemyPos = enemy->GetPosition();
						Vector3 toEnemy = { enemyPos.x - cameraPos.x, enemyPos.y - cameraPos.y, enemyPos.z - cameraPos.z };
						float toEnemyLen = std::sqrt(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y + toEnemy.z * toEnemy.z);
						if (toEnemyLen > 0.0f)
						{
							toEnemy.x /= toEnemyLen;
							toEnemy.y /= toEnemyLen;
							toEnemy.z /= toEnemyLen;
						}

						float dot = ray.direction.x * toEnemy.x + ray.direction.y * toEnemy.y + ray.direction.z * toEnemy.z;
						float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

						float score = angle * 100.0f + dist * 0.1f;

						if (score < bestScore)
						{
							bestScore = score;
							lockOnPos = {
								ray.origin.x + ray.direction.x * dist,
								ray.origin.y + ray.direction.y * dist,
								ray.origin.z + ray.direction.z * dist
							};
							isLockOn = true;
							lockOnEnemy = enemy.get();
						}
					}
				}
			}

			float minEnemyDist = 1000000.0f;
			for (auto& enemy : (*ctx.enemies))
			{
				if (enemy->IsDead()) continue;
				Vector3 ePos = enemy->GetPosition();
				float d = std::sqrt((ePos.x - cameraPos.x) * (ePos.x - cameraPos.x) +
					(ePos.y - cameraPos.y) * (ePos.y - cameraPos.y) +
					(ePos.z - cameraPos.z) * (ePos.z - cameraPos.z));
				if (d < minEnemyDist)
				{
					minEnemyDist = d;
				}
			}

			float maxLockDist = ctx.player ? ctx.player->GetLockOnMaxDistance() : ctx.lockOnMaxDistance;
			if (isLockOn)
			{
				ctx.player->SetReticleColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // ロックオン時: 赤
				ctx.player->SetLockOn(true, lockOnPos, lockOnEnemy);
			}
			else
			{
				if (minEnemyDist < maxLockDist)
				{
					// 射程内に敵が存在する（接近: オレンジ）
					ctx.player->SetReticleColor({ 1.0f, 0.6f, 0.0f, 1.0f });
				}
				else
				{
					// 射程外（通常: 白）
					ctx.player->SetReticleColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				}
				ctx.player->SetLockOn(false);
			}
		}


		for (auto it = (*ctx.bullets).begin(); it != (*ctx.bullets).end(); )
		{
			if ((*it)->IsDead())
			{
				it = (*ctx.bullets).erase(it);
			}
			else
			{
				++it;
			}
		}


		for (auto it = (*ctx.missiles).begin(); it != (*ctx.missiles).end(); )
		{
			if ((*it)->IsDead())
			{
				it = (*ctx.missiles).erase(it);
			}
			else
			{
				++it;
			}
		}

	}
	else
	{
		for (auto& bullet : (*ctx.bullets))
		{
			bullet->Update3DObjectOnly();
		}
		for (auto& missile : (*ctx.missiles))
		{
			missile->Update3DObjectOnly();
		}
		for (auto& enemy : (*ctx.enemies))
		{
			enemy->Update3DObjectOnly();
		}
	}




}


void GamePlayWorld::Draw(const GamePlayWorldContext& ctx)
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();
	auto spriteCommon = services->GetSpriteCommon();

	if (ctx.skybox)
	{
		ctx.skybox->Draw();
	}

	// 視錐台（Frustum）カリング用の情報取得
	Frustum frustum{};
	bool enableCulling = false;
	if (ctx.activeCamera)
	{
		Matrix4x4 vp = ctx.activeCamera->GetViewMatrix() * ctx.activeCamera->GetProjectionMatrix();
		frustum = Frustum::CreateFromViewProjection(vp);
		enableCulling = true;
	}

	if (object3dCommon) object3dCommon->SetCommonDrawSetting();

	for (auto& model : (*ctx.modelInstances))
	{
		if (!model) continue;

		if (enableCulling)
		{
			Vector3 center;
			float radius = 0.0f;
			if (model->GetBoundingSphere(center, radius))
			{
				// 半径が極端に巨大なオブジェクト（広域背景など）以外は視錐台カリング
				if (radius < 300.0f && !frustum.ContainsSphere(center, radius))
				{
					continue; // 画面外のため描画スキップ
				}
			}
		}

		model->Draw();
	}

	// スタート演出中のオブジェクト実体化判定（地形が実体化した領域の敵やリングのみ出現・描画）
	auto IsMaterializedAt = [&](const Vector3& pos) -> bool {
		if (!ctx.isStartCutscenePhase)
		{
			return true;
		}
		const float kPhase1Duration = 2.20f;
		if (ctx.openingSequence->GetTimer() >= kPhase1Duration)
		{
			return true; // Phase 1 完了後はすべて出現
		}

		// シェーダー側の実体化波（solidT）と完全に同一の進行度計算
		float solidT = std::clamp((ctx.terrainMaterializeProgress - 0.54f) / 0.46f, 0.0f, 1.0f);
		if (solidT <= 0.0001f)
		{
			return false; // まだ実体化波がスタートしていない（ワイヤーフレーム段階では一切非表示）
		}

		float scanDistSolid = -60.0f + (850.0f - (-60.0f)) * solidT;

		Vector3 camPos = ctx.activeCamera ? ctx.activeCamera->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
		float zDist = pos.z - camPos.z;
		float heightDelay = std::max(0.0f, pos.y) * 1.5f;
		float scanCoord = zDist + heightDelay;

		return (scanCoord <= scanDistSolid);
	};

	if (object3dCommon) object3dCommon->SetCommonDrawSetting();
	for (auto& enemy : (*ctx.enemies))
	{
		if (ctx.gameOverSequence->IsRetryDematerializing()) continue; // 地形消滅演出中は敵を完全非表示
		if (ctx.isStartCutscenePhase && !IsMaterializedAt(enemy->GetPosition()))
		{
			continue; // 地形実体化前の敵は非表示
		}
		if (enableCulling)
		{
			Vector3 ePos = enemy->GetPosition();
			if (!frustum.ContainsSphere(ePos, 20.0f))
			{
				continue; // 画面外スキップ
			}
		}
		enemy->Draw();
	}
	if (ctx.armoredTrainBoss && ((*ctx.isBossSpawned) || (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsActive())) && !ctx.gameOverSequence->IsRetryDematerializing())
	{
		if (!ctx.isStartCutscenePhase || IsMaterializedAt(ctx.armoredTrainBoss->GetPosition()))
		{
			ctx.armoredTrainBoss->Draw();
			ctx.bossBattle->Draw3D();
		}
	}
	for (auto& obstacle : (*ctx.obstacles))
	{
		if (enableCulling)
		{
			Vector3 oPos = obstacle->GetPosition();
			if (!frustum.ContainsSphere(oPos, 25.0f))
			{
				continue; // 画面外スキップ
			}
		}
		obstacle->Draw();
	}
	for (auto& ring : (*ctx.enhanceRings))
	{
		if (ctx.gameOverSequence->IsRetryDematerializing()) continue; // 地形消滅演出中はリングを完全非表示
		if (ctx.isStartCutscenePhase && !IsMaterializedAt(ring->GetPosition()))
		{
			continue; // 地形実体化前の領域は非表示
		}
		if (enableCulling)
		{
			Vector3 rPos = ring->GetPosition();
			if (!frustum.ContainsSphere(rPos, 20.0f))
			{
				continue; // 画面外スキップ
			}
		}
		ring->Draw();
	}

	// メモリドロップアイテムの描画
	for (auto& item : (*ctx.memoryItems))
	{
		if (item) item->Draw();
	}


	if (ctx.isDrawCollider || ctx.isDrawTerrainWireframe)
	{
		if (object3dCommon) object3dCommon->SetWireframeDrawSetting();

		if (ctx.isDrawCollider)
		{
			// 障害物のコライダー描画
			for (auto& obstacle : (*ctx.obstacles))
			{
				obstacle->DrawCollider();
			}
			for (auto& enemy : (*ctx.enemies))
			{
				if (ctx.isStartCutscenePhase && !IsMaterializedAt(enemy->GetPosition())) continue;
				enemy->DrawCollider();
			}
			if (ctx.armoredTrainBoss && (*ctx.isBossSpawned))
			{
				if (!ctx.isStartCutscenePhase || IsMaterializedAt(ctx.armoredTrainBoss->GetPosition()))
				{
					ctx.armoredTrainBoss->DrawCollider();
				}
			}
			for (auto& ring : (*ctx.enhanceRings))
			{
				if (ctx.isStartCutscenePhase && !IsMaterializedAt(ring->GetPosition())) continue;
				ring->DrawCollider();
			}
			for (auto& bullet : (*ctx.bullets))
			{
				bullet->DrawCollider();
			}
			for (auto& missile : (*ctx.missiles))
			{
				missile->DrawCollider();
			}

			#if 0
		Vector3 bombPos = { 0.0f, 0.0f, 0.0f };
		float bombRadius = 0.0f;
		bool isBombClearing = ctx.player && ctx.player->HasActiveBombExplosion(&bombPos, &bombRadius);

		for (auto it = (*ctx.enemyBullets).begin(); it != (*ctx.enemyBullets).end(); )
		{
			if (isBombClearing)
			{
				Vector3 ebPos = (*it)->GetPosition();
				float bdx = ebPos.x - bombPos.x;
				float bdy = ebPos.y - bombPos.y;
				float bdz = ebPos.z - bombPos.z;
				if (bdx * bdx + bdy * bdy + bdz * bdz <= bombRadius * bombRadius)
				{
					it = (*ctx.enemyBullets).erase(it);
					continue;
				}
			}
			(*it)->Update(ctx.gameSpeed);
			if ((*it)->IsDead())
			{
				it = (*ctx.enemyBullets).erase(it);
			}
			else
			{
				++it;
			}
		}
#endif
		for (auto& bullet : (*ctx.enemyBullets))
			{
				bullet->DrawCollider();
			}
			if (ctx.player)
			{
				ctx.player->DrawCollider();
			}
		}

		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
	}


	for (auto& bullet : (*ctx.bullets))
	{
		bullet->Draw();
	}
	for (auto& missile : (*ctx.missiles))
	{
		missile->Draw();
	}
	for (auto& bullet : (*ctx.enemyBullets))
	{
		bullet->Draw();
	}


	if (ctx.player)
	{
		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
		ctx.player->Draw();

		// ゲームオーバー破片プリズム描画
		if (ctx.isGameOverPhase)
		{
			ctx.gameOverSequence->Draw3D(ctx.activeCamera, object3dCommon);
		}

		// スタート演出中の自機復元ポリゴン破片描画（Phase 2: 自機集束フェーズ 2.20s〜3.05s のみ描画）
		if (ctx.isStartCutscenePhase)
		{
			ctx.openingSequence->Draw3D(ctx.activeCamera, object3dCommon);
		}
	}


	if (ctx.isDrawRail)
	{
		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
		for (auto& vis : (*ctx.railVisualizers))
		{
			if (vis) vis->Draw();
		}
		for (auto& vis : (*ctx.enemyRailVisualizers))
		{
			if (vis) vis->Draw();
		}
	}

	bool drawThruster = (!ctx.isStartCutscenePhase || ctx.openingSequence->IsThrusterReady() || (ctx.player && ctx.player->IsCutsceneBoosting())) && !ctx.isGameOverPhase;
	bool drawWind = (ctx.isPlayingPhase || (ctx.isStartCutscenePhase && ctx.openingSequence->IsThrusterReady()));
	ctx.vfx->Draw(drawThruster, drawWind, object3dCommon);

	// 地形コリジョンデバッグ描画（判定の計算が行われている部分のみ描画）
	if (ctx.isDrawTerrainWireframe || ctx.isDrawCollider)
	{
		TerrainCollisionDebugger::GetInstance()->SetDebugEnabled(true);
		TerrainCollisionDebugger::GetInstance()->Draw(ctx.activeCamera);
	}
	else
	{
		TerrainCollisionDebugger::GetInstance()->SetDebugEnabled(false);
	}

	// 電脳フィニッシャー演出（弱点コア、ミサイル、崩壊シャード等）の3D描画
	if (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsActive())
	{
		auto services = EngineServices::GetInstance();
		auto obj3dCommon = services ? services->GetObject3dCommon() : nullptr;
		if (obj3dCommon) obj3dCommon->SetCommonDrawSetting();
		auto srvMgr = services ? services->GetSrvManager() : nullptr;
		if (srvMgr) srvMgr->PreDraw();
		ctx.bossFinisherSequence->Draw3D();
	}
}


void GamePlayWorld::SpawnMemoryItem(const Vector3& pos, MemoryType type, std::list<std::unique_ptr<MemoryItem>>& memoryItems)
{
	auto object3dCommon = EngineServices::GetInstance()->GetObject3dCommon();
	auto item = std::make_unique<MemoryItem>();
	item->Initialize(object3dCommon, pos, type);
	memoryItems.push_back(std::move(item));
}
void GamePlayWorld::UpdateCameraFovAndRail(const GamePlayWorldContext& ctx, const Vector3& clearStartPlayerPos)
{
			// ブースト時の動的FOV拡大演出（通常66度 → ブースト時76度へ滑らかに補間）
			if (ctx.activeCamera)
			{
				float targetFov = (ctx.player && ctx.player->IsBoosting()) ? 1.32f : 1.15f;
				float currentFov = ctx.activeCamera->GetFovY();
				currentFov += (targetFov - currentFov) * 0.15f;
				ctx.activeCamera->SetFovY(currentFov);
			}


			if (ctx.railCameraController)
			{
				Vector3 cameraTrackPos = ctx.player->GetTranslate();
				if (ctx.isClearPhase || (ctx.bossFinisherSequence && ctx.bossFinisherSequence->IsMoviePhase()))
				{
					// クリア演出・処刑ムービー中は自機が移動してもレールカメラを動かさない
					cameraTrackPos = clearStartPlayerPos;
				}
				ctx.railCameraController->Update(ctx.gameSpeed, cameraTrackPos);
			}

}

void GamePlayWorld::UpdateCameraAndEnvironment(const GamePlayWorldContext& ctx)
{
	if (ctx.activeCamera)
	{
		// 1. 通常の大きなカメラシェイク（被弾・ボス爆発）
		if (ctx.cameraShakeTimer && *ctx.cameraShakeTimer > 0.0f)
		{
			*ctx.cameraShakeTimer -= 1.0f;
			float p = *ctx.cameraShakeTimer / 20.0f;
			float shakePower = 2.0f * p;
			Vector3 pos = ctx.activeCamera->GetTranslate();
			pos.x += ((rand() % 100) / 50.0f - 1.0f) * shakePower;
			pos.y += ((rand() % 100) / 50.0f - 1.0f) * shakePower;
			ctx.activeCamera->SetTranslate(pos);
		}
		// 2. 敵撃破時のごくわずかなマイクロシェイク（1〜2フレームだけ微小に振動）
		if (ctx.enemyKillShakeFrames && *ctx.enemyKillShakeFrames > 0)
		{
			--(*ctx.enemyKillShakeFrames);
			float killShakePower = 0.22f;
			Vector3 pos = ctx.activeCamera->GetTranslate();
			pos.x += ((rand() % 100) / 50.0f - 1.0f) * killShakePower;
			pos.y += ((rand() % 100) / 50.0f - 1.0f) * killShakePower;
			ctx.activeCamera->SetTranslate(pos);
		}
		// 3. 被弾時のカメラパンチ（Zロール傾き・バネ減衰）
		if (ctx.player)
		{
			Vector3 camRot = ctx.activeCamera->GetRotation();
			camRot.z = ctx.player->GetCameraPunchRoll();
			ctx.activeCamera->SetRotation(camRot);
		}
		ctx.activeCamera->Update();
	}

	if (ctx.modelInstances)
	{
		for (auto& model : *ctx.modelInstances) if (model) model->Update();
	}
	if (ctx.isDrawRail)
	{
		if (ctx.railVisualizers) for (auto& vis : *ctx.railVisualizers) if (vis) vis->Update();
		if (ctx.enemyRailVisualizers) for (auto& vis : *ctx.enemyRailVisualizers) if (vis) vis->Update();
	}

	if (!ctx.isPlaying || (ctx.pauseMenu && ctx.pauseMenu->IsPaused()))
	{
		if (ctx.enemies) for (auto& enemy : *ctx.enemies) if (enemy) enemy->Update3DObjectOnly();
		if (ctx.obstacles) for (auto& obstacle : *ctx.obstacles) if (obstacle) obstacle->Update3DObjectOnly();
		if (ctx.enhanceRings) for (auto& ring : *ctx.enhanceRings) if (ring) ring->Update();
	}

	if (ctx.skybox)
	{
		ctx.skybox->SetCamera(ctx.activeCamera);
		ctx.skybox->Update();
	}
}

void GamePlayWorld::InitializeCamerasAndSkybox(
	std::unique_ptr<Camera>& camera,
	std::unique_ptr<Camera>& debugCamera,
	Camera*& activeCamera,
	std::unique_ptr<Skybox>& skybox,
	float& outSkyboxMs)
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	auto dxCommon = services ? services->GetDirectXCommon() : nullptr;

	camera = std::make_unique<Camera>();
	camera->SetTranslate({ 0.0f, 6.0f, -20.0f });
	camera->SetRotation({ 0.0f, 0.0f, 0.0f });

	debugCamera = std::make_unique<Camera>();
	debugCamera->SetTranslate({ 0.0f, 6.0f, -20.0f });
	debugCamera->SetRotation({ 0.0f, 0.0f, 0.0f });

	activeCamera = camera.get();
	if (object3dCommon)
	{
		object3dCommon->SetDefaultCamera(activeCamera);
	}

	ParticleManager::GetInstance()->RegisterQuad("quad", "circle2.png");
	ParticleManager::GetInstance()->RegisterRing("ring", "resources/sprites/effect/gradationLine.png", 32, 0.5f, 1.0f);
	ParticleManager::GetInstance()->RegisterCylinder("Cylinder", "resources/sprites/effect/gradationLine.png");

	if (dxCommon)
	{
		dxCommon->BeginTextureUploadBatch();
	}

	auto t0 = std::chrono::high_resolution_clock::now();
	skybox = std::make_unique<Skybox>();
	skybox->Initialize(dxCommon, "resources/skybox.dds");
	auto t1 = std::chrono::high_resolution_clock::now();
	outSkyboxMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void GamePlayWorld::InitializeActors(
	std::unique_ptr<Object3d>& cameraObject,
	std::unique_ptr<Player>& player,
	std::unique_ptr<RailCameraController>& railCameraController,
	std::unique_ptr<ArmoredTrainBoss>& armoredTrainBoss,
	bool& isBossSpawned,
	Camera* activeCamera,
	Skybox* skybox,
	const std::vector<std::unique_ptr<Rail>>& mainRails,
	const std::vector<std::unique_ptr<Object3d>>& modelInstances)
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	auto dxCommon = services ? services->GetDirectXCommon() : nullptr;

	cameraObject = std::make_unique<Object3d>();
	cameraObject->Initialize(object3dCommon);

	uint32_t skyCubemap = skybox ? skybox->GetCubemapSrvIndex() : 0;
	player = std::make_unique<Player>();
	player->Initialize(object3dCommon, skyCubemap);
	player->LoadSettings("resources/json/player/player_settings.json");

	player->GetObject3d()->SetParent(cameraObject.get());
	if (player->GetColliderObject())
	{
		player->GetColliderObject()->SetParent(cameraObject.get());
	}
	player->GetReticle()->SetParent(cameraObject.get());
	if (player->GetFrontReticle())
	{
		player->GetFrontReticle()->SetParent(player->GetObject3d());
	}
	if (player->GetGroundMarkerObject())
	{
		player->GetGroundMarkerObject()->SetParent(cameraObject.get());
	}

	railCameraController = std::make_unique<RailCameraController>();
	std::vector<Rail*> railsRaw;
	for (auto& r : mainRails)
	{
		railsRaw.push_back(r.get());
	}
	railCameraController->Initialize(railsRaw, activeCamera, cameraObject.get());

	armoredTrainBoss = std::make_unique<ArmoredTrainBoss>();
	Vector3 bossSpawnPos = { 0.0f, 0.0f, 150.0f };
	armoredTrainBoss->Initialize(object3dCommon, bossSpawnPos, skyCubemap);
	armoredTrainBoss->SetTerrainObjects(&modelInstances);
	if (!mainRails.empty())
	{
		armoredTrainBoss->SetRail(mainRails[0].get());
	}
	isBossSpawned = false;

	TerrainCollisionDebugger::GetInstance()->Initialize(dxCommon, object3dCommon);
}

void GamePlayWorld::ResetActorsForRetry(
	ArmoredTrainBoss* armoredTrainBoss,
	bool& isBossSpawned,
	RailCameraController* railCameraController,
	Rail* bossRail,
	const std::vector<std::unique_ptr<Rail>>& mainRails,
	Player* player)
{
	isBossSpawned = false;
	if (railCameraController)
	{
		railCameraController->SetBaseSpeed(50.0f);
	}
	if (armoredTrainBoss)
	{
		armoredTrainBoss->SetActive(false);
		armoredTrainBoss->ResetChaseState();
		if (bossRail && bossRail->IsValid())
		{
			armoredTrainBoss->SetRail(bossRail);
			armoredTrainBoss->SetRailProgress(0.0f);
		}
		else if (railCameraController && !mainRails.empty() && mainRails[0]->IsValid())
		{
			armoredTrainBoss->SetRail(mainRails[0].get());
			armoredTrainBoss->SetRailProgress(0.0f);
		}
	}

	if (railCameraController)
	{
		railCameraController->Reset();
	}

	if (player)
	{
		player->SetDead(false);
		player->Heal(player->GetMaxHp());
		player->ResetBoost();
		player->ResetPosture();
		player->SetPowerUpLevel(0);
		player->SetVisible(true);
	}
}
