#include "Game/System/CollisionManager.h"
#include "Game/System/TerrainCollisionDebugger.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/PostProcess/PostProcess.h"
#include <cmath>
#include <algorithm>

void CollisionManager::ProcessAllCollisions(CollisionContext& ctx)
{
    // 1. プレイヤー攻撃 vs ザコ敵
    CheckPlayerAttacksVsEnemies(ctx);

    // 2. プレイヤー攻撃 vs ボス
    CheckPlayerAttacksVsBoss(ctx);

    // 3. 敵弾 vs 障害物・地形・プレイヤー
    CheckEnemyBullets(ctx);

    // 4. 強化リング vs プレイヤー
    CheckEnhanceRingsVsPlayer(ctx);

    // 5. プレイヤー攻撃 vs 障害物
    CheckPlayerAttacksVsObstacles(ctx);

    // 6. プレイヤー vs 障害物・地形メッシュ
    CheckPlayerVsObstaclesAndTerrain(ctx);

    // 7. プレイヤー弾 vs 地形メッシュ
    CheckPlayerBulletsVsTerrain(ctx);
}

void CollisionManager::CheckPlayerAttacksVsEnemies(CollisionContext& ctx)
{
    if (!ctx.enemies || !ctx.playerBullets || !ctx.playerMissiles) return;

    for (auto it = ctx.enemies->begin(); it != ctx.enemies->end();)
    {
        // プレイヤー通常弾との判定
        for (auto& bullet : *ctx.playerBullets)
        {
            if (bullet->IsDead()) continue;

            Sphere bulletSphere = { bullet->GetPosition(), 1.0f };
            bool isHit = (*it)->CheckCollision(bulletSphere);

            if (!isHit)
            {
                Vector3 prev = bullet->GetPreviousPosition();
                Vector3 curr = bullet->GetPosition();
                Vector3 diff = { curr.x - prev.x, curr.y - prev.y, curr.z - prev.z };
                float moveLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (moveLen > 0.0001f)
                {
                    Ray moveRay = { prev, { diff.x / moveLen, diff.y / moveLen, diff.z / moveLen } };
                    float hitDist = 0.0f;
                    if ((*it)->CheckRaycast(moveRay, &hitDist))
                    {
                        if (hitDist <= moveLen + 1.0f)
                        {
                            isHit = true;
                        }
                    }
                }
            }

            if (isHit)
            {
                bullet->OnCollision();
                (*it)->OnCollision(bullet->GetDamage());

                if (ctx.hitEffect)
                {
                    ctx.hitEffect->SetPosition(bulletSphere.center);
                    ctx.hitEffect->Play();
                }
            }
        }

        // プレイヤーミサイルとの判定
        for (auto& missile : *ctx.playerMissiles)
        {
            if (missile->IsDead()) continue;

            Sphere missileSphere = { missile->GetPosition(), 1.0f };
            bool isHit = (*it)->CheckCollision(missileSphere);

            if (!isHit)
            {
                Vector3 prev = missile->GetPreviousPosition();
                Vector3 curr = missile->GetPosition();
                Vector3 diff = { curr.x - prev.x, curr.y - prev.y, curr.z - prev.z };
                float moveLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (moveLen > 0.0001f)
                {
                    Ray moveRay = { prev, { diff.x / moveLen, diff.y / moveLen, diff.z / moveLen } };
                    float hitDist = 0.0f;
                    if ((*it)->CheckRaycast(moveRay, &hitDist))
                    {
                        if (hitDist <= moveLen + 1.0f)
                        {
                            isHit = true;
                        }
                    }
                }
            }

            if (isHit)
            {
                missile->OnCollision();
                (*it)->Kill();

                if (ctx.hitEffect)
                {
                    ctx.hitEffect->SetPosition(missileSphere.center);
                    ctx.hitEffect->Play();
                }
                if (ctx.explosionEffect)
                {
                    ctx.explosionEffect->SetPosition(missileSphere.center);
                    ctx.explosionEffect->Play();
                }
            }
        }

        // 敵の死亡判定
        if ((*it)->IsDead())
        {
            Vector3 deadPos = (*it)->GetPosition();
            if (ctx.score) *ctx.score += 100;
            if (ctx.onEnemyKilled) ctx.onEnemyKilled(deadPos, 100);
            if (ctx.explosionEffect)
            {
                ctx.explosionEffect->SetPosition(deadPos);
                ctx.explosionEffect->Play();
            }
            SoundManager::GetInstance()->PlaySE("small_explosion.mp3", 0.8f);
            it = ctx.enemies->erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CollisionManager::CheckPlayerAttacksVsBoss(CollisionContext& ctx)
{
    if (!ctx.boss || !ctx.isBossSpawned || !ctx.playerBullets || !ctx.playerMissiles) return;

    // プレイヤー通常弾との判定
    for (auto& bullet : *ctx.playerBullets)
    {
        if (bullet->IsDead()) continue;
        Sphere bulletSphere = { bullet->GetPosition(), 1.5f };
        int hitCarIdx = -1;
        BossPartType hitPartType = BossPartType::Carriage;
        int hitPartIdx = -1;
        Vector3 hitPos = bulletSphere.center;

        if (ctx.boss->CheckCollision(bulletSphere, &hitCarIdx, &hitPartType, &hitPartIdx, &hitPos))
        {
            bullet->OnCollision();
            ctx.boss->OnDamaged(hitCarIdx, 1, hitPartType, hitPartIdx);
            if (ctx.hitEffect)
            {
                ctx.hitEffect->SetPosition(hitPos);
                ctx.hitEffect->Play();
            }
            if (hitPartType != BossPartType::Carriage)
            {
                if (ctx.score) *ctx.score += 150; // 部位攻撃ボーナス
            }
            else
            {
                if (ctx.score) *ctx.score += 50;
            }
        }
    }

    // プレイヤーミサイルとの判定
    for (auto& missile : *ctx.playerMissiles)
    {
        if (missile->IsDead()) continue;
        Sphere missileSphere = { missile->GetPosition(), 2.2f };
        int hitCarIdx = -1;
        BossPartType hitPartType = BossPartType::Carriage;
        int hitPartIdx = -1;
        Vector3 hitPos = missileSphere.center;

        bool isHit = ctx.boss->CheckCollision(missileSphere, &hitCarIdx, &hitPartType, &hitPartIdx, &hitPos);

        // ミサイルのすり抜け防止レイキャスト判定
        if (!isHit)
        {
            Vector3 prev = missile->GetPreviousPosition();
            Vector3 curr = missile->GetPosition();
            Vector3 diff = { curr.x - prev.x, curr.y - prev.y, curr.z - prev.z };
            float moveLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            if (moveLen > 0.0001f)
            {
                Ray moveRay = { prev, { diff.x / moveLen, diff.y / moveLen, diff.z / moveLen } };
                float hitDist = 0.0f;
                if (ctx.boss->CheckRaycast(moveRay, &hitDist, &hitCarIdx, &hitPartType, &hitPartIdx))
                {
                    if (hitDist <= moveLen + 2.2f)
                    {
                        isHit = true;
                        hitPos = { prev.x + moveRay.direction.x * hitDist, prev.y + moveRay.direction.y * hitDist, prev.z + moveRay.direction.z * hitDist };
                    }
                }
            }
        }

        if (isHit)
        {
            missile->OnCollision();
            ctx.boss->OnDamaged(hitCarIdx, 4, hitPartType, hitPartIdx);
            if (ctx.hitEffect)
            {
                ctx.hitEffect->SetPosition(hitPos);
                ctx.hitEffect->Play();
            }
            if (ctx.explosionEffect)
            {
                ctx.explosionEffect->SetPosition(hitPos);
                ctx.explosionEffect->Play();
            }
            if (hitPartType != BossPartType::Carriage)
            {
                if (ctx.score) *ctx.score += 300; // 部位破壊ボーナス
            }
            else
            {
                if (ctx.score) *ctx.score += 200;
            }
        }
    }

    // ボス撃破コールバック実行判定
    if (ctx.boss->IsDefeated() && ctx.onBossDefeated)
    {
        ctx.onBossDefeated();
    }
}

void CollisionManager::CheckEnemyBullets(CollisionContext& ctx)
{
    if (!ctx.enemyBullets) return;

    for (auto it = ctx.enemyBullets->begin(); it != ctx.enemyBullets->end();)
    {
        if (!(*it)->IsDead())
        {
            Vector3 curr = (*it)->GetPosition();
            Vector3 prev = (*it)->GetPreviousPosition();
            Sphere bulletSphere = { curr, 1.2f };
            bool isHit = false;

            // 1. 障害物（obstacles）との衝突判定
            if (ctx.obstacles)
            {
                for (const auto& obstacle : *ctx.obstacles)
                {
                    if (!obstacle || obstacle->IsDead()) continue;

                    if (obstacle->CheckCollision(bulletSphere))
                    {
                        isHit = true;
                        break;
                    }

                    // レイキャスト判定（すり抜け防止）
                    Vector3 diff = { curr.x - prev.x, curr.y - prev.y, curr.z - prev.z };
                    float moveLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    if (moveLen > 0.0001f)
                    {
                        Ray moveRay = { prev, { diff.x / moveLen, diff.y / moveLen, diff.z / moveLen } };
                        float hitDist = 0.0f;
                        if (obstacle->CheckRaycast(moveRay, &hitDist))
                        {
                            if (hitDist <= moveLen + 1.2f)
                            {
                                isHit = true;
                                break;
                            }
                        }
                    }
                }
            }

            // 2. 地形・建物メッシュ（terrainModels）との衝突判定
            if (!isHit && ctx.terrainModels)
            {
                // ボス戦中の超高速カリング：
                // ボス戦では30〜50発の敵弾・ミサイルが空中で交錯するため、
                // 全弾に対して毎フレーム全地形メッシュ（数十万ポリゴン）の総当たり判定を行うとFPSが壊滅する。
                // ボス戦中は空中戦のため、地面下（Y <= 0.0f）に突入した場合のみ地面衝突として処理する。
                if (ctx.isBossSpawned)
                {
                    if (curr.y <= 0.0f)
                    {
                        isHit = true;
                    }
                }
                else
                {
                    for (const auto& modelObj : *ctx.terrainModels)
                    {
                        if (!modelObj || !modelObj->IsCollisionEnabled()) continue;

                        if (modelObj->CheckCollisionWithSphere(bulletSphere, nullptr))
                        {
                            isHit = true;
                            break;
                        }
                    }
                }
            }

            if (isHit)
            {
                (*it)->OnCollision();
                if (ctx.hitEffect)
                {
                    ctx.hitEffect->SetPosition(curr);
                    ctx.hitEffect->Play();
                }
            }
        }

        // 3. 撃墜可能ミサイル（インターセプト）判定：プレイヤーの通常弾やミサイルで撃ち落とせる
        if (!(*it)->IsDead() && (*it)->IsDestructible())
        {
            Vector3 mPos = (*it)->GetPosition();
            Sphere missileSphere = { mPos, (*it)->GetRadius() };

            if (ctx.playerBullets)
            {
                for (auto& pb : *ctx.playerBullets)
                {
                    if (pb->IsDead()) continue;
                    Sphere pbSphere = { pb->GetPosition(), 1.2f };
                    if (CollisionMath::IsCollision(missileSphere, pbSphere))
                    {
                        pb->OnCollision();
                        (*it)->OnDamaged(1);
                        if ((*it)->IsDead())
                        {
                            if (ctx.explosionEffect)
                            {
                                ctx.explosionEffect->SetPosition(mPos);
                                ctx.explosionEffect->Play();
                            }
                            SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.75f);
                            break;
                        }
                    }
                }
            }

            if (!(*it)->IsDead() && ctx.playerMissiles)
            {
                for (auto& pm : *ctx.playerMissiles)
                {
                    if (pm->IsDead()) continue;
                    Sphere pmSphere = { pm->GetPosition(), 1.6f };
                    if (CollisionMath::IsCollision(missileSphere, pmSphere))
                    {
                        pm->OnCollision();
                        (*it)->OnDamaged(3);
                        if ((*it)->IsDead())
                        {
                            if (ctx.explosionEffect)
                            {
                                ctx.explosionEffect->SetPosition(mPos);
                                ctx.explosionEffect->Play();
                            }
                            SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.85f);
                            break;
                        }
                    }
                }
            }
        }

        // 自機との衝突判定
        if (!(*it)->IsDead() && ctx.player && !ctx.player->IsDead())
        {
            Vector3 currPos = (*it)->GetPosition();
            Vector3 prevPos = (*it)->GetPreviousPosition();
            float hitRadius = (*it)->GetRadius();
            Sphere bulletSphere = { currPos, hitRadius };
            OBB playerOBB = ctx.player->GetWorldOBB();

            // 現在位置、または前フレームとの中間位置での判定（高速移動時のすり抜け防止）
            bool hitPlayer = CollisionMath::IsCollision(bulletSphere, playerOBB);
            if (!hitPlayer)
            {
                Vector3 midPos = {
                    (currPos.x + prevPos.x) * 0.5f,
                    (currPos.y + prevPos.y) * 0.5f,
                    (currPos.z + prevPos.z) * 0.5f
                };
                Sphere midSphere = { midPos, hitRadius };
                hitPlayer = CollisionMath::IsCollision(midSphere, playerOBB);
            }

            if (hitPlayer)
            {
                if (ctx.player->IsRolling() && ctx.player->GetRollTimer() < 15.0f)
                {
                    // ジャスト回避成功
                    if (ctx.isJustDodgeActive) *ctx.isJustDodgeActive = true;
                    if (ctx.justDodgeTimer) *ctx.justDodgeTimer = ctx.justDodgeMaxTime;

                    auto pp = EngineServices::GetInstance()->GetPostProcess();
                    if (pp)
                    {
                        pp->SetEffectActive("Grayscale", true);
                        pp->SetEffectActive("Vignetting", true);
                    }

                    if (ctx.dodgeEffect)
                    {
                        ctx.dodgeEffect->SetPosition((*it)->GetPosition());
                        ctx.dodgeEffect->Play();
                    }

                    (*it)->OnCollision();
                }
                else
                {
                    // 被弾
                    (*it)->OnCollision();
                    ctx.player->OnCollision();
                    if (ctx.cameraShakeTimer) *ctx.cameraShakeTimer = 20.0f;
                }
            }
        }

        if ((*it)->IsDead())
        {
            it = ctx.enemyBullets->erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CollisionManager::CheckEnhanceRingsVsPlayer(CollisionContext& ctx)
{
    if (!ctx.enhanceRings) return;

    for (auto it = ctx.enhanceRings->begin(); it != ctx.enhanceRings->end();)
    {
        if (!(*it)->IsDead() && ctx.player && !ctx.player->IsDead())
        {
            if ((*it)->CheckCollision(ctx.player))
            {
                if ((*it)->GetType() == RingType::POWER_UP)
                {
                    if (ctx.acquiredEnhanceRingCount)
                    {
                        *ctx.acquiredEnhanceRingCount = std::min(ctx.maxEnhanceRingIcons, *ctx.acquiredEnhanceRingCount + 1);
                        if (ctx.player)
                        {
                            ctx.player->SetPowerUpLevel(*ctx.acquiredEnhanceRingCount);
                        }
                    }
                    SoundManager::GetInstance()->PlaySE("powerup.mp3", 0.9f);

                    if (ctx.ringEffect)
                    {
                        ctx.ringEffect->SetPosition((*it)->GetPosition());
                        ctx.ringEffect->Play();
                    }
                }
                else if ((*it)->GetType() == RingType::HEAL)
                {
                    ctx.player->Heal(3000);
                    SoundManager::GetInstance()->PlaySE("powerup.mp3", 0.8f);

                    if (ctx.healRingEffect)
                    {
                        ctx.healRingEffect->SetPosition((*it)->GetPosition());
                        ctx.healRingEffect->Play();
                    }
                }

                (*it)->StartShrink(ctx.player);
            }
        }

        if ((*it)->IsDead())
        {
            it = ctx.enhanceRings->erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CollisionManager::CheckPlayerAttacksVsObstacles(CollisionContext& ctx)
{
    if (!ctx.obstacles || !ctx.playerBullets || !ctx.playerMissiles) return;

    for (auto it = ctx.obstacles->begin(); it != ctx.obstacles->end();)
    {
        // プレイヤー通常弾との判定
        for (auto& bullet : *ctx.playerBullets)
        {
            if (bullet->IsDead()) continue;

            Sphere bulletSphere = { bullet->GetPosition(), 1.0f };
            bool isHit = (*it)->CheckCollision(bulletSphere);

            if (!isHit)
            {
                Vector3 prev = bullet->GetPreviousPosition();
                Vector3 curr = bullet->GetPosition();
                Vector3 diff = { curr.x - prev.x, curr.y - prev.y, curr.z - prev.z };
                float moveLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (moveLen > 0.0001f)
                {
                    Ray moveRay = { prev, { diff.x / moveLen, diff.y / moveLen, diff.z / moveLen } };
                    float hitDist = 0.0f;
                    if ((*it)->CheckRaycast(moveRay, &hitDist))
                    {
                        if (hitDist <= moveLen + 1.0f)
                        {
                            isHit = true;
                        }
                    }
                }
            }

            if (isHit)
            {
                bullet->OnCollision();
                (*it)->OnCollision(bullet->GetDamage());

                if (ctx.hitEffect)
                {
                    ctx.hitEffect->SetPosition(bulletSphere.center);
                    ctx.hitEffect->Play();
                }
            }
        }

        // プレイヤーミサイルとの判定
        for (auto& missile : *ctx.playerMissiles)
        {
            if (missile->IsDead()) continue;

            Sphere missileSphere = { missile->GetPosition(), 1.0f };
            bool isHit = (*it)->CheckCollision(missileSphere);

            if (!isHit)
            {
                Vector3 prev = missile->GetPreviousPosition();
                Vector3 curr = missile->GetPosition();
                Vector3 diff = { curr.x - prev.x, curr.y - prev.y, curr.z - prev.z };
                float moveLen = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (moveLen > 0.0001f)
                {
                    Ray moveRay = { prev, { diff.x / moveLen, diff.y / moveLen, diff.z / moveLen } };
                    float hitDist = 0.0f;
                    if ((*it)->CheckRaycast(moveRay, &hitDist))
                    {
                        if (hitDist <= moveLen + 1.0f)
                        {
                            isHit = true;
                        }
                    }
                }
            }

            if (isHit)
            {
                missile->OnCollision();
                (*it)->Kill();

                if (ctx.hitEffect)
                {
                    ctx.hitEffect->SetPosition(missileSphere.center);
                    ctx.hitEffect->Play();
                }
            }
        }

        if ((*it)->IsDead())
        {
            if (ctx.explosionEffect)
            {
                ctx.explosionEffect->SetPosition((*it)->GetPosition());
                ctx.explosionEffect->Play();
            }
            SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.8f);
            it = ctx.obstacles->erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CollisionManager::CheckPlayerVsObstaclesAndTerrain(CollisionContext& ctx)
{
    if (!ctx.player || ctx.player->IsDead()) return;

    OBB playerOBB = ctx.player->GetWorldOBB();

    // 1. 障害物とのメッシュコリジョン
    if (ctx.obstacles)
    {
        for (auto& obs : *ctx.obstacles)
        {
            if (!obs || obs->IsDead()) continue;

            CollisionResult colRes;
            std::vector<Triangle> testedTris;
            std::vector<Triangle> hitTris;
            if (obs->CheckCollisionWithOBB(playerOBB, &colRes, &testedTris, &hitTris))
            {
                bool causedDamage = ctx.player->OnTerrainCollision(colRes.normal, colRes.penetrationDepth, ctx.cameraObject, &colRes.hitPoint);
                if (causedDamage && ctx.cameraShakeTimer)
                {
                    *ctx.cameraShakeTimer = 20.0f;
                }
            }

            TerrainCollisionDebugger::GetInstance()->RecordCollisionCheck(
                static_cast<int>(obs->GetMeshTriangleCount()),
                testedTris,
                hitTris,
                colRes.isHit ? &colRes : nullptr
            );
        }
    }

    // 2. 地形・背景メッシュ（terrainModels）とのメッシュコリジョン
    if (ctx.terrainModels)
    {
        for (auto& modelObj : *ctx.terrainModels)
        {
            if (!modelObj) continue;

            CollisionResult colRes;
            std::vector<Triangle> testedTris;
            std::vector<Triangle> hitTris;
            if (modelObj->CheckCollisionWithOBB(playerOBB, &colRes, &testedTris, &hitTris))
            {
                bool causedDamage = ctx.player->OnTerrainCollision(colRes.normal, colRes.penetrationDepth, ctx.cameraObject, &colRes.hitPoint);
                if (causedDamage && ctx.cameraShakeTimer)
                {
                    *ctx.cameraShakeTimer = 20.0f;
                }
            }

            TerrainCollisionDebugger::GetInstance()->RecordCollisionCheck(
                static_cast<int>(modelObj->GetMeshTriangleCount()),
                testedTris,
                hitTris,
                colRes.isHit ? &colRes : nullptr
            );
        }

        // 自機OBBコライダーの色更新
        if (TerrainCollisionDebugger::GetInstance()->IsHit())
        {
            ctx.player->SetColliderColor({ 1.0f, 0.15f, 0.15f, 1.0f }); // 衝突中: 鮮やかな赤色
        }
        else
        {
            ctx.player->SetColliderColor({ 0.0f, 1.0f, 1.0f, 1.0f }); // 安全時: 鮮やかな水色
        }
    }
}

void CollisionManager::CheckPlayerBulletsVsTerrain(CollisionContext& ctx)
{
    if (!ctx.playerBullets || !ctx.terrainModels) return;

    for (auto& bullet : *ctx.playerBullets)
    {
        if (bullet->IsDead()) continue;
        Sphere bulletSphere = { bullet->GetPosition(), 1.0f };
        for (auto& modelObj : *ctx.terrainModels)
        {
            if (!modelObj) continue;
            if (modelObj->CheckCollisionWithSphere(bulletSphere, nullptr))
            {
                bullet->OnCollision();
                if (ctx.hitEffect)
                {
                    ctx.hitEffect->SetPosition(bulletSphere.center);
                    ctx.hitEffect->Play();
                }
                break;
            }
        }
    }
}
