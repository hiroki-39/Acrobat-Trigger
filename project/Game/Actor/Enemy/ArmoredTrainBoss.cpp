#define NOMINMAX
#include "ArmoredTrainBoss.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include "externals/nlohmann/json.hpp"

void ArmoredTrainBoss::Initialize(Object3dCommon* object3dCommon, const Vector3& startPos, uint32_t skyboxTexIndex) {
    object3dCommon_ = object3dCommon;
    skyboxTexIndex_ = skyboxTexIndex;
    position_ = startPos;
    isActive_ = true;
    isDefeated_ = false;
    isCoreExposed_ = false;
    defeatTimer_ = 0.0f;

    // 新モデルのロード
    ModelManager::GetInstance()->LoadModel("boss_Car1.obj");
    ModelManager::GetInstance()->LoadModel("boss_Car2.obj");
    ModelManager::GetInstance()->LoadModel("boss_Car3.obj");
    ModelManager::GetInstance()->LoadModel("boss_Car4.obj");
    ModelManager::GetInstance()->LoadModel("boss_bigTurret.obj");
    ModelManager::GetInstance()->LoadModel("boss_bigTurret_1.obj");
    ModelManager::GetInstance()->LoadModel("boss_turret.obj");
    ModelManager::GetInstance()->LoadModel("boss_MissilePod.obj");
    ModelManager::GetInstance()->LoadModel("collider_cube_enemy.obj");
    ModelManager::GetInstance()->LoadModel("collider_sphere_enemy.obj");

    carriages_.clear();

    // ヘルパー：砲台マウント生成ラムダ
    auto createTurret = [&](TrainCarriage& car, const std::string& tName, const Vector3& lPos, const Vector3& lRot = { 0.0f, 0.0f, 0.0f }) {
        BossTurretMount t;
        t.name = tName;
        t.baseLocalPos = lPos;
        t.baseLocalRot = lRot;
        t.currentRot = lRot;
        t.attackInterval = 85.0f;
        t.attackTimer = static_cast<float>((car.turrets.size() * 23) % 85);
        t.hp = 8;
        t.maxHp = 8;
        t.colliderRadius = 2.4f;

        t.object = std::make_unique<Object3d>();
        t.object->Initialize(object3dCommon_);
        t.object->SetModel("boss_turret.obj");
        t.object->SetParent(car.object.get());
        t.object->SetTranslate(lPos);
        t.object->SetRotation(lRot);
        t.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
        t.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        t.colliderObject = std::make_unique<Object3d>();
        t.colliderObject->Initialize(object3dCommon_);
        t.colliderObject->SetModel("collider_sphere_enemy.obj");
        t.colliderObject->GetModel()->SetColor({ 1.0f, 0.45f, 0.0f, 1.0f }); // 砲台コライダー: オレンジ
        t.colliderObject->SetParent(t.object.get());
        t.colliderObject->SetTranslate({ 0.0f, 0.8f, 0.0f });
        t.colliderObject->SetScale({ t.colliderRadius * 2.0f, t.colliderRadius * 2.0f, t.colliderRadius * 2.0f });

        car.turrets.push_back(std::move(t));
    };

    // ヘルパー：ミサイルポッド生成ラムダ
    auto createMissilePod = [&](TrainCarriage& car, const std::string& pName, const Vector3& lPos, const Vector3& lRot = { 0.0f, 0.0f, 0.0f }) {
        BossMissilePodMount p;
        p.name = pName;
        p.baseLocalPos = lPos;
        p.baseLocalRot = lRot;
        p.attackInterval = 130.0f;
        p.attackTimer = static_cast<float>((car.missilePods.size() * 30) % 90);
        p.hp = 10;
        p.maxHp = 10;
        p.colliderRadius = 2.4f;

        p.object = std::make_unique<Object3d>();
        p.object->Initialize(object3dCommon_);
        p.object->SetModel("boss_MissilePod.obj");
        p.object->SetParent(car.object.get());
        p.object->SetTranslate(lPos);
        p.object->SetRotation(lRot);
        p.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
        p.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        p.colliderObject = std::make_unique<Object3d>();
        p.colliderObject->Initialize(object3dCommon_);
        p.colliderObject->SetModel("collider_sphere_enemy.obj");
        p.colliderObject->GetModel()->SetColor({ 1.0f, 0.15f, 0.65f, 1.0f }); // ミサイルポッドコライダー: マゼンタ
        p.colliderObject->SetParent(p.object.get());
        p.colliderObject->SetTranslate({ 0.0f, 0.8f, 0.0f });
        p.colliderObject->SetScale({ p.colliderRadius * 2.0f, p.colliderRadius * 2.0f, p.colliderRadius * 2.0f });

        car.missilePods.push_back(std::move(p));
    };

    // 0: 先頭重装甲機関車 (Car1: Locomotive)
    {
        TrainCarriage car;
        car.name = "Locomotive";
        car.displayName = "先頭装甲機関車 (Main Core)";
        car.modelName = "boss_Car1.obj";
        car.baseScale = { 1.0f, 1.0f, 1.0f };
        car.scale = car.baseScale;
        car.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        car.baseLengthOffset = 0.0f;
        car.lengthOffset = 0.0f;
        car.hp = 100;
        car.maxHp = 100;
        car.attackInterval = 100.0f;
        car.collider.type = "BOX";
        car.baseColliderCenter = { 0.0f, 8.5f, 4.5f };
        car.collider.center = car.baseColliderCenter;
        car.baseColliderSize = { 19.0f, 18.0f, 46.0f };
        car.collider.size = car.baseColliderSize;

        car.object = std::make_unique<Object3d>();
        car.object->Initialize(object3dCommon_);
        car.object->SetModel(car.modelName);
        car.object->SetScale(car.scale);
        car.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
        car.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        car.colliderObject = std::make_unique<Object3d>();
        car.colliderObject->Initialize(object3dCommon_);
        car.colliderObject->SetModel("collider_cube_enemy.obj");
        car.colliderObject->SetScale(car.collider.size);

        // 後部キャビン屋根砲台 (2基)
        createTurret(car, "Turret_Car1_L", { -4.5f, 10.5f, -10.0f });
        createTurret(car, "Turret_Car1_R", {  4.5f, 10.5f, -10.0f });

        carriages_.push_back(std::move(car));
    }

    // 1: 垂直ミサイル車 (Car2: MissileCar) ※前方に配置し、ミサイルの飛翔・猶予を目視可能に
    {
        TrainCarriage car;
        car.name = "MissileCar";
        car.displayName = "垂直ミサイルコンテナ車";
        car.modelName = "boss_Car4.obj";
        car.baseScale = { 1.0f, 1.0f, 1.0f };
        car.scale = car.baseScale;
        car.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        car.baseLengthOffset = -38.0f;
        car.lengthOffset = car.baseLengthOffset;
        car.hp = 60;
        car.maxHp = 60;
        car.attackInterval = 120.0f;
        car.collider.type = "BOX";
        car.baseColliderCenter = { 0.0f, 6.0f, 0.0f };
        car.collider.center = car.baseColliderCenter;
        car.baseColliderSize = { 16.0f, 14.0f, 30.0f };
        car.collider.size = car.baseColliderSize;

        car.object = std::make_unique<Object3d>();
        car.object->Initialize(object3dCommon_);
        car.object->SetModel(car.modelName);
        car.object->SetScale(car.scale);
        car.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
        car.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        car.colliderObject = std::make_unique<Object3d>();
        car.colliderObject->Initialize(object3dCommon_);
        car.colliderObject->SetModel("collider_cube_enemy.obj");
        car.colliderObject->SetScale(car.collider.size);

        // ミサイルポッド (4基)
        createMissilePod(car, "MissilePod_1", { -4.5f, 7.5f,  6.0f });
        createMissilePod(car, "MissilePod_2", {  4.5f, 7.5f,  6.0f });
        createMissilePod(car, "MissilePod_3", { -4.5f, 7.5f,  0.0f });
        createMissilePod(car, "MissilePod_4", {  4.5f, 7.5f,  0.0f });

        // 後部迎撃砲台 (2基)
        createTurret(car, "Turret_Car4_B1", { -2.83f, 6.11f, -11.23f });
        createTurret(car, "Turret_Car4_B2", {  2.83f, 6.11f, -11.23f });

        carriages_.push_back(std::move(car));
    }

    // 2: 超巨大主砲車 (Car3: CannonCar)
    {
        TrainCarriage car;
        car.name = "CannonCar";
        car.displayName = "超巨大主砲車";
        car.modelName = "boss_Car3.obj";
        car.baseScale = { 1.0f, 1.0f, 1.0f };
        car.scale = car.baseScale;
        car.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        car.baseLengthOffset = -72.0f;
        car.lengthOffset = car.baseLengthOffset;
        car.hp = 80;
        car.maxHp = 80;
        car.attackInterval = 150.0f;
        car.collider.type = "BOX";
        car.baseColliderCenter = { 0.0f, 8.0f, 0.0f };
        car.collider.center = car.baseColliderCenter;
        car.baseColliderSize = { 16.0f, 16.0f, 30.0f };
        car.collider.size = car.baseColliderSize;

        car.object = std::make_unique<Object3d>();
        car.object->Initialize(object3dCommon_);
        car.object->SetModel(car.modelName);
        car.object->SetScale(car.scale);
        car.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
        car.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        car.colliderObject = std::make_unique<Object3d>();
        car.colliderObject->Initialize(object3dCommon_);
        car.colliderObject->SetModel("collider_cube_enemy.obj");
        car.colliderObject->SetScale(car.collider.size);

        // 超巨大主砲マウント (旋回台座 + 砲身)
        car.bigCannon = std::make_unique<BossBigCannonMount>();
        car.bigCannon->baseLocalPos = { 0.0f, 0.0f, -4.0f };
        car.bigCannon->baseRot = { 0.0f, 0.0f, 0.0f };
        car.bigCannon->barrelRot = { 0.0f, 0.0f, 0.0f };

        // 旋回台座
        car.bigCannon->baseObject = std::make_unique<Object3d>();
        car.bigCannon->baseObject->Initialize(object3dCommon_);
        car.bigCannon->baseObject->SetModel("boss_bigTurret.obj");
        car.bigCannon->baseObject->SetParent(car.object.get());
        car.bigCannon->baseObject->SetTranslate(car.bigCannon->baseLocalPos);
        car.bigCannon->baseObject->SetEnvironmentTextureIndex(skyboxTexIndex_);
        car.bigCannon->baseObject->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        // 砲身 (台座の子オブジェクト)
        car.bigCannon->barrelObject = std::make_unique<Object3d>();
        car.bigCannon->barrelObject->Initialize(object3dCommon_);
        car.bigCannon->barrelObject->SetModel("boss_bigTurret_1.obj");
        car.bigCannon->barrelObject->SetParent(car.bigCannon->baseObject.get());
        car.bigCannon->barrelObject->SetTranslate({ 0.0f, 0.0f, 0.0f });
        car.bigCannon->barrelObject->SetEnvironmentTextureIndex(skyboxTexIndex_);
        car.bigCannon->barrelObject->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        // 前部サポート砲台 (2基)
        createTurret(car, "Turret_Car3_F1", { -3.8f, 3.8f, 10.5f });
        createTurret(car, "Turret_Car3_F2", {  3.8f, 3.8f, 10.5f });

        carriages_.push_back(std::move(car));
    }

    // 3: 多連装砲塔車 (Car4: TurretCar) ※最後尾に配置し、至近距離での迫力ある機銃戦を展開
    {
        TrainCarriage car;
        car.name = "TurretCar";
        car.displayName = "多連装砲塔車";
        car.modelName = "boss_Car2.obj";
        car.baseScale = { 1.0f, 1.0f, 1.0f };
        car.scale = car.baseScale;
        car.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        car.baseLengthOffset = -105.0f;
        car.lengthOffset = car.baseLengthOffset;
        car.hp = 60;
        car.maxHp = 60;
        car.attackInterval = 75.0f;
        car.collider.type = "BOX";
        car.baseColliderCenter = { 0.0f, 6.0f, 0.0f };
        car.collider.center = car.baseColliderCenter;
        car.baseColliderSize = { 16.0f, 14.0f, 30.0f };
        car.collider.size = car.baseColliderSize;

        car.object = std::make_unique<Object3d>();
        car.object->Initialize(object3dCommon_);
        car.object->SetModel(car.modelName);
        car.object->SetScale(car.scale);
        car.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
        car.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

        car.colliderObject = std::make_unique<Object3d>();
        car.colliderObject->Initialize(object3dCommon_);
        car.colliderObject->SetModel("collider_cube_enemy.obj");
        car.colliderObject->SetScale(car.collider.size);

        // 階段状デッキ多連装砲台 (8基)
        createTurret(car, "Turret_Car2_L1", { -5.8f, 3.8f,  7.0f });
        createTurret(car, "Turret_Car2_L2", { -5.8f, 5.5f,  0.0f });
        createTurret(car, "Turret_Car2_L3", { -5.8f, 7.2f, -7.0f });
        createTurret(car, "Turret_Car2_R1", {  5.8f, 3.8f,  7.0f });
        createTurret(car, "Turret_Car2_R2", {  5.8f, 5.5f,  0.0f });
        createTurret(car, "Turret_Car2_R3", {  5.8f, 7.2f, -7.0f });
        createTurret(car, "Turret_Car2_M1", {  0.0f, 10.5f, -3.0f });
        createTurret(car, "Turret_Car2_M2", {  0.0f, 12.5f, -11.0f });

        carriages_.push_back(std::move(car));
    }

    SetScaleMultiplier(scaleMultiplier_);
    LoadSettingsJson();
    UpdateCarriageTransforms();
}

void ArmoredTrainBoss::SetScaleMultiplier(float scale) {
    scaleMultiplier_ = (std::max)(0.2f, scale);
    for (auto& car : carriages_) {
        car.scale = {
            car.baseScale.x * scaleMultiplier_,
            car.baseScale.y * scaleMultiplier_,
            car.baseScale.z * scaleMultiplier_
        };
        car.lengthOffset = car.baseLengthOffset * scaleMultiplier_;
        car.collider.center = {
            car.baseColliderCenter.x * scaleMultiplier_,
            car.baseColliderCenter.y * scaleMultiplier_,
            car.baseColliderCenter.z * scaleMultiplier_
        };
        car.collider.size = {
            car.baseColliderSize.x * scaleMultiplier_,
            car.baseColliderSize.y * scaleMultiplier_,
            car.baseColliderSize.z * scaleMultiplier_
        };
        if (car.object) {
            car.object->SetScale(car.scale);
        }
        if (car.colliderObject) {
            car.colliderObject->SetScale(car.collider.size);
        }
    }
    UpdateCarriageTransforms();
}

void ArmoredTrainBoss::ResetChaseState() {
    isCloseToPlayer_ = false;
    isOvertaking_ = false;
    currentSpeed_ = 50.0f;
    dynamicLeadDistance_ = desiredLeadDistance_;
    targetDynamicLeadDistance_ = desiredLeadDistance_;
    isShortDistanceOpportunity_ = false;
    leadDistanceChangeTimer_ = 3.5f;
}

void ArmoredTrainBoss::Update(const Vector3& cameraPos, const Vector3& playerForward, float playerSpeed, Player* player, std::list<std::unique_ptr<EnemyBullet>>& enemyBullets, float gameSpeed) {
    if (!isActive_) return;

    // 撃破演出中
    if (isDefeated_) {
        defeatTimer_ += gameSpeed;
        currentSpeed_ = (std::max)(0.0f, currentSpeed_ - 0.8f * gameSpeed);
        if (rail_) {
            float totalLen = (std::max)(1.0f, rail_->GetTotalLength());
            float deltaProg = (currentSpeed_ * (gameSpeed / 60.0f)) / totalLen;
            railProgress_ = (std::min)(1.0f, railProgress_ + deltaProg);
            position_ = rail_->GetPosition(railProgress_);
        } else {
            position_.z -= currentSpeed_ * (gameSpeed / 60.0f);
        }
        UpdateCarriageTransforms();
        return;
    }

    // レール走行・プレイヤー連動チェイス制御
    if (rail_) {
        // 緩やかな車線変更による側面露出（約16秒周期で左並走 -12m 〜 右並走 +12m へゆったりシフト）
        if (isDynamicLaneChange_) {
            laneChangeTimer_ += (gameSpeed / 60.0f) * 0.35f;
            laneOffset_ = std::sin(laneChangeTimer_) * 12.0f;
        }

        if (isFollowPlayer_) {
            // プレイヤー位置と向き
            Vector3 playerPos = player ? player->GetWorldPosition() : cameraPos;
            Vector3 toBoss = { position_.x - playerPos.x, position_.y - playerPos.y, position_.z - playerPos.z };
            float straightDist = std::sqrt(toBoss.x * toBoss.x + toBoss.y * toBoss.y + toBoss.z * toBoss.z);

            // 自機前進方向におけるボスの前後相対距離 (内積)
            // 正: ボスが自機の前方にいる / 負: 自機がボスの前にいる(追い抜いた)
            relativeForwardDist_ = toBoss.x * playerForward.x + toBoss.y * playerForward.y + toBoss.z * playerForward.z;

            // 列車の全長（最後尾車両のオフセット + 尾部マージン）
            float trainTotalLength = 115.0f;
            if (!carriages_.empty()) {
                trainTotalLength = std::abs(carriages_.back().lengthOffset) + 20.0f;
            }

            // 至近距離での接触回避下限（最後尾が自機前方12m未満にはめり込まない）
            float minSafeLeadDistance = trainTotalLength + 12.0f;

            // ランダム並走距離の短縮サイクル（接近スリルと通常戦闘のメリハリ）
            if (isCloseToPlayer_ && !isOvertaking_ && isRandomLeadEnabled_) {
                float dtSec = gameSpeed / 60.0f;
                leadDistanceChangeTimer_ -= dtSec;
                if (leadDistanceChangeTimer_ <= 0.0f) {
                    if (!isShortDistanceOpportunity_) {
                        // 基準距離 -> 急接近アタックチャンス！（最後尾が自機前方15m〜18mの超至近距離まで迫る大迫力！）
                        isShortDistanceOpportunity_ = true;
                        float randFactor = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                        targetDynamicLeadDistance_ = trainTotalLength + 15.0f + randFactor * 4.0f;
                        leadDistanceChangeTimer_ = 3.5f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f;
                        // 急接近の予兆警告音（迫り来る巨体！）
                        // SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.65f);
                    } else {
                        // 接近終了 -> 基準距離へ復帰（最後尾が自機前方30m〜36mの中距離へ戻る）
                        isShortDistanceOpportunity_ = false;
                        float randOffset = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) - 0.5f) * 6.0f;
                        targetDynamicLeadDistance_ = trainTotalLength + 32.0f + randOffset;
                        leadDistanceChangeTimer_ = 4.5f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.5f;
                    }
                }
            } else if (!isRandomLeadEnabled_) {
                targetDynamicLeadDistance_ = (std::max)(minSafeLeadDistance, desiredLeadDistance_);
            }

            // 安全マージンを下回らないよう最終ガード
            if (targetDynamicLeadDistance_ < minSafeLeadDistance) {
                targetDynamicLeadDistance_ = minSafeLeadDistance;
            }

            // 目標距離を滑らかに補間
            float distLerpRate = 0.045f * gameSpeed;
            dynamicLeadDistance_ += (targetDynamicLeadDistance_ - dynamicLeadDistance_) * distLerpRate;

            // --- 3段階のチェイス挙動判定 ---
            if (!isCloseToPlayer_) {
                // 【フェーズ1: 出現〜接近】
                // 出現位置が遠いため、プレイヤーに追いつくまで高速で疾走
                targetSpeed_ = playerSpeed + rushSpeedBonus_ + 25.0f;

                // プレイヤーの近場（自機前方 desiredLeadDistance_ 付近）に到達したら並走へ移行
                if (relativeForwardDist_ >= (trainTotalLength + 25.0f) && straightDist < (trainTotalLength + 90.0f)) {
                    isCloseToPlayer_ = true;
                    leadDistanceChangeTimer_ = 3.0f;
                }
            } else {
                // 【フェーズ3: 追い抜き判定】
                // プレイヤーがボスの最後尾よりも前に躍り出た場合
                if (relativeForwardDist_ < trainTotalLength) {
                    isOvertaking_ = true;
                } else if (relativeForwardDist_ >= (dynamicLeadDistance_ + 5.0f)) {
                    isOvertaking_ = false;
                }

                if (isOvertaking_) {
                    // 自機より前に出るため進行スピードを急上昇させて追い抜く
                    targetSpeed_ = playerSpeed + rushSpeedBonus_;
                } else {
                    // 【フェーズ2: 並走（シンクロ巡航）】
                    // プレイヤーの近場で自機速度と同程度で並走
                    // dynamicLeadDistance_（基準70mまたはランダム短縮距離）との前後ズレに応じて速度を微調整
                    float distDiff = relativeForwardDist_ - dynamicLeadDistance_;
                    targetSpeed_ = playerSpeed - distDiff * 0.8f;
                    // 急停止しないよう下限を設定 (自機速度の50%以上、かつ最低20m/s)
                    float minSpeed = (std::max)(20.0f, playerSpeed * 0.5f);
                    if (targetSpeed_ < minSpeed) {
                        targetSpeed_ = minSpeed;
                    }
                }
            }

            // 加減速のスムージング（急激な速度変化を抑えて自然な列車走行感を出す）
            float accelRate = (targetSpeed_ > currentSpeed_) ? (2.5f * gameSpeed) : (1.8f * gameSpeed);
            if (currentSpeed_ < targetSpeed_) {
                currentSpeed_ = (std::min)(targetSpeed_, currentSpeed_ + accelRate);
            } else {
                currentSpeed_ = (std::max)(targetSpeed_, currentSpeed_ - accelRate);
            }

            // 実速度(m/s)に基づいて進行度を加算（物理的に瞬間移動せず連続走行）
            float totalLen = (std::max)(1.0f, rail_->GetTotalLength());
            float deltaProg = (currentSpeed_ * (gameSpeed / 60.0f)) / totalLen;
            railProgress_ = (std::clamp)(railProgress_ + deltaProg, 0.0f, 1.0f);

        } else {
            // 固定速度走行モード
            float totalLen = (std::max)(1.0f, rail_->GetTotalLength());
            float deltaProg = (currentSpeed_ * (gameSpeed / 60.0f)) / totalLen;
            railProgress_ = (std::clamp)(railProgress_ + deltaProg, 0.0f, 1.0f);
        }
    } else {
        // レールなし時の直進
        position_.z -= currentSpeed_ * (gameSpeed / 60.0f);
    }

    // 各車両の位置・向き（坂道の斜面傾斜・カーブ）を個別に更新
    UpdateCarriageTransforms();

    // 各砲台の自立旋回（左右90度の射角制限）
    if (player) {
        Vector3 playerPos = player->GetWorldPosition();

        auto TransformPoint = [](const Vector3& p, const Matrix4x4& m) -> Vector3 {
            return {
                p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
                p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
                p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
            };
        };

        for (auto& car : carriages_) {
            if (car.isDestroyed || !car.object) continue;

            const Matrix4x4& carWorldMat = car.object->GetmatWorld();

            for (auto& turret : car.turrets) {
                if (turret.isDestroyed || !turret.object) continue;

                // 砲台マウント位置の正確なワールド座標
                Vector3 turretWorldPos = TransformPoint(turret.baseLocalPos, carWorldMat);

                // 砲台からプレイヤーへのワールド方向ベクトル (X-Z平面)
                Vector3 toPWorld = { playerPos.x - turretWorldPos.x, 0.0f, playerPos.z - turretWorldPos.z };
                float distXZ = std::sqrt(toPWorld.x * toPWorld.x + toPWorld.z * toPWorld.z);

                if (distXZ > 0.01f) {
                    toPWorld = { toPWorld.x / distXZ, 0.0f, toPWorld.z / distXZ };

                    // 親車両の向き（Y回転）の逆回転を使って、車両ローカル空間におけるプレイヤー方向を算出
                    float carYaw = car.rotation.y;
                    float cosCar = std::cos(-carYaw);
                    float sinCar = std::sin(-carYaw);
                    // DirectX左手系行ベクトル変換での逆回転
                    Vector3 toPInCar = {
                        toPWorld.x * cosCar - toPWorld.z * sinCar,
                        0.0f,
                        toPWorld.x * sinCar + toPWorld.z * cosCar
                    };

                    // 砲台モデルは初期状態で +X 軸を向いているため、
                    // 車両ローカル空間で (toPInCar.x, toPInCar.z) を向くための目標ヨー角は:
                    // x = cos(yaw), z = -sin(yaw) => yaw = atan2(-z, x)
                    float targetLocalRotY = std::atan2(-toPInCar.z, toPInCar.x);

                    // 基準正面（baseLocalRot.y）からの角度差分を算出
                    float angleDiff = targetLocalRotY - turret.baseLocalRot.y;
                    while (angleDiff > 3.14159265f) angleDiff -= 6.2831853f;
                    while (angleDiff < -3.14159265f) angleDiff += 6.2831853f;

                    // 左右90度（±π/2）以内かチェック！
                    const float kMaxHalfAngle = 3.14159265f * 0.5f; // 90度
                    if (std::abs(angleDiff) <= kMaxHalfAngle) {
                        turret.canShoot = true;

                        // 目標ヨー角へ滑らかに旋回追従
                        float rotSpeed = 0.12f * gameSpeed;
                        float diff = targetLocalRotY - turret.currentRot.y;
                        while (diff > 3.14159265f) diff -= 6.2831853f;
                        while (diff < -3.14159265f) diff += 6.2831853f;
                        turret.currentRot.y += diff * rotSpeed;
                    } else {
                        // 射角外（90度を超える死角）：撃てない！初期正面に戻る
                        turret.canShoot = false;
                        float rotSpeed = 0.06f * gameSpeed;
                        float diff = turret.baseLocalRot.y - turret.currentRot.y;
                        while (diff > 3.14159265f) diff -= 6.2831853f;
                        while (diff < -3.14159265f) diff += 6.2831853f;
                        turret.currentRot.y += diff * rotSpeed;
                    }

                    turret.object->SetRotation(turret.currentRot);
                    turret.object->Update();
                }
            }
        }
    }

    // コア露出判定（護衛車両が2両以上撃破されたら先頭機関車の装甲がパージ）
    int destroyedCount = 0;
    for (size_t i = 1; i < carriages_.size(); ++i) {
        if (carriages_[i].isDestroyed) destroyedCount++;
    }
    if (destroyedCount >= 2 && !isCoreExposed_) {
        isCoreExposed_ = true;
        // 機関車の発光カラーを赤熱化
        carriages_[0].color = { 1.0f, 0.25f, 0.2f, 1.0f };
        if (carriages_[0].object) {
            carriages_[0].object->SetColor(carriages_[0].color);
        }
    }

    // 各車両の武装攻撃
    ExecuteAttacks(player, enemyBullets, gameSpeed);
}

bool ArmoredTrainBoss::GetGroundHeight(float x, float z, float startY, float* outGroundY, Vector3* outNormal) const {
    if (!terrainObjects_ || terrainObjects_->empty()) return false;

    float bestY = -1e9f;
    Vector3 bestNormal = { 0.0f, 1.0f, 0.0f };
    bool found = false;

    for (const auto& obj : *terrainObjects_) {
        if (!obj || !obj->IsCollisionEnabled()) continue;
        float gy = 0.0f;
        Vector3 norm;
        if (obj->RaycastDown(x, z, startY, &gy, &norm)) {
            if (gy > bestY) {
                bestY = gy;
                bestNormal = norm;
                found = true;
            }
        }
    }

    if (found) {
        if (outGroundY) *outGroundY = bestY;
        if (outNormal) *outNormal = bestNormal;
        return true;
    }
    return false;
}

void ArmoredTrainBoss::UpdateCarriageTransforms() {
    float totalLen = rail_ ? (std::max)(1.0f, rail_->GetTotalLength()) : 1.0f;

    for (size_t i = 0; i < carriages_.size(); ++i) {
        auto& car = carriages_[i];

        if (rail_ && rail_->IsValid()) {
            // 各車両のレール上進行度（先頭進行度 + lengthOffset(負の値) / totalLen）
            float carProgress = railProgress_ + (car.lengthOffset / totalLen);

            Vector3 baseRailPos = { 0.0f, 0.0f, 0.0f };
            Vector3 forward = { 0.0f, 0.0f, -1.0f };

            if (carProgress >= 0.0f && carProgress <= 1.0f) {
                // レール上の正規位置
                baseRailPos = rail_->GetPosition(carProgress);
                forward = rail_->GetForward(carProgress);
                car.rotation.z = rail_->GetTilt(carProgress); // カント（ロール）
            } else if (carProgress < 0.0f) {
                // レール始点の手前にいる場合：始点の接線ベクトルを逆方向に伸ばして自然に配置
                Vector3 startPos = rail_->GetPosition(0.0f);
                forward = rail_->GetForward(0.0f);
                float distBehind = -carProgress * totalLen;
                baseRailPos = {
                    startPos.x - forward.x * distBehind,
                    startPos.y - forward.y * distBehind,
                    startPos.z - forward.z * distBehind
                };
                car.rotation.z = rail_->GetTilt(0.0f);
            } else {
                // レール終点を超えた場合：終点の接線ベクトル方向に自然に延長
                Vector3 endPos = rail_->GetPosition(1.0f);
                forward = rail_->GetForward(1.0f);
                float distAhead = (carProgress - 1.0f) * totalLen;
                baseRailPos = {
                    endPos.x + forward.x * distAhead,
                    endPos.y + forward.y * distAhead,
                    endPos.z + forward.z * distAhead
                };
                car.rotation.z = rail_->GetTilt(1.0f);
            }

            // 前進方向からピッチ・ヨー角を算出
            float fLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
            if (fLen > 0.0001f) {
                car.rotation.y = std::atan2(forward.x, forward.z);
                car.rotation.x = std::atan2(-forward.y, fLen); // 坂道の傾斜角（ピッチ）
            }

            // レール進行方向の右法線ベクトルを算出（並走レーンオフセット用）
            Vector3 right = (fLen > 0.0001f) ? Vector3(forward.z / fLen, 0.0f, -forward.x / fLen) : Vector3(1.0f, 0.0f, 0.0f);

            // 並走オフセットを適用（正面を塞がず、全車両の側面をズラリと露出させる）
            car.position = {
                baseRailPos.x + right.x * laneOffset_,
                baseRailPos.y,
                baseRailPos.z + right.z * laneOffset_
            };
        } else {
            // レールなし時のフォールバック（直線追従）
            float cy = std::cos(rotation_.y);
            float sy = std::sin(rotation_.y);
            car.position = {
                position_.x + (sy * car.lengthOffset),
                position_.y,
                position_.z + (cy * car.lengthOffset)
            };
            car.rotation = rotation_;
        }

        // 地面メッシュへの自動吸着（地面レイキャスト）
        // ※毎フレーム全車両で実行すると数十万ポリゴンの走査でFPSが激落するため、
        //   車両ごとにフレーム分散（1フレームあたり1車両のみ走査）＋高さキャッシュ補間を行いCPU負荷を劇的カット
        if (isGroundSnapEnabled_ && terrainObjects_ && !carriages_.empty()) {
            bool shouldCheck = !car.hasGroundHeight || ((groundCheckFrame_ % carriages_.size()) == i);
            if (shouldCheck) {
                float groundY = 0.0f;
                Vector3 groundNorm;
                float searchStartY = (std::max)(car.position.y + 60.0f, 150.0f);
                if (GetGroundHeight(car.position.x, car.position.z, searchStartY, &groundY, &groundNorm)) {
                    car.cachedGroundY = groundY;
                    car.hasGroundHeight = true;
                }
            }

            if (car.hasGroundHeight) {
                // 車両の車輪接地面 (Y = -1.75f) が地面高さに密着するよう配置
                car.position.y = car.cachedGroundY + (1.75f * scaleMultiplier_) + groundSnapOffset_;
            }
        }

        // 先頭車両（i == 0）の位置と向きをボスの代表値として同期
        if (i == 0) {
            position_ = car.position;
            rotation_ = car.rotation;
        }

        // 3Dオブジェクトとコライダーの更新
        if (car.object) {
            car.object->SetTranslate(car.position);
            car.object->SetRotation(car.rotation);
            car.object->Update();
        }

        if (car.colliderObject) {
            car.colliderObject->SetTranslate(car.position);
            car.colliderObject->SetRotation(car.rotation);
            car.colliderObject->Update();
        }

        // 子パーツ群（砲台・ミサイルポッド・主砲）のトランスフォーム更新
        for (auto& turret : car.turrets) {
            if (turret.object) {
                turret.object->SetTranslate(turret.baseLocalPos);
                turret.object->SetRotation(turret.currentRot);
                turret.object->Update();
            }
        }

        for (auto& pod : car.missilePods) {
            if (pod.object) {
                pod.object->SetTranslate(pod.baseLocalPos);
                pod.object->SetRotation(pod.baseLocalRot);
                pod.object->Update();
            }
        }

        if (car.bigCannon) {
            if (car.bigCannon->baseObject) {
                car.bigCannon->baseObject->SetTranslate(car.bigCannon->baseLocalPos);
                car.bigCannon->baseObject->SetRotation(car.bigCannon->baseRot);
                car.bigCannon->baseObject->Update();
            }
            if (car.bigCannon->barrelObject) {
                car.bigCannon->barrelObject->SetTranslate({ 0.0f, 0.0f, car.bigCannon->recoilZ });
                car.bigCannon->barrelObject->SetRotation(car.bigCannon->barrelRot);
                car.bigCannon->barrelObject->Update();
            }
        }
    }
    groundCheckFrame_++;
}

void ArmoredTrainBoss::ExecuteAttacks(Player* player, std::list<std::unique_ptr<EnemyBullet>>& enemyBullets, float gameSpeed) {
    if (!player || isDefeated_) return;
    Vector3 playerPos = player->GetWorldPosition();

    // 行ベクトル変換ヘルパー (DirectX形式: v * M)
    auto TransformPoint = [](const Vector3& p, const Matrix4x4& m) -> Vector3 {
        return {
            p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
            p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
            p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2]
        };
    };
    auto TransformVector = [](const Vector3& v, const Matrix4x4& m) -> Vector3 {
        return {
            v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
            v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
            v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
        };
    };

    for (size_t carIdx = 0; carIdx < carriages_.size(); ++carIdx) {
        auto& car = carriages_[carIdx];
        if (car.isDestroyed) continue;

        // ----------------------------------------------------
        // 1. 砲台の攻撃処理（射角制限 左右90度以内時のみ先端2門から発射）
        // ----------------------------------------------------
        for (auto& turret : car.turrets) {
            if (turret.isDestroyed || !turret.object) continue;

            if (turret.canShoot) {
                turret.attackTimer += gameSpeed;
                if (turret.attackTimer >= turret.attackInterval) {
                    turret.attackTimer = 0.0f;

                    const Matrix4x4& tMat = turret.object->GetmatWorld();

                    // 砲台の前方向ベクトル（+X方向）
                    Vector3 fireDir = TransformVector({ 1.0f, 0.0f, 0.0f }, tMat);
                    float dirLen = fireDir.Length();
                    if (dirLen > 0.0001f) {
                        fireDir = fireDir / dirLen;
                    } else {
                        fireDir = { 0.0f, 0.0f, -1.0f };
                    }
                    Vector3 bulletVel = fireDir * 2.5f;

                    // 2連装砲身の先端オフセット (X=5.10m, Y=0.325m, Z=±0.67m)
                    Vector3 leftTipLocal  = { 5.10f, 0.325f, -0.67f };
                    Vector3 rightTipLocal = { 5.10f, 0.325f,  0.67f };

                    Vector3 leftTipWorld  = TransformPoint(leftTipLocal, tMat);
                    Vector3 rightTipWorld = TransformPoint(rightTipLocal, tMat);

                    // プレイヤーの方向へ向けて正確に発射
                    Vector3 toTargetL = { playerPos.x - leftTipWorld.x, playerPos.y - leftTipWorld.y, playerPos.z - leftTipWorld.z };
                    Vector3 toTargetR = { playerPos.x - rightTipWorld.x, playerPos.y - rightTipWorld.y, playerPos.z - rightTipWorld.z };

                    float lenL = toTargetL.Length();
                    Vector3 bulletVelL = (lenL > 0.001f) ? (toTargetL / lenL) * 2.5f : fireDir * 2.5f;

                    float lenR = toTargetR.Length();
                    Vector3 bulletVelR = (lenR > 0.001f) ? (toTargetR / lenR) * 2.5f : fireDir * 2.5f;

                    // 左砲身から発射
                    auto bulletL = std::make_unique<EnemyBullet>();
                    bulletL->Initialize(object3dCommon_, leftTipWorld, bulletVelL, false, player);
                    enemyBullets.push_back(std::move(bulletL));

                    // 右砲身から発射
                    auto bulletR = std::make_unique<EnemyBullet>();
                    bulletR->Initialize(object3dCommon_, rightTipWorld, bulletVelR, false, player);
                    enemyBullets.push_back(std::move(bulletR));

                    // 発射SE
                    // SoundManager::GetInstance()->PlaySE("launcher.mp3", 0.35f);
                }
            } else {
                // 射角外（撃てない死角）の時はタイマー進行を抑止
                if (turret.attackTimer > turret.attackInterval * 0.5f) {
                    turret.attackTimer = turret.attackInterval * 0.5f;
                }
            }
        }

        // ----------------------------------------------------
        // 2. ミサイルポッドの攻撃処理（ミサイルポッド搭載車両：ロックオン予告後に上空へ連続発射）
        // ----------------------------------------------------
        if (!car.missilePods.empty()) {
        // ----------------------------------------------------
        // 2. ミサイル車の垂直ミサイル一斉攻撃シーケンス（6発発射 ➔ 全滅待機 ➔ 再ロックオン）
        // ----------------------------------------------------
        if (!car.missilePods.empty()) {
            // 生存しているポッドを収集
            std::vector<size_t> alivePodIndices;
            for (size_t podIdx = 0; podIdx < car.missilePods.size(); ++podIdx) {
                if (!car.missilePods[podIdx].isDestroyed && car.missilePods[podIdx].object) {
                    alivePodIndices.push_back(podIdx);
                }
            }

            if (!alivePodIndices.empty()) {
                // フィールド上の残存敵ミサイル数をカウント
                int activeMissiles = 0;
                for (const auto& b : enemyBullets) {
                    if (b && !b->IsDead() && b->IsMissile()) {
                        activeMissiles++;
                    }
                }

                switch (missileSeqState_) {
                case BossMissileSequence::IDLE:
                    // 全ミサイル消滅後の待機インターバル（約3秒）
                    missileSeqTimer_ += gameSpeed;
                    if (missileSeqTimer_ >= missileIdleInterval_) {
                        missileSeqState_ = BossMissileSequence::LOCK_ON;
                        missileSeqTimer_ = 0.0f;
                    }
                    break;

                case BossMissileSequence::LOCK_ON:
                    // ロックオンチャージ中（約2.6秒間じっくりHUD警告表示）
                    missileSeqTimer_ += gameSpeed;
                    if (missileSeqTimer_ >= missileLockOnDuration_) {
                        // ロックオン確定！6発連続発射へ移行
                        missileSeqState_ = BossMissileSequence::FIRING;
                        missileSeqTimer_ = 0.0f;
                        missileFiredCount_ = 0;
                        missileFireIntervalTimer_ = 0.0f; // 即座に1発目を発射
                    }
                    break;

                case BossMissileSequence::FIRING:
                    missileFireIntervalTimer_ -= gameSpeed;
                    if (missileFireIntervalTimer_ <= 0.0f && missileFiredCount_ < 6) {
                        // 生きているポッドから順番に選定して発射
                        size_t chosenPodIdx = alivePodIndices[missileFiredCount_ % alivePodIndices.size()];
                        auto& pod = car.missilePods[chosenPodIdx];
                        const Matrix4x4& pMat = pod.object->GetmatWorld();

                        Vector3 spawnLocal = { 0.0f, 1.6f, 0.0f };
                        Vector3 spawnWorld = TransformPoint(spawnLocal, pMat);

                        // 列車の前進ベクトル（慣性）を取得
                        const Matrix4x4& cMat = car.object ? car.object->GetmatWorld() : pMat;
                        Vector3 trainForward = TransformVector({ 0.0f, 0.0f, 1.0f }, cMat);
                        float fLen = trainForward.Length();
                        if (fLen > 0.001f) {
                            trainForward = trainForward / fLen;
                        } else {
                            trainForward = { 0.0f, 0.0f, -1.0f };
                        }
                        Vector3 trainInertia = trainForward * (currentSpeed_ / 60.0f);

                        // 打ち上げ初速（慣性に加え、上空へ素早く舞い上がる力強い推進力）
                        Vector3 launchVel = {
                            trainInertia.x,
                            3.0f,
                            trainInertia.z
                        };

                        auto missile = std::make_unique<EnemyBullet>();
                        // salvoIndex (0〜5) を渡して、プレイヤー正面視界内の綺麗な扇状整列を実現
                        missile->InitializeMissile(object3dCommon_, spawnWorld, launchVel, player, missileFiredCount_);
                        enemyBullets.push_back(std::move(missile));

                        missileFiredCount_++;
                        missileFireIntervalTimer_ = 11.0f; // 11フレーム（約0.18秒）ごとにリズミカルに発射！

                        if (missileFiredCount_ >= 6) {
                            // きっちり6本撃ち切り！フィールド上の全滅監視へ移行
                            missileSeqState_ = BossMissileSequence::WAIT_ALL_CLEAR;
                            missileSeqTimer_ = 0.0f;
                        }
                    }
                    break;

                case BossMissileSequence::WAIT_ALL_CLEAR:
                    // 発射した6発が「全て消えるまで」絶対に次のロックオンを開始しない！
                    if (activeMissiles == 0) {
                        // 全てのミサイルが撃墜または消滅した！待機インターバルへ移行
                        missileSeqState_ = BossMissileSequence::IDLE;
                        missileSeqTimer_ = 0.0f;
                    }
                    break;
                }
            } else {
                // 全ポッド破壊時はシーケンス停止
                missileSeqState_ = BossMissileSequence::IDLE;
                missileSeqTimer_ = 0.0f;
            }
        }
        }

        // ----------------------------------------------------
        // 3. 先頭機関車（コア露出時）の激しい前方向ビーム連射
        // ----------------------------------------------------
        if (carIdx == 0 && isCoreExposed_) {
            car.attackTimer += gameSpeed;
            if (car.attackTimer >= 100.0f) {
                car.attackTimer = 0.0f;
                Vector3 carPos = car.object ? car.object->GetTranslate() : position_;
                Vector3 toPlayer = { playerPos.x - carPos.x, playerPos.y - carPos.y, playerPos.z - carPos.z };
                float len = toPlayer.Length();
                if (len > 0.001f) toPlayer = toPlayer.Normalize();

                for (float spread = -0.2f; spread <= 0.21f; spread += 0.2f) {
                    auto bullet = std::make_unique<EnemyBullet>();
                    Vector3 spawnP = { carPos.x, carPos.y + (5.0f * scaleMultiplier_), carPos.z + (12.0f * scaleMultiplier_) };
                    Vector3 vel = { (toPlayer.x + spread) * 2.6f, toPlayer.y * 2.6f, toPlayer.z * 2.6f };
                    bullet->Initialize(object3dCommon_, spawnP, vel, false, player);
                    enemyBullets.push_back(std::move(bullet));
                }
            }
        }
    }
}

void ArmoredTrainBoss::Draw() {
    if (!isActive_) return;
    for (auto& car : carriages_) {
        if (car.object) {
            car.object->Draw();
        }
        for (auto& turret : car.turrets) {
            if (turret.object) {
                turret.object->Draw();
            }
        }
        for (auto& pod : car.missilePods) {
            if (pod.object) {
                pod.object->Draw();
            }
        }
        if (car.bigCannon) {
            if (car.bigCannon->baseObject) {
                car.bigCannon->baseObject->Draw();
            }
            if (car.bigCannon->barrelObject) {
                car.bigCannon->barrelObject->Draw();
            }
        }
    }
}

void ArmoredTrainBoss::DrawCollider() {
    if (!isActive_ || isDefeated_) return;
    for (auto& car : carriages_) {
        if (!car.isDestroyed) {
            if (car.colliderObject) {
                car.colliderObject->Draw();
            }
            for (auto& turret : car.turrets) {
                if (!turret.isDestroyed && turret.colliderObject) {
                    turret.colliderObject->Update();
                    turret.colliderObject->Draw();
                }
            }
            for (auto& pod : car.missilePods) {
                if (!pod.isDestroyed && pod.colliderObject) {
                    pod.colliderObject->Update();
                    pod.colliderObject->Draw();
                }
            }
        }
    }
}

bool ArmoredTrainBoss::CheckCollision(const Sphere& bulletSphere, int* outCarriageIndex, BossPartType* outPartType, int* outPartIndex, Vector3* outHitPos) {
    if (!isActive_ || isDefeated_) return false;

    // 1. 各車両の武装パーツ（砲台・ミサイルポッド）の球判定を優先チェック（部位破壊）
    for (int i = 0; i < static_cast<int>(carriages_.size()); ++i) {
        auto& car = carriages_[i];
        if (car.isDestroyed) continue;

        // 砲台チェック
        for (int tIdx = 0; tIdx < static_cast<int>(car.turrets.size()); ++tIdx) {
            auto& turret = car.turrets[tIdx];
            if (turret.isDestroyed || !turret.object) continue;

            const Matrix4x4& mat = turret.object->GetmatWorld();
            Vector3 tWorldPos = { mat.m[3][0], mat.m[3][1] + 0.8f, mat.m[3][2] };
            Sphere tSphere = { tWorldPos, turret.colliderRadius };

            if (CollisionMath::IsCollision(bulletSphere, tSphere)) {
                if (outCarriageIndex) *outCarriageIndex = i;
                if (outPartType) *outPartType = BossPartType::Turret;
                if (outPartIndex) *outPartIndex = tIdx;
                if (outHitPos) *outHitPos = tWorldPos;
                return true;
            }
        }

        // ミサイルポッドチェック
        for (int pIdx = 0; pIdx < static_cast<int>(car.missilePods.size()); ++pIdx) {
            auto& pod = car.missilePods[pIdx];
            if (pod.isDestroyed || !pod.object) continue;

            const Matrix4x4& mat = pod.object->GetmatWorld();
            Vector3 pWorldPos = { mat.m[3][0], mat.m[3][1] + 0.8f, mat.m[3][2] };
            Sphere pSphere = { pWorldPos, pod.colliderRadius };

            if (CollisionMath::IsCollision(bulletSphere, pSphere)) {
                if (outCarriageIndex) *outCarriageIndex = i;
                if (outPartType) *outPartType = BossPartType::MissilePod;
                if (outPartIndex) *outPartIndex = pIdx;
                if (outHitPos) *outHitPos = pWorldPos;
                return true;
            }
        }
    }

    // 2. 車両本体のOBB判定
    for (int i = 0; i < static_cast<int>(carriages_.size()); ++i) {
        auto& car = carriages_[i];
        if (car.isDestroyed) continue;

        Vector3 carPos = car.object ? car.object->GetTranslate() : car.position;
        Matrix4x4 rotMat = Matrix4x4::MakeAffine({ 1.0f, 1.0f, 1.0f }, car.rotation, { 0.0f, 0.0f, 0.0f });
        Vector3 colCenter = carPos + (rotMat * car.collider.center);

        OBB enemyOBB = CollisionMath::CreateOBB(colCenter, car.collider.size, rotMat);
        if (CollisionMath::IsCollision(bulletSphere, enemyOBB)) {
            if (outCarriageIndex) *outCarriageIndex = i;
            if (outPartType) *outPartType = BossPartType::Carriage;
            if (outPartIndex) *outPartIndex = -1;
            if (outHitPos) *outHitPos = colCenter;
            return true;
        }
    }
    return false;
}

bool ArmoredTrainBoss::CheckRaycast(const Ray& ray, float* outDist, int* outCarriageIndex, BossPartType* outPartType, int* outPartIndex) {
    if (!isActive_ || isDefeated_) return false;

    float nearestDist = 99999.0f;
    int hitIdx = -1;
    BossPartType hitType = BossPartType::Carriage;
    int hitPartIdx = -1;

    // 1. 各車両の武装パーツ（砲台・ミサイルポッド）
    for (int i = 0; i < static_cast<int>(carriages_.size()); ++i) {
        auto& car = carriages_[i];
        if (car.isDestroyed) continue;

        for (int tIdx = 0; tIdx < static_cast<int>(car.turrets.size()); ++tIdx) {
            auto& turret = car.turrets[tIdx];
            if (turret.isDestroyed || !turret.object) continue;
            const Matrix4x4& mat = turret.object->GetmatWorld();
            Vector3 tWorldPos = { mat.m[3][0], mat.m[3][1] + 0.8f, mat.m[3][2] };
            Sphere tSphere = { tWorldPos, turret.colliderRadius };
            float dist = 0.0f;
            if (CollisionMath::Raycast(ray, tSphere, &dist)) {
                if (dist < nearestDist) {
                    nearestDist = dist;
                    hitIdx = i;
                    hitType = BossPartType::Turret;
                    hitPartIdx = tIdx;
                }
            }
        }

        for (int pIdx = 0; pIdx < static_cast<int>(car.missilePods.size()); ++pIdx) {
            auto& pod = car.missilePods[pIdx];
            if (pod.isDestroyed || !pod.object) continue;
            const Matrix4x4& mat = pod.object->GetmatWorld();
            Vector3 pWorldPos = { mat.m[3][0], mat.m[3][1] + 0.8f, mat.m[3][2] };
            Sphere pSphere = { pWorldPos, pod.colliderRadius };
            float dist = 0.0f;
            if (CollisionMath::Raycast(ray, pSphere, &dist)) {
                if (dist < nearestDist) {
                    nearestDist = dist;
                    hitIdx = i;
                    hitType = BossPartType::MissilePod;
                    hitPartIdx = pIdx;
                }
            }
        }
    }

    // 2. 車両本体OBB
    for (int i = 0; i < static_cast<int>(carriages_.size()); ++i) {
        auto& car = carriages_[i];
        if (car.isDestroyed) continue;

        Vector3 carPos = car.object ? car.object->GetTranslate() : car.position;
        Matrix4x4 rotMat = Matrix4x4::MakeAffine({ 1.0f, 1.0f, 1.0f }, car.rotation, { 0.0f, 0.0f, 0.0f });
        Vector3 colCenter = carPos + (rotMat * car.collider.center);

        OBB enemyOBB = CollisionMath::CreateOBB(colCenter, car.collider.size, rotMat);

        float dist = 0.0f;
        if (CollisionMath::Raycast(ray, enemyOBB, &dist)) {
            if (dist < nearestDist) {
                nearestDist = dist;
                hitIdx = i;
                hitType = BossPartType::Carriage;
                hitPartIdx = -1;
            }
        }
    }

    if (hitIdx >= 0) {
        if (outDist) *outDist = nearestDist;
        if (outCarriageIndex) *outCarriageIndex = hitIdx;
        if (outPartType) *outPartType = hitType;
        if (outPartIndex) *outPartIndex = hitPartIdx;
        return true;
    }
    return false;
}

void ArmoredTrainBoss::OnDamaged(int carriageIndex, int damage, BossPartType partType, int partIndex) {
    if (carriageIndex < 0 || carriageIndex >= static_cast<int>(carriages_.size())) return;
    auto& car = carriages_[carriageIndex];
    if (car.isDestroyed) return;

    // 1. 砲台の部位破壊処理
    if (partType == BossPartType::Turret && partIndex >= 0 && partIndex < static_cast<int>(car.turrets.size())) {
        auto& turret = car.turrets[partIndex];
        if (!turret.isDestroyed) {
            turret.hp -= damage;
            if (turret.hp <= 0) {
                turret.hp = 0;
                turret.isDestroyed = true;
                if (turret.object) {
                    turret.object->SetColor({ 0.15f, 0.15f, 0.15f, 1.0f }); // 煤けた黒焦げ
                }
                // SoundManager::GetInstance()->PlaySE("small_explosion.mp3", 0.9f);
                // 車両本体にも誘爆連鎖ダメージ
                car.hp = (std::max)(0, car.hp - 3);
            }
            return;
        }
    }

    // 2. ミサイルポッドの部位破壊処理
    if (partType == BossPartType::MissilePod && partIndex >= 0 && partIndex < static_cast<int>(car.missilePods.size())) {
        auto& pod = car.missilePods[partIndex];
        if (!pod.isDestroyed) {
            pod.hp -= damage;
            if (pod.hp <= 0) {
                pod.hp = 0;
                pod.isDestroyed = true;
                if (pod.object) {
                    pod.object->SetColor({ 0.15f, 0.15f, 0.15f, 1.0f }); // 煤けた黒焦げ
                }
                // SoundManager::GetInstance()->PlaySE("small_explosion.mp3", 0.9f);
                // 車両本体にも誘爆連鎖ダメージ
                car.hp = (std::max)(0, car.hp - 4);
            }
            return;
        }
    }

    // 3. 車両本体への通常ダメージ
    if (carriageIndex == 0 && !isCoreExposed_) {
        damage = (std::max)(1, damage / 2);
    }

    car.hp -= damage;
    if (car.hp <= 0) {
        car.hp = 0;
        car.isDestroyed = true;

        // 破壊された車両は黒煙・煤けた色に変化
        car.color = { 0.15f, 0.15f, 0.15f, 1.0f };
        if (car.object) {
            car.object->SetColor(car.color);
        }
        for (auto& t : car.turrets) {
            t.isDestroyed = true;
            if (t.object) t.object->SetColor(car.color);
        }
        for (auto& p : car.missilePods) {
            p.isDestroyed = true;
            if (p.object) p.object->SetColor(car.color);
        }
        if (car.bigCannon) {
            if (car.bigCannon->baseObject) car.bigCannon->baseObject->SetColor(car.color);
            if (car.bigCannon->barrelObject) car.bigCannon->barrelObject->SetColor(car.color);
        }

        // 先頭機関車が破壊されたらボス全体が撃破
        if (carriageIndex == 0) {
            isDefeated_ = true;
            // 全車両を黒焦げに
            for (auto& c : carriages_) {
                c.isDestroyed = true;
                c.color = { 0.1f, 0.1f, 0.12f, 1.0f };
                if (c.object) {
                    c.object->SetColor(c.color);
                }
                for (auto& t : c.turrets) {
                    t.isDestroyed = true;
                    if (t.object) t.object->SetColor(c.color);
                }
                for (auto& p : c.missilePods) {
                    p.isDestroyed = true;
                    if (p.object) p.object->SetColor(c.color);
                }
                if (c.bigCannon) {
                    if (c.bigCannon->baseObject) c.bigCannon->baseObject->SetColor(c.color);
                    if (c.bigCannon->barrelObject) c.bigCannon->barrelObject->SetColor(c.color);
                }
            }
        }
    }
}

float ArmoredTrainBoss::GetTotalHpRate() const {
    int cur = 0;
    int max = 0;
    for (const auto& car : carriages_) {
        cur += car.hp;
        max += car.maxHp;
    }
    return (max > 0) ? static_cast<float>(cur) / max : 0.0f;
}

bool ArmoredTrainBoss::LoadSettingsJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json root;
        file >> root;

        if (root.contains("boss_scale")) SetScaleMultiplier(root["boss_scale"].get<float>());
        if (root.contains("is_follow_player")) isFollowPlayer_ = root["is_follow_player"].get<bool>();
        if (root.contains("desired_lead_distance")) SetDesiredLeadDistance(root["desired_lead_distance"].get<float>());
        if (root.contains("is_random_lead_enabled")) isRandomLeadEnabled_ = root["is_random_lead_enabled"].get<bool>();
        if (root.contains("rush_speed_bonus")) rushSpeedBonus_ = root["rush_speed_bonus"].get<float>();
        if (root.contains("is_ground_snap_enabled")) isGroundSnapEnabled_ = root["is_ground_snap_enabled"].get<bool>();
        if (root.contains("ground_snap_offset")) groundSnapOffset_ = root["ground_snap_offset"].get<float>();
        if (root.contains("lane_offset")) laneOffset_ = root["lane_offset"].get<float>();
        if (root.contains("is_dynamic_lane_change")) isDynamicLaneChange_ = root["is_dynamic_lane_change"].get<bool>();

        // パーツ配置の読み込み
        if (root.contains("part_mounts") && root["part_mounts"].contains("carriages")) {
            const auto& carList = root["part_mounts"]["carriages"];
            for (const auto& cData : carList) {
                int cIdx = cData.value("index", -1);
                if (cIdx < 0 || cIdx >= static_cast<int>(carriages_.size())) continue;
                auto& car = carriages_[cIdx];

                if (cData.contains("length_offset")) {
                    car.baseLengthOffset = cData["length_offset"].get<float>();
                    car.lengthOffset = car.baseLengthOffset * scaleMultiplier_;
                }

                // 砲台
                if (cData.contains("turrets")) {
                    const auto& tList = cData["turrets"];
                    car.turrets.clear();
                    for (const auto& tData : tList) {
                        std::string tName = tData.value("name", "Turret");
                        Vector3 pos = { 0.0f, 0.0f, 0.0f };
                        Vector3 rot = { 0.0f, 0.0f, 0.0f };
                        if (tData.contains("pos") && tData["pos"].is_array() && tData["pos"].size() >= 3) {
                            pos = { tData["pos"][0].get<float>(), tData["pos"][1].get<float>(), tData["pos"][2].get<float>() };
                        }
                        if (tData.contains("rot") && tData["rot"].is_array() && tData["rot"].size() >= 3) {
                            rot = { tData["rot"][0].get<float>(), tData["rot"][1].get<float>(), tData["rot"][2].get<float>() };
                        }
                        AddTurret(cIdx, pos, rot);
                        if (!car.turrets.empty()) car.turrets.back().name = tName;
                    }
                }

                // ミサイルポッド
                if (cData.contains("missile_pods")) {
                    const auto& pList = cData["missile_pods"];
                    car.missilePods.clear();
                    for (const auto& pData : pList) {
                        std::string pName = pData.value("name", "MissilePod");
                        Vector3 pos = { 0.0f, 0.0f, 0.0f };
                        Vector3 rot = { 0.0f, 0.0f, 0.0f };
                        if (pData.contains("pos") && pData["pos"].is_array() && pData["pos"].size() >= 3) {
                            pos = { pData["pos"][0].get<float>(), pData["pos"][1].get<float>(), pData["pos"][2].get<float>() };
                        }
                        if (pData.contains("rot") && pData["rot"].is_array() && pData["rot"].size() >= 3) {
                            rot = { pData["rot"][0].get<float>(), pData["rot"][1].get<float>(), pData["rot"][2].get<float>() };
                        }
                        AddMissilePod(cIdx, pos, rot);
                        if (!car.missilePods.empty()) car.missilePods.back().name = pName;
                    }
                }

                // 大砲台座位置・旋回・仰角
                if (car.bigCannon) {
                    if (cData.contains("big_cannon_pos") && cData["big_cannon_pos"].is_array() && cData["big_cannon_pos"].size() >= 3) {
                        const auto& bp = cData["big_cannon_pos"];
                        car.bigCannon->baseLocalPos = { bp[0].get<float>(), bp[1].get<float>(), bp[2].get<float>() };
                    }
                    if (cData.contains("big_cannon_base_rot") && cData["big_cannon_base_rot"].is_array() && cData["big_cannon_base_rot"].size() >= 3) {
                        const auto& br = cData["big_cannon_base_rot"];
                        car.bigCannon->baseRot = { br[0].get<float>(), br[1].get<float>(), br[2].get<float>() };
                    }
                    if (cData.contains("big_cannon_barrel_rot") && cData["big_cannon_barrel_rot"].is_array() && cData["big_cannon_barrel_rot"].size() >= 3) {
                        const auto& bbr = cData["big_cannon_barrel_rot"];
                        car.bigCannon->barrelRot = { bbr[0].get<float>(), bbr[1].get<float>(), bbr[2].get<float>() };
                    }
                }
            }
        }
        UpdateCarriageTransforms();
        return true;
    } catch (...) {
        return false;
    }
}

bool ArmoredTrainBoss::SaveSettingsJson(const std::string& filepath) const {
    nlohmann::json root;

    // 既存ファイルがあれば読み込んで上書き（他のキーを保持）
    std::ifstream inFile(filepath);
    if (inFile.is_open()) {
        try {
            inFile >> root;
        } catch (...) {}
        inFile.close();
    }

    root["boss_scale"] = scaleMultiplier_;
    root["is_follow_player"] = isFollowPlayer_;
    root["desired_lead_distance"] = desiredLeadDistance_;
    root["is_random_lead_enabled"] = isRandomLeadEnabled_;
    root["rush_speed_bonus"] = rushSpeedBonus_;
    root["is_ground_snap_enabled"] = isGroundSnapEnabled_;
    root["ground_snap_offset"] = groundSnapOffset_;
    root["lane_offset"] = laneOffset_;
    root["is_dynamic_lane_change"] = isDynamicLaneChange_;

    // パーツ配置データの保存
    nlohmann::json carArray = nlohmann::json::array();
    for (int i = 0; i < static_cast<int>(carriages_.size()); ++i) {
        const auto& car = carriages_[i];
        nlohmann::json cData;
        cData["index"] = i;
        cData["name"] = car.name;
        cData["length_offset"] = car.baseLengthOffset;

        // 砲台
        nlohmann::json tArray = nlohmann::json::array();
        for (const auto& t : car.turrets) {
            nlohmann::json tData;
            tData["name"] = t.name;
            tData["pos"] = { t.baseLocalPos.x, t.baseLocalPos.y, t.baseLocalPos.z };
            tData["rot"] = { t.baseLocalRot.x, t.baseLocalRot.y, t.baseLocalRot.z };
            tArray.push_back(tData);
        }
        cData["turrets"] = tArray;

        // ミサイルポッド
        nlohmann::json pArray = nlohmann::json::array();
        for (const auto& p : car.missilePods) {
            nlohmann::json pData;
            pData["name"] = p.name;
            pData["pos"] = { p.baseLocalPos.x, p.baseLocalPos.y, p.baseLocalPos.z };
            pData["rot"] = { p.baseLocalRot.x, p.baseLocalRot.y, p.baseLocalRot.z };
            pArray.push_back(pData);
        }
        cData["missile_pods"] = pArray;

        // 大砲
        if (car.bigCannon) {
            cData["big_cannon_pos"] = { car.bigCannon->baseLocalPos.x, car.bigCannon->baseLocalPos.y, car.bigCannon->baseLocalPos.z };
            cData["big_cannon_base_rot"] = { car.bigCannon->baseRot.x, car.bigCannon->baseRot.y, car.bigCannon->baseRot.z };
            cData["big_cannon_barrel_rot"] = { car.bigCannon->barrelRot.x, car.bigCannon->barrelRot.y, car.bigCannon->barrelRot.z };
        }

        carArray.push_back(cData);
    }
    root["part_mounts"]["carriages"] = carArray;

    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
    std::ofstream outFile(filepath);
    if (!outFile.is_open()) return false;
    outFile << root.dump(4);
    return true;
}

void ArmoredTrainBoss::AddTurret(int carriageIndex, const Vector3& localPos, const Vector3& localRot) {
    if (carriageIndex < 0 || carriageIndex >= static_cast<int>(carriages_.size())) return;
    auto& car = carriages_[carriageIndex];
    BossTurretMount t;
    t.name = "Turret_" + std::to_string(car.turrets.size() + 1);
    t.baseLocalPos = localPos;
    t.baseLocalRot = localRot;
    t.currentRot = localRot;
    t.attackInterval = 85.0f;
    t.attackTimer = static_cast<float>((car.turrets.size() * 23) % 85);
    t.hp = 8;
    t.maxHp = 8;
    t.colliderRadius = 2.4f;

    t.object = std::make_unique<Object3d>();
    t.object->Initialize(object3dCommon_);
    t.object->SetModel("boss_turret.obj");
    t.object->SetParent(car.object.get());
    t.object->SetTranslate(localPos);
    t.object->SetRotation(localRot);
    t.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
    t.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

    t.colliderObject = std::make_unique<Object3d>();
    t.colliderObject->Initialize(object3dCommon_);
    t.colliderObject->SetModel("collider_sphere_enemy.obj");
    t.colliderObject->GetModel()->SetColor({ 1.0f, 0.45f, 0.0f, 1.0f });
    t.colliderObject->SetParent(t.object.get());
    t.colliderObject->SetTranslate({ 0.0f, 0.8f, 0.0f });
    t.colliderObject->SetScale({ t.colliderRadius * 2.0f, t.colliderRadius * 2.0f, t.colliderRadius * 2.0f });

    car.turrets.push_back(std::move(t));
}

void ArmoredTrainBoss::RemoveTurret(int carriageIndex, int turretIndex) {
    if (carriageIndex < 0 || carriageIndex >= static_cast<int>(carriages_.size())) return;
    auto& car = carriages_[carriageIndex];
    if (turretIndex >= 0 && turretIndex < static_cast<int>(car.turrets.size())) {
        car.turrets.erase(car.turrets.begin() + turretIndex);
    }
}

void ArmoredTrainBoss::AddMissilePod(int carriageIndex, const Vector3& localPos, const Vector3& localRot) {
    if (carriageIndex < 0 || carriageIndex >= static_cast<int>(carriages_.size())) return;
    auto& car = carriages_[carriageIndex];
    BossMissilePodMount p;
    p.name = "MissilePod_" + std::to_string(car.missilePods.size() + 1);
    p.baseLocalPos = localPos;
    p.baseLocalRot = localRot;
    p.attackInterval = 130.0f;
    p.attackTimer = static_cast<float>((car.missilePods.size() * 30) % 90);
    p.hp = 10;
    p.maxHp = 10;
    p.colliderRadius = 2.4f;

    p.object = std::make_unique<Object3d>();
    p.object->Initialize(object3dCommon_);
    p.object->SetModel("boss_MissilePod.obj");
    p.object->SetParent(car.object.get());
    p.object->SetTranslate(localPos);
    p.object->SetRotation(localRot);
    p.object->SetEnvironmentTextureIndex(skyboxTexIndex_);
    p.object->SetSelectLightings(9); // サイバー仮想空間 装甲列車ボスシェーダー（case 9）

    p.colliderObject = std::make_unique<Object3d>();
    p.colliderObject->Initialize(object3dCommon_);
    p.colliderObject->SetModel("collider_sphere_enemy.obj");
    p.colliderObject->GetModel()->SetColor({ 1.0f, 0.15f, 0.65f, 1.0f });
    p.colliderObject->SetParent(p.object.get());
    p.colliderObject->SetTranslate({ 0.0f, 0.8f, 0.0f });
    p.colliderObject->SetScale({ p.colliderRadius * 2.0f, p.colliderRadius * 2.0f, p.colliderRadius * 2.0f });

    car.missilePods.push_back(std::move(p));
}

void ArmoredTrainBoss::RemoveMissilePod(int carriageIndex, int podIndex) {
    if (carriageIndex < 0 || carriageIndex >= static_cast<int>(carriages_.size())) return;
    auto& car = carriages_[carriageIndex];
    if (podIndex >= 0 && podIndex < static_cast<int>(car.missilePods.size())) {
        car.missilePods.erase(car.missilePods.begin() + podIndex);
    }
}

void ArmoredTrainBoss::UpdateTimeOverCannon(const Vector3& targetPlayerPos, float timeOverTimer, float dt) {
    for (auto& car : carriages_) {
        if (!car.bigCannon || !car.bigCannon->baseObject || !car.bigCannon->barrelObject || !car.object) continue;

        // 車両のワールド姿勢を取得
        const Matrix4x4& cMat = car.object->GetmatWorld();
        Vector3 cRight = Vector3(cMat.m[0][0], cMat.m[0][1], cMat.m[0][2]).Normalize();
        Vector3 cUp    = Vector3(cMat.m[1][0], cMat.m[1][1], cMat.m[1][2]).Normalize();
        Vector3 cFwd   = Vector3(cMat.m[2][0], cMat.m[2][1], cMat.m[2][2]).Normalize();

        Vector3 cannonBaseWorld = Vector3(cMat.m[3][0], cMat.m[3][1], cMat.m[3][2])
            + cRight * car.bigCannon->baseLocalPos.x
            + cUp    * car.bigCannon->baseLocalPos.y
            + cFwd   * car.bigCannon->baseLocalPos.z;

        Vector3 toTarget = targetPlayerPos - cannonBaseWorld;

        // 車両ローカル空間での相対ベクトル
        Vector3 localTarget = {
            toTarget.x * cRight.x + toTarget.y * cRight.y + toTarget.z * cRight.z,
            toTarget.x * cUp.x    + toTarget.y * cUp.y    + toTarget.z * cUp.z,
            toTarget.x * cFwd.x   + toTarget.y * cFwd.y   + toTarget.z * cFwd.z
        };

        // boss_bigTurret_1 の砲身モデルは初期状態で -Z 軸を向いているため、
        // ターゲットへ砲口を向けるための目標Y軸ヨー角は atan2(-localTarget.x, -localTarget.z)
        float targetRotY = std::atan2(-localTarget.x, -localTarget.z);
        float xzLen = std::sqrt(localTarget.x * localTarget.x + localTarget.z * localTarget.z);
        // 上を向かせる (+Y) ためのX軸ピッチ角（仰角）
        float targetRotX = std::atan2(localTarget.y, (std::max)(xzLen, 0.001f));

        // 角度差分の正規化 (-PI ~ +PI)
        float diffY = targetRotY - car.bigCannon->baseRot.y;
        while (diffY > 3.14159265f) diffY -= 6.2831853f;
        while (diffY < -3.14159265f) diffY += 6.2831853f;

        float diffX = targetRotX - car.bigCannon->barrelRot.x;
        while (diffX > 3.14159265f) diffX -= 6.2831853f;
        while (diffX < -3.14159265f) diffX += 6.2831853f;

        // タイムオーバー開始直後（0.05秒以内）は瞬時にプレイヤー方向へスナップ！
        if (timeOverTimer <= 0.05f) {
            car.bigCannon->baseRot.y = targetRotY;
            car.bigCannon->barrelRot.x = targetRotX;
        } else {
            // タイムオーバー中は高精度・急速に追従（重厚かつ確実にプレイヤーを捉える）
            float rotSpeed = std::clamp(dt * 15.0f, 0.0f, 1.0f);
            car.bigCannon->baseRot.y   += diffY * rotSpeed;
            car.bigCannon->barrelRot.x += diffX * rotSpeed;
        }

        // 発射時（2.2秒以降：UI消滅直後）の強烈な反動（砲身が-Zを向いているため+Z方向へ後退）
        float recoilZ = 0.0f;
        if (timeOverTimer >= 2.2f) {
            float tSinceFire = timeOverTimer - 2.2f;
            recoilZ = std::exp(-tSinceFire * 5.0f) * 3.5f; // 瞬時に約3.5m後退（手前に引く）
        }
        car.bigCannon->recoilZ = recoilZ;

        car.bigCannon->baseObject->SetTranslate(car.bigCannon->baseLocalPos);
        car.bigCannon->baseObject->SetRotation(car.bigCannon->baseRot);
        car.bigCannon->baseObject->Update();

        car.bigCannon->barrelObject->SetTranslate({ 0.0f, 0.0f, recoilZ });
        car.bigCannon->barrelObject->SetRotation(car.bigCannon->barrelRot);
        car.bigCannon->barrelObject->Update();
    }
}

Vector3 ArmoredTrainBoss::GetBigCannonMuzzlePosition() const {
    for (const auto& car : carriages_) {
        if (car.bigCannon && car.bigCannon->barrelObject) {
            const Matrix4x4& m = car.bigCannon->barrelObject->GetmatWorld();
            // boss_bigTurret_1 のローカル砲身先端位置 (0.0f, 12.0f, -25.0f) をワールド変換
            Vector3 muzzleLocal = { 0.0f, 12.0f, -25.0f };
            Vector3 muzzleWorld = {
                muzzleLocal.x * m.m[0][0] + muzzleLocal.y * m.m[1][0] + muzzleLocal.z * m.m[2][0] + m.m[3][0],
                muzzleLocal.x * m.m[0][1] + muzzleLocal.y * m.m[1][1] + muzzleLocal.z * m.m[2][1] + m.m[3][1],
                muzzleLocal.x * m.m[0][2] + muzzleLocal.y * m.m[1][2] + muzzleLocal.z * m.m[2][2] + m.m[3][2]
            };
            return muzzleWorld;
        }
    }
    return position_;
}