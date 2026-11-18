#include "BossFinisherSequence.h"
#include "KHEngine/Graphics/Resource/Descriptor/SrvManager.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/System/RailCameraController.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/UI/UITextManager.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include <cmath>
#include <algorithm>

void BossFinisherSequence::Initialize(Object3dCommon* object3dCommon, Camera* camera, RailCameraController* railCam, uint32_t skyboxTexIndex) {
    object3dCommon_ = object3dCommon;
    camera_ = camera;
    railCam_ = railCam;
    skyboxTexIndex_ = skyboxTexIndex;
    phase_ = Phase::INACTIVE;
    phaseTimer_ = 0.0f;

    if (camera_) {
        defaultCameraFov_ = camera_->GetFovY();
    }

    // 演出に必要な3Dモデルを事前ロード
    auto modelMgr = ModelManager::GetInstance();
    modelMgr->LoadModel("missile.obj");
    modelMgr->LoadModel("memory.obj");
    modelMgr->LoadModel("cube.obj");
    modelMgr->LoadModel("ring.obj");
    modelMgr->LoadModel("collider_sphere_enemy.obj");

    // 赤データメモリ用オブジェクトの生成
    redMemoryObj_ = std::make_unique<Object3d>();
    redMemoryObj_->Initialize(object3dCommon_);
    redMemoryObj_->SetModel("memory.obj");
    redMemoryObj_->SetColor({ 1.0f, 0.15f, 0.2f, 1.0f }); // 高エネルギー赤ネオン
    redMemoryObj_->SetEnvironmentTextureIndex(skyboxTexIndex_);
    redMemoryObj_->SetEnableLighting(false);
    redMemoryObj_->SetScale({ 0.08f, 0.08f, 0.08f });

    // 衝撃波リングの生成
    shockwaveRings_.clear();
    for (int i = 0; i < 4; ++i) {
        auto ring = std::make_unique<Object3d>();
        ring->Initialize(object3dCommon_);
        ring->SetModel("ring.obj");
        ring->SetColor({ 1.0f, 0.1f, 0.5f, 1.0f }); // ネオンマゼンタ
        ring->SetEnvironmentTextureIndex(skyboxTexIndex_);
        ring->SetEnableLighting(false);
        ring->SetScale({ 0.1f, 0.1f, 0.1f });
        shockwaveRings_.push_back(std::move(ring));
    }
}

void BossFinisherSequence::StartSequence(ArmoredTrainBoss* boss, Player* player) {
    if (!boss || !player) return;
    boss_ = boss;
    player_ = player;

    // 自機を処刑モードに移行（通常弾や通常特殊弾の誤発射を遮断、発射までは移動可能）
    player->SetExecutionMode(true);
    player->SetEnableInput(true);
    player->SetCutsceneOverride(false);

    phase_ = Phase::PHASE_1_TRANSITION;
    phaseTimer_ = 0.0f;
    spaceHoldTimer_ = 0.0f;
    isAllCoresLocked_ = false;
    isRedMemoryAbsorbed_ = false;
    hitStopTimer_ = 0.0f;
    glitchPulseTimer_ = 0.0f;
    hasResolvedFrontCam_ = false;
    chosenCamSide_ = 1.0f;
    hasDissolveStarted_ = false;
    dissolveTimer_ = 0.0f;
    voxelSpawnAccum_ = 0.0f;

    // 弱点コア（4両それぞれの車両位置）を初期化
    SetupCores(boss);

    // ボス先頭車両位置から自機へ向かう赤データメモリを配置
    redMemoryStartPos_ = boss->GetPosition() + Vector3(0.0f, 6.0f, 0.0f);
    redMemoryPos_ = redMemoryStartPos_;
    if (redMemoryObj_) {
        redMemoryObj_->SetTranslate(redMemoryPos_);
        redMemoryObj_->Update();
    }

    // カメラFOVをリセット
    if (camera_) {
        camera_->SetFovY(defaultCameraFov_);
    }

    // SE再生（警報・警告音）
    SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.9f);

    // UIテキストエディタ一括管理: 処刑警告・操作ガイドを表示（エディタで編集・保存された文言と色を保持）
    if (auto textMgr = UITextManager::GetInstance()) {
        textMgr->SetVisible("ExecuteCritical", true, "GAMEPLAY");
        textMgr->SetVisible("ExecuteGuide", true, "GAMEPLAY");
        textMgr->SetVisible("MissionAccomplishedTitle", false, "GAMEPLAY");
        textMgr->SetVisible("MissionAccomplishedSub", false, "GAMEPLAY");
    }
}

void BossFinisherSequence::Reset(Player* player) {
    phase_ = Phase::INACTIVE;
    phaseTimer_ = 0.0f;
    spaceHoldTimer_ = 0.0f;
    isAllCoresLocked_ = false;
    isRedMemoryAbsorbed_ = false;
    hitStopTimer_ = 0.0f;
    glitchPulseTimer_ = 0.0f;
    hasResolvedFrontCam_ = false;
    chosenCamSide_ = 1.0f;
    hasDissolveStarted_ = false;
    dissolveTimer_ = 0.0f;
    voxelSpawnAccum_ = 0.0f;
    cores_.clear();
    missiles_.clear();
    collapseShards_.clear();
    wingTrailParticles_.clear();
    chargeParticles_.clear();

    if (camera_) {
        camera_->SetFovY(defaultCameraFov_);
    }
    if (player) {
        player->SetVisible(true);
        player->SetExecutionMode(false);
        player->SetEnableInput(true);
        player->SetCutsceneOverride(false);
    }

    // UIテキストエディタ一括管理: 全項目を非表示
    if (auto textMgr = UITextManager::GetInstance()) {
        textMgr->SetVisible("ExecuteCritical", false, "GAMEPLAY");
        textMgr->SetVisible("ExecuteGuide", false, "GAMEPLAY");
        textMgr->SetVisible("MissionAccomplishedTitle", false, "GAMEPLAY");
        textMgr->SetVisible("MissionAccomplishedSub", false, "GAMEPLAY");
    }
}

void BossFinisherSequence::SetupCores(ArmoredTrainBoss* boss) {
    cores_.clear();
    if (!boss) return;

    const auto& carriages = boss->GetCarriages();
    int carCount = static_cast<int>(carriages.size());
    if (carCount == 0) carCount = 4;

    for (int i = 0; i < carCount; ++i) {
        FinisherCore core;
        core.carriageIndex = i;
        core.worldPos = boss->GetCarriagePosition(i) + Vector3(0.0f, 5.0f, 0.0f);
        core.isLocked = false;
        core.lockProgress = 0.0f;
        core.snapTimer = 0.0f;
        float angles[4] = { -0.7f, 0.7f, -1.3f, 1.3f };
        core.startAngle = (i < 4) ? angles[i] : (0.5f * (i % 2 == 0 ? -1.0f : 1.0f));

        // コア球体オブジェクト
        core.sphereObj = std::make_unique<Object3d>();
        core.sphereObj->Initialize(object3dCommon_);
        core.sphereObj->SetModel("collider_sphere_enemy.obj");
        core.sphereObj->SetColor({ 1.0f, 0.1f, 0.15f, 1.0f }); // 初期: 赤エラー発光
        core.sphereObj->SetEnvironmentTextureIndex(skyboxTexIndex_);
        core.sphereObj->SetEnableLighting(false);
        core.sphereObj->SetScale({ 2.8f, 2.8f, 2.8f });
        core.sphereObj->SetTranslate(core.worldPos);
        core.sphereObj->Update();

        // 六角形ハニカムマーカー
        core.markerObj = std::make_unique<Object3d>();
        core.markerObj->Initialize(object3dCommon_);
        core.markerObj->SetModel("ring.obj");
        core.markerObj->SetColor({ 1.0f, 0.2f, 0.2f, 0.9f });
        core.markerObj->SetEnvironmentTextureIndex(skyboxTexIndex_);
        core.markerObj->SetEnableLighting(false);
        core.markerObj->SetScale({ 3.5f, 3.5f, 3.5f });
        core.markerObj->SetTranslate(core.worldPos);
        core.markerObj->Update();

        cores_.push_back(std::move(core));
    }
}

void BossFinisherSequence::Update(float dt, ArmoredTrainBoss* boss, Player* player, float& inOutGameSpeed) {
    if (phase_ == Phase::INACTIVE || phase_ == Phase::COMPLETED) return;
    boss_ = boss;
    player_ = player;

    phaseTimer_ += dt;

    // ヒットストップ処理
    if (hitStopTimer_ > 0.0f) {
        hitStopTimer_ -= dt;
        inOutGameSpeed = 0.0f; // 停止
        if (hitStopTimer_ <= 0.0f) {
            inOutGameSpeed = 1.0f;
        }
    }

    // 各フェーズの更新
    switch (phase_) {
    case Phase::PHASE_1_TRANSITION:
        UpdatePhase1Transition(dt, boss, player);
        break;
    case Phase::PHASE_2_AUTO_LOCKON:
        UpdatePhase2AutoLockOn(dt, boss, player);
        break;
    case Phase::PHASE_3_FINISHER_FIRE:
        UpdatePhase3FinisherFire(dt, boss, player, inOutGameSpeed);
        break;
    case Phase::PHASE_4_DATA_COLLAPSE:
        UpdatePhase4DataCollapse(dt, boss, player);
        break;
    case Phase::PHASE_5_MISSION_ACCOMPLISHED:
        UpdatePhase5MissionAccomplished(dt, boss, player);
        break;
    default:
        break;
    }
}

void BossFinisherSequence::UpdatePhase1Transition(float dt, ArmoredTrainBoss* boss, Player* player) {
    if (!boss || !player) return;

    // 1. 各車両の弱点コア位置をボスの最新位置に追従
    for (auto& core : cores_) {
        core.worldPos = boss->GetCarriagePosition(core.carriageIndex) + Vector3(0.0f, 5.0f, 0.0f);
        if (core.sphereObj) {
            // パルス点滅（警告赤）
            float pulse = 0.8f + 0.3f * std::sin(phaseTimer_ * 12.0f);
            core.sphereObj->SetScale({ 2.8f * pulse, 2.8f * pulse, 2.8f * pulse });
            core.sphereObj->SetTranslate(core.worldPos);
            core.sphereObj->Update();
        }
        if (core.markerObj) {
            core.markerObj->SetTranslate(core.worldPos);
            core.markerObj->Update();
        }
    }

    // 2. 赤データメモリの強制飛来（ボスから自機へ）
    if (!isRedMemoryAbsorbed_ && redMemoryObj_) {
        Vector3 targetPos = player->GetWorldPosition();
        float t = std::clamp(phaseTimer_ / 1.1f, 0.0f, 1.0f);
        float smoothT = t * t * (3.0f - 2.0f * t);

        // ベジェ曲線状に少し上へ弧を描いて飛来
        Vector3 midPos = (redMemoryStartPos_ + targetPos) * 0.5f + Vector3(0.0f, 8.0f, 0.0f);
        float w0 = (1.0f - smoothT) * (1.0f - smoothT);
        float w1 = 2.0f * (1.0f - smoothT) * smoothT;
        float w2 = smoothT * smoothT;
        redMemoryPos_ = redMemoryStartPos_ * w0 + midPos * w1 + targetPos * w2;

        redMemoryObj_->SetTranslate(redMemoryPos_);
        redMemoryObj_->SetRotation({ phaseTimer_ * 8.0f, phaseTimer_ * 12.0f, 0.0f });
        redMemoryObj_->Update();

        // 自機へ到達・吸収
        if (t >= 0.99f || (redMemoryPos_ - targetPos).Length() < 3.0f) {
            isRedMemoryAbsorbed_ = true;
            SoundManager::GetInstance()->PlaySE("button.mp3", 0.9f);
            SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.6f);

            // 自機の武器UIをオーバーライド
            player->SetReticleColor({ 1.0f, 0.1f, 0.4f, 1.0f }); // 処刑マゼンタ
        }
    }

    // 飛来吸収完了後、フェーズ②（自動ロックオン）へ遷移
    if (phaseTimer_ >= 1.4f) {
        phase_ = Phase::PHASE_2_AUTO_LOCKON;
        phaseTimer_ = 0.0f;
        spaceHoldTimer_ = 0.0f;
    }
}

void BossFinisherSequence::UpdatePhase2AutoLockOn(float dt, ArmoredTrainBoss* boss, Player* player) {
    if (!boss || !player) return;

    auto services = EngineServices::GetInstance();
    auto input = services ? services->GetInput() : nullptr;
    bool isDemoMode = (services && services->IsDemoMode());
    bool isSpaceHolding = false;
    if (isDemoMode) {
        // デモプレイ中は自動でスペース長押しを行い、全コアロック完了の0.45秒後に自動発射する
        isSpaceHolding = (spaceHoldTimer_ < kRequiredHoldTime_ + 0.45f);
    } else if (input) {
        isSpaceHolding = input->PushKey(DIK_SPACE) || input->PushPadButton(XINPUT_GAMEPAD_A);
    }

    // 弱点コアの位置同期
    for (auto& core : cores_) {
        core.worldPos = boss->GetCarriagePosition(core.carriageIndex) + Vector3(0.0f, 5.0f, 0.0f);
        if (core.sphereObj) {
            core.sphereObj->SetTranslate(core.worldPos);
            core.sphereObj->Update();
        }
        if (core.markerObj) {
            core.markerObj->SetTranslate(core.worldPos);
            core.markerObj->Update();
        }
    }

    if (isSpaceHolding) {
        spaceHoldTimer_ += dt;

        // カメラFOVをわずかに拡大（0.45 -> 0.52rad：溜め感のある引きカメラ）
        if (camera_) {
            float fovT = std::clamp(spaceHoldTimer_ / kRequiredHoldTime_, 0.0f, 1.0f);
            camera_->SetFovY(defaultCameraFov_ + 0.07f * fovT);
        }

        // 4箇所の弱点コアをファンネルのように順番に接近・収束ロックオン（約0.25秒ごと）
        float lockInterval = kRequiredHoldTime_ / static_cast<float>(cores_.size());

        for (int i = 0; i < static_cast<int>(cores_.size()); ++i) {
            float coreStartTime = i * lockInterval;
            float coreEndTime = (i + 1) * lockInterval;

            if (spaceHoldTimer_ >= coreStartTime) {
                // このコアのファンネルロックオン接近開始
                float progress = std::clamp((spaceHoldTimer_ - coreStartTime) / lockInterval, 0.0f, 1.0f);
                cores_[i].lockProgress = progress;

                if (spaceHoldTimer_ >= coreEndTime) {
                    // ロックオン確定！
                    if (!cores_[i].isLocked) {
                        cores_[i].isLocked = true;
                        cores_[i].snapTimer = 1.0f; // スナップ収縮開始
                        SoundManager::GetInstance()->PlaySE("select.mp3", 0.85f);

                        // ロックオン完了カラー：赤からネオンマゼンタ／シアンへ収束
                        if (cores_[i].sphereObj) {
                            cores_[i].sphereObj->SetColor({ 1.0f, 0.1f, 0.6f, 1.0f });
                            cores_[i].sphereObj->SetScale({ 3.5f, 3.5f, 3.5f });
                        }
                        if (cores_[i].markerObj) {
                            cores_[i].markerObj->SetColor({ 0.2f, 0.9f, 1.0f, 1.0f });
                        }
                    }
                } else {
                    cores_[i].isLocked = false;
                }
            } else {
                // まだ順番が来ていない
                cores_[i].lockProgress = 0.0f;
                cores_[i].isLocked = false;
            }
        }

        if (spaceHoldTimer_ >= kRequiredHoldTime_) {
            if (!isAllCoresLocked_) {
                SoundManager::GetInstance()->PlaySE("button.mp3", 0.9f);
            }
            isAllCoresLocked_ = true;
        }
    } else {
        // キーを離した判定（プレイヤー自身の入力タイミングで発射）
        if (isAllCoresLocked_) {
            // 全ロック完了後に離した -> 処刑ミサイル一斉射出！
            FireExecutionMissiles(player);
            phase_ = Phase::PHASE_3_FINISHER_FIRE;
            phaseTimer_ = 0.0f;
            return;
        } else {
            // 溜め途中で離した場合はチャージ進行をリセット
            spaceHoldTimer_ = 0.0f;
            isAllCoresLocked_ = false;
            for (auto& core : cores_) {
                core.isLocked = false;
            }
            if (camera_) {
                camera_->SetFovY(defaultCameraFov_);
            }
        }
    }

    // 各コアのスナップ収縮アニメーションタイマー更新
    for (auto& core : cores_) {
        if (core.snapTimer > 0.0f) {
            core.snapTimer -= 3.5f * dt;
            if (core.snapTimer < 0.0f) core.snapTimer = 0.0f;
        }
    }
}

void BossFinisherSequence::FireExecutionMissiles(Player* player) {
    missiles_.clear();
    if (!player) return;

    // 処刑ミサイル発射と同時にムービー突入：プレイヤーの操作をロックし、ミサイル発射シーンでは非表示にする
    // カメラワークが発射位置によってブレないよう、常に「画面中央の少し上」({0, 1.5, 0})を基準位置として固定する
    player->SetEnableInput(false);
    player->SetVisible(false);
    player->SetCutsceneOverride(true, { 0.0f, 1.5f, 0.0f }, { 0.0f, 0.0f, 0.0f });
    player->Update3DObjectOnly();

    // ボスとレール進行を即座に固定し、カットインカメラ構図を安定させる
    if (boss_) {
        boss_->Stop();
        boss_->SetDefeated(true);
    }
    if (railCam_) {
        railCam_->SetSpeedMultiplier(0.0f);
    }

    // 黒帯ムービー開始と同時にすべてのUIテキストを完全非表示化
    if (auto textMgr = UITextManager::GetInstance()) {
        textMgr->SetHudAlpha(0.0f);
        textMgr->SetVisible("ExecuteCritical", false, "GAMEPLAY");
        textMgr->SetVisible("ExecuteGuide", false, "GAMEPLAY");
        textMgr->SetVisible("MissionAccomplishedTitle", false, "GAMEPLAY");
        textMgr->SetVisible("MissionAccomplishedSub", false, "GAMEPLAY");
    }

    Vector3 pPos = player->GetWorldPosition();
    Vector3 pForward = player->GetWorldForward();
    Vector3 pRight = player->GetWorldRight();
    Vector3 pUp = player->GetWorldUp();

    SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.7f);

    // 4号車(index 3) -> 3号車(index 2) -> 2号車(index 1) -> 1号車(index 0) の順に着弾する時刻テーブル
    const float kHitTimes[4] = { 1.60f, 1.25f, 0.90f, 0.55f };

    for (size_t i = 0; i < cores_.size(); ++i) {
        ExecutionMissile m;
        m.targetCoreIndex = static_cast<int>(i);
        m.targetPos = cores_[i].worldPos;
        m.hitTime = (i < 4) ? kHitTimes[i] : 1.0f;
        m.isHit = false;
        m.lifeTimer = 0.0f;

        // 自機の左右スプレッド位置から発射
        float side = (i % 2 == 0) ? -2.8f : 2.8f;
        float height = (i < 2) ? 1.2f : -0.8f;
        m.startPos = pPos + pRight * side + pUp * height;
        m.position = m.startPos;

        // 外側・上方に大きく弧を描いてから各車両コアへ突き刺さるベジェ制御点
        m.controlPos = (m.startPos + m.targetPos) * 0.5f + pRight * (side * 5.0f) + pUp * (9.0f + height * 2.0f);
        m.velocity = pForward * 35.0f;
        m.rotation = { 0.0f, 0.0f, 0.0f };

        m.obj = std::make_unique<Object3d>();
        m.obj->Initialize(object3dCommon_);
        m.obj->SetModel("missile.obj");
        m.obj->SetColor({ 1.0f, 0.15f, 0.55f, 1.0f }); // ネオンマゼンタ発光
        m.obj->SetEnvironmentTextureIndex(skyboxTexIndex_);
        m.obj->SetEnvironmentCoefficient(0.0f);
        m.obj->SetEnableLighting(true);
        m.obj->SetSelectLightings(11); // ミサイル専用サイバーシェーダー
        m.obj->SetScale({ 1.5f, 1.5f, 1.5f });
        m.obj->SetTranslate(m.position);
        m.obj->Update();

        missiles_.push_back(std::move(m));
    }
}

void BossFinisherSequence::UpdatePhase3FinisherFire(float dt, ArmoredTrainBoss* boss, Player* player, float& inOutGameSpeed) {
    if (!boss) return;
    boss->Stop();
    boss->SetDefeated(true);
    if (railCam_) {
        railCam_->SetSpeedMultiplier(0.0f);
    }

    // カメラFOVを通常へ滑らかに戻す
    if (camera_) {
        float curFov = camera_->GetFovY();
        camera_->SetFovY(curFov + (defaultCameraFov_ - curFov) * (4.0f * dt));
    }

    // 弱点コアの位置同期
    for (auto& core : cores_) {
        core.worldPos = boss->GetCarriagePosition(core.carriageIndex) + Vector3(0.0f, 5.0f, 0.0f);
    }

    // =========================================================================
    // 【カメラフェーズ1：発射アングル (0.0s 〜 1.8s)】(PDF仕様書 4節)
    // ・カメラ位置：自機の斜め前方・ローアングルへ回り込み、ミサイル群を追ってボスへ接近
    // ・注視点(LookAt)：画面手前に自機、奥にボスを収め、自機から放たれたミサイル群の
    //   軌跡と各弱点コアへの着弾インパクトを捉える
    // =========================================================================
    if (railCam_ && player) {
        const Vector3 kMovieOrigin = { 0.0f, 1.5f, 0.0f };
        player->SetCutsceneOverride(true, kMovieOrigin, { 0.0f, 0.0f, 0.0f });
        player->Update3DObjectOnly();

        Vector3 pLocal = kMovieOrigin;
        Vector3 pWorld = player->GetWorldPosition();
        Vector3 pRight = player->GetWorldRight();
        Vector3 pUp    = player->GetWorldUp();
        Vector3 pFwd   = player->GetWorldForward();
        Vector3 anchorOrigin = pWorld - (pRight * pLocal.x + pUp * pLocal.y + pFwd * pLocal.z);

        auto WorldToAnchorLocal = [&](const Vector3& wPos) -> Vector3 {
            Vector3 diff = wPos - anchorOrigin;
            return {
                Vector3::Dot(diff, pRight),
                Vector3::Dot(diff, pUp),
                Vector3::Dot(diff, pFwd)
            };
        };

        Vector3 bossCenterWorld = (
            boss->GetCarriagePosition(0) +
            boss->GetCarriagePosition(1) +
            boss->GetCarriagePosition(2) +
            boss->GetCarriagePosition(3)
        ) * 0.25f + Vector3(0.0f, 5.5f, 0.0f);
        Vector3 bossCenterLocal = WorldToAnchorLocal(bossCenterWorld);
        float sideSign = (bossCenterLocal.x < pLocal.x) ? 1.0f : -1.0f;

        // 0.0s〜1.8s で自機斜め前方（高めの視点）からミサイル群を追ってボスへ接近
        float tMove = std::clamp(phaseTimer_ / 1.8f, 0.0f, 1.0f);
        float easeMove = tMove * tMove * (3.0f - 2.0f * tMove);

        Vector3 camLaunchPos = pLocal + Vector3(sideSign * 7.5f, 4.5f, 6.5f);
        Vector3 camApproachPos = pLocal + (bossCenterLocal - pLocal) * 0.45f + Vector3(sideSign * 14.0f, 6.5f, 4.0f);
        Vector3 camPos = camLaunchPos + (camApproachPos - camLaunchPos) * easeMove;

        // 画面手前に自機、奥にボスを収める注視点から徐々にボスの着弾点へフォーカス
        Vector3 lookStart = pLocal * 0.45f + bossCenterLocal * 0.55f;
        Vector3 lookEnd = bossCenterLocal;
        Vector3 lookAt = lookStart + (lookEnd - lookStart) * easeMove;

        railCam_->SetCinematicCamera(true, camPos, lookAt, 1.0f);
    }

    bool allMissilesHit = true;

    for (auto& m : missiles_) {
        if (m.isHit) continue;
        allMissilesHit = false;

        m.lifeTimer += dt;

        // ターゲットコアの最新位置
        Vector3 target = (m.targetCoreIndex >= 0 && m.targetCoreIndex < static_cast<int>(cores_.size()))
            ? cores_[m.targetCoreIndex].worldPos
            : m.targetPos;

        float t = std::clamp(m.lifeTimer / (std::max)(0.1f, m.hitTime), 0.0f, 1.0f);
        float invT = 1.0f - t;

        Vector3 prevPos = m.position;
        // 2次ベジェ曲線で正確に hitTime でターゲットへ到達
        m.position = m.startPos * (invT * invT) + m.controlPos * (2.0f * invT * t) + target * (t * t);

        Vector3 moveDir = m.position - prevPos;
        if (moveDir.Length() > 0.0001f) {
            float rotY = std::atan2(moveDir.x, moveDir.z);
            float xzLen = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
            float rotX = std::atan2(-moveDir.y, (std::max)(xzLen, 0.001f));
            m.rotation = { rotX, rotY, 0.0f };
        }

        if (m.obj) {
            m.obj->SetTranslate(m.position);
            m.obj->SetRotation(m.rotation);
            m.obj->Update();
        }

        // 着弾判定（4 -> 3 -> 2 -> 1 の順番にピタッと着弾！）
        if (t >= 1.0f) {
            m.isHit = true;
            SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.95f);

            // サイバーボクセルブロック着弾パーティクルを再生（巨大ボスの車体に隠れないようカメラ手前・上側へ引き出す）
            if (onVoxelHit_) {
                Vector3 fxPos = target + Vector3(0.0f, 4.0f, 0.0f);
                if (camera_) {
                    const Matrix4x4& cMat = camera_->GetWorldMatrix();
                    Vector3 toCam = { cMat.m[3][0] - target.x, cMat.m[3][1] - target.y, cMat.m[3][2] - target.z };
                    float len = toCam.Length();
                    if (len > 0.001f) {
                        fxPos = fxPos + (toCam / len) * 10.5f;
                    }
                }
                onVoxelHit_(fxPos, 32, 1.3f, 2.7f);
            }

            // 衝撃波リングをコア位置に展開
            if (m.targetCoreIndex >= 0 && m.targetCoreIndex < static_cast<int>(shockwaveRings_.size())) {
                auto& ring = shockwaveRings_[m.targetCoreIndex];
                if (ring) {
                    ring->SetTranslate(target);
                    ring->SetScale({ 1.2f, 1.2f, 1.2f });
                    ring->Update();
                }
            }
        }
    }

    // 衝撃波リングの拡大演出
    for (auto& ring : shockwaveRings_) {
        if (ring) {
            Vector3 sc = ring->GetScale();
            if (sc.x > 0.5f) {
                sc = sc + Vector3(28.0f * dt, 28.0f * dt, 28.0f * dt);
                ring->SetScale(sc);
                ring->Update();
            }
        }
    }

    // フェーズ1（発射アングル 0.0s〜1.8s）完了後、フェーズ2（1車両目の正面が見える位置へカメラを引く）へ移行
    if (allMissilesHit && phaseTimer_ >= 1.80f) {
        phase_ = Phase::PHASE_4_DATA_COLLAPSE;
        phaseTimer_ = 0.0f;
        if (railCam_) {
            cinematicCamStartPos_ = railCam_->GetCinematicLocalPos();
        }
        playerAscentStartPos_ = { 0.0f, 1.5f, 0.0f };
        collapseShards_.clear(); // 消滅が始まるフェーズ3からパーティクルを出すためここでは生成しない
    }
}

void BossFinisherSequence::SpawnCollapseShards(ArmoredTrainBoss* boss) {
    collapseShards_.clear();
    if (!boss) return;

    // 各車両の周囲から大量のキューブ＆三角ポリゴンシャードを生成
    for (int i = 0; i < 4; ++i) {
        Vector3 carPos = boss->GetCarriagePosition(i);

        int shardCountPerCar = 35;
        for (int k = 0; k < shardCountPerCar; ++k) {
            CollapseShard s;
            s.isCube = (k % 2 == 0);
            s.obj = std::make_unique<Object3d>();
            s.obj->Initialize(object3dCommon_);
            s.obj->SetModel(s.isCube ? "cube.obj" : "ring.obj");

            // マゼンタ・シアン・赤のグリッチカラー
            if (k % 3 == 0) s.color = { 1.0f, 0.1f, 0.5f, 1.0f }; // ネオンマゼンタ
            else if (k % 3 == 1) s.color = { 0.1f, 0.9f, 1.0f, 1.0f }; // ネオンシアン
            else s.color = { 1.0f, 0.2f, 0.2f, 1.0f }; // レッド

            s.obj->SetColor(s.color);
            s.obj->SetEnvironmentTextureIndex(skyboxTexIndex_);
            s.obj->SetEnableLighting(false);

            float sz = 0.35f + (rand() % 10) * 0.06f;
            s.scale = { sz, sz, sz };
            s.obj->SetScale(s.scale);

            // 車両周囲のランダム位置
            Vector3 offset = {
                ((rand() % 100) - 50) * 0.12f,
                ((rand() % 100) - 20) * 0.08f,
                ((rand() % 100) - 50) * 0.25f
            };
            s.position = carPos + offset;
            s.velocity = offset * 2.5f + Vector3(0.0f, 4.0f + (rand() % 10) * 0.5f, 0.0f);
            s.rotation = { 0.0f, 0.0f, 0.0f };
            s.rotSpeed = {
                ((rand() % 100) - 50) * 0.1f,
                ((rand() % 100) - 50) * 0.1f,
                ((rand() % 100) - 50) * 0.1f
            };
            s.life = 0.0f;
            s.maxLife = 1.8f;

            s.obj->SetTranslate(s.position);
            s.obj->Update();
            collapseShards_.push_back(std::move(s));
        }
    }
}

bool BossFinisherSequence::IsWorldPointClearOfTerrain(const Vector3& worldPos, float radius) const {
    if (!terrainObjects_ || terrainObjects_->empty()) return true;
    for (const auto& obj : *terrainObjects_) {
        if (!obj || !obj->HasMeshCollider() || obj->IsBackgroundMountain()) continue;
        // 1. 地形メッシュとの球体めり込み判定
        if (obj->CheckCollisionWithSphere(Sphere{ worldPos, radius })) {
            return false;
        }
        // 2. 真下にレイを飛ばし、崖の内側・地面の下に埋まっていないか判定
        float gy = 0.0f;
        Vector3 norm = { 0.0f, 1.0f, 0.0f };
        if (obj->RaycastDown(worldPos.x, worldPos.z, worldPos.y + 80.0f, &gy, &norm)) {
            if (worldPos.y < gy + radius) {
                return false;
            }
        }
    }
    return true;
}

Vector3 BossFinisherSequence::ResolveSafeCameraLocalPos(
    const Vector3& desiredLocal,
    const Vector3& anchorOrigin,
    const Vector3& pRight,
    const Vector3& pUp,
    const Vector3& pFwd,
    const Vector3& bossHeadLocal,
    const Vector3& bossFwd) const
{
    auto LocalToWorld = [&](const Vector3& lPos) -> Vector3 {
        return anchorOrigin + pRight * lPos.x + pUp * lPos.y + pFwd * lPos.z;
    };

    const float kCamSafeRadius = 3.5f;
    if (IsWorldPointClearOfTerrain(LocalToWorld(desiredLocal), kCamSafeRadius)) {
        return desiredLocal;
    }

    // 崖や山にめり込んでいる場合：まず空側（上方向）へ段階的に逃がす
    const float heightOffsets[] = { 4.0f, 8.0f, 12.0f, 16.0f, 22.0f, 28.0f };
    for (float dy : heightOffsets) {
        Vector3 cand = desiredLocal + Vector3(0.0f, dy, 0.0f);
        if (IsWorldPointClearOfTerrain(LocalToWorld(cand), kCamSafeRadius)) {
            return cand;
        }
    }

    // それでも崖に当たる場合：少しボス側（内側）へ寄せつつ高さを上げてクリア位置を探す
    const float pullFactors[] = { 0.72f, 0.50f };
    Vector3 relFromHead = desiredLocal - bossHeadLocal;
    for (float pf : pullFactors) {
        for (float dy : heightOffsets) {
            Vector3 cand = bossHeadLocal + Vector3(relFromHead.x * pf, relFromHead.y + dy, relFromHead.z * pf);
            if (IsWorldPointClearOfTerrain(LocalToWorld(cand), kCamSafeRadius)) {
                return cand;
            }
        }
    }

    // 最終フォールバック：ボス上空の安全な高い位置
    return bossHeadLocal + bossFwd * 24.0f + Vector3(0.0f, 22.0f, 0.0f);
}

void BossFinisherSequence::UpdatePhase4DataCollapse(float dt, ArmoredTrainBoss* boss, Player* player) {
    if (!boss) return;
    boss->Stop();
    boss->SetDefeated(true);
    if (railCam_) {
        railCam_->SetSpeedMultiplier(0.0f);
    }

    // 1. まだボスの実体を保持（次のロール通過フェーズでプレイヤーが上空を通る瞬間に一斉消滅開始させる）
    boss->SetDissolveAlpha(1.0f);

    // =========================================================================
    // 【カメラフェーズ2：1車両目正面への引きカメラ (1.8s 〜 2.6s : 計0.8s)】
    // ・ボスの進行方向基準で正面斜め前を計算し、地形（渓谷の崖）を回避した開けた側へ配置
    // =========================================================================
    if (railCam_ && player) {
        player->SetCutsceneOverride(true, playerAscentStartPos_, { 0.0f, 0.0f, 0.0f });
        player->Update3DObjectOnly();

        Vector3 pLocal = playerAscentStartPos_;
        Vector3 pWorld = player->GetWorldPosition();
        Vector3 pRight = player->GetWorldRight();
        Vector3 pUp    = player->GetWorldUp();
        Vector3 pFwd   = player->GetWorldForward();
        Vector3 anchorOrigin = pWorld - (pRight * pLocal.x + pUp * pLocal.y + pFwd * pLocal.z);

        auto WorldToAnchorLocal = [&](const Vector3& wPos) -> Vector3 {
            Vector3 diff = wPos - anchorOrigin;
            return {
                Vector3::Dot(diff, pRight),
                Vector3::Dot(diff, pUp),
                Vector3::Dot(diff, pFwd)
            };
        };
        auto LocalToWorld = [&](const Vector3& lPos) -> Vector3 {
            return anchorOrigin + pRight * lPos.x + pUp * lPos.y + pFwd * lPos.z;
        };

        Vector3 bossHeadLocal = WorldToAnchorLocal(boss->GetCarriagePosition(0) + Vector3(0.0f, 5.5f, 0.0f));
        Vector3 bossTailLocal = WorldToAnchorLocal(boss->GetCarriagePosition(3) + Vector3(0.0f, 5.5f, 0.0f));
        Vector3 bossCenterWorld = (
            boss->GetCarriagePosition(0) +
            boss->GetCarriagePosition(1) +
            boss->GetCarriagePosition(2) +
            boss->GetCarriagePosition(3)
        ) * 0.25f + Vector3(0.0f, 5.5f, 0.0f);
        Vector3 bossCenterLocal = WorldToAnchorLocal(bossCenterWorld);

        // ボスの実際の進行方向と左右軸（カーブや斜め向きでも100%ボス基準に揃える）
        Vector3 bossFwd = bossHeadLocal - bossTailLocal;
        bossFwd.y = 0.0f;
        float bLen = bossFwd.Length();
        if (bLen > 0.001f) bossFwd = bossFwd / bLen;
        else bossFwd = { 0.0f, 0.0f, 1.0f };
        Vector3 bossRight = { bossFwd.z, 0.0f, -bossFwd.x };
        Vector3 bossUp = { 0.0f, 1.0f, 0.0f };

        if (!hasResolvedFrontCam_) {
            hasResolvedFrontCam_ = true;
            // 右側と左側でどちらが渓谷の壁に当たらず開けているかを判定
            Vector3 candRight = bossHeadLocal + bossRight * 12.0f + bossUp * 8.5f + bossFwd * 46.0f;
            Vector3 candLeft  = bossHeadLocal - bossRight * 12.0f + bossUp * 8.5f + bossFwd * 46.0f;
            bool clearRight = IsWorldPointClearOfTerrain(LocalToWorld(candRight), 3.5f);
            bool clearLeft  = IsWorldPointClearOfTerrain(LocalToWorld(candLeft), 3.5f);

            if (clearRight && !clearLeft) {
                chosenCamSide_ = 1.0f;
                resolvedFrontCamPos_ = candRight;
            } else if (clearLeft && !clearRight) {
                chosenCamSide_ = -1.0f;
                resolvedFrontCamPos_ = candLeft;
            } else {
                // 両方クリア、または両方当たる場合は、より少ない回避移動で済む側を採用
                Vector3 safeR = ResolveSafeCameraLocalPos(candRight, anchorOrigin, pRight, pUp, pFwd, bossHeadLocal, bossFwd);
                Vector3 safeL = ResolveSafeCameraLocalPos(candLeft, anchorOrigin, pRight, pUp, pFwd, bossHeadLocal, bossFwd);
                float costR = (safeR - candRight).Length();
                float costL = (safeL - candLeft).Length();
                if (costR <= costL) {
                    chosenCamSide_ = 1.0f;
                    resolvedFrontCamPos_ = safeR;
                } else {
                    chosenCamSide_ = -1.0f;
                    resolvedFrontCamPos_ = safeL;
                }
            }
        }

        float tPull = std::clamp(phaseTimer_ / 0.80f, 0.0f, 1.0f);
        float easePull = tPull * tPull * (3.0f - 2.0f * tPull);
        Vector3 camPos = cinematicCamStartPos_ + (resolvedFrontCamPos_ - cinematicCamStartPos_) * easePull;

        Vector3 lookTarget = bossCenterLocal + (bossHeadLocal - bossCenterLocal) * (0.65f * easePull);

        flybyCamPos_ = camPos;
        flybyLookStart_ = lookTarget;
        railCam_->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
    }

    // 2. 崩壊シャード（キューブ・ポリゴン）の拡散更新
    for (auto& s : collapseShards_) {
        s.life += dt;
        s.position = s.position + s.velocity * dt;
        s.rotation = s.rotation + s.rotSpeed * dt;
        s.velocity.y += 2.0f * dt;

        float alpha = std::clamp(1.0f - s.life / s.maxLife, 0.0f, 1.0f);
        s.color.w = alpha;

        if (s.obj) {
            s.obj->SetTranslate(s.position);
            s.obj->SetRotation(s.rotation);
            s.obj->SetColor(s.color);
            s.obj->Update();
        }
    }

        // 0.8秒経過で、ボス実体を残したままフェーズ3（ボスの真上通過＆2車両目後半通過後に消滅開始）へ移行
        if (phaseTimer_ >= 0.80f) {
            phase_ = Phase::PHASE_5_MISSION_ACCOMPLISHED;
            phaseTimer_ = 0.0f;
            hasDissolveStarted_ = false;
            dissolveTimer_ = 0.0f;
            voxelSpawnAccum_ = 0.0f;
            wingTrailParticles_.clear();
            if (player) {
                player->SetVisible(true); // ボスの上を通るシーンから自機を再表示
            }
        }
}

void BossFinisherSequence::UpdatePhase5MissionAccomplished(float dt, ArmoredTrainBoss* boss, Player* player) {
    // =========================================================================
    // 【カメラフェーズ3：ローリング通過＆ボス一斉消滅 ➔ 上空フライバイ (計4.8s)】
    // ・プレイヤーが2車両目の後半を通り過ぎたタイミングでボス全車両の消滅が一斉に始まる
    // ・消滅中は上から細かいデジタル粒子・ボクセルブロックが上方向へ立ち昇る
    // =========================================================================
    if (railCam_ && player && boss) {
        player->SetVisible(true);
        // まず回転を0に戻した状態でレールアンカーの純粋なワールド基底ベクトルを取得
        player->SetCutsceneOverride(true, playerAscentStartPos_, { 0.0f, 0.0f, 0.0f });
        player->Update3DObjectOnly();

        Vector3 pLocal = playerAscentStartPos_;
        Vector3 pWorld = player->GetWorldPosition();
        Vector3 pRight = player->GetWorldRight();
        Vector3 pUp    = player->GetWorldUp();
        Vector3 pFwd   = player->GetWorldForward();
        Vector3 anchorOrigin = pWorld - (pRight * pLocal.x + pUp * pLocal.y + pFwd * pLocal.z);

        auto WorldToAnchorLocal = [&](const Vector3& wPos) -> Vector3 {
            Vector3 diff = wPos - anchorOrigin;
            return {
                Vector3::Dot(diff, pRight),
                Vector3::Dot(diff, pUp),
                Vector3::Dot(diff, pFwd)
            };
        };

        Vector3 bossHeadLocal = WorldToAnchorLocal(boss->GetCarriagePosition(0) + Vector3(0.0f, 5.5f, 0.0f));
        Vector3 bossTailLocal = WorldToAnchorLocal(boss->GetCarriagePosition(3) + Vector3(0.0f, 5.5f, 0.0f));

        // ボスの実際の進行方向と左右軸（カーブやどの向きにいても常にボスの真上を通る）
        Vector3 bossFwd = bossHeadLocal - bossTailLocal;
        bossFwd.y = 0.0f;
        float bLen = bossFwd.Length();
        if (bLen > 0.001f) bossFwd = bossFwd / bLen;
        else bossFwd = { 0.0f, 0.0f, 1.0f };
        Vector3 bossRight = { bossFwd.z, 0.0f, -bossFwd.x };
        Vector3 bossUp = { 0.0f, 1.0f, 0.0f };

        // 固定ルート（ボス進行方向基準）：
        // 前半 (0.0s〜3.5s): ボスの真上（ボスの右+5m, 高さ+11.7m）を一直線に通過し、さらに前方（+60m）まで水平飛行を続ける
        // 後半 (3.5s〜4.8s): 前半の水平ベクトル・速度をそのまま接線として引き継ぎ（C1連続）、滑らかに上空へ上昇して飛び去る
        Vector3 shipStart    = bossTailLocal + bossRight * 5.0f + bossUp * 11.7f - bossFwd * 22.0f;
        Vector3 shipOverHead = bossHeadLocal + bossRight * 5.0f + bossUp * 11.7f + bossFwd * 60.0f;
        Vector3 horizVec = shipOverHead - shipStart;
        float horizLen = horizVec.Length();
        Vector3 horizUnit = (horizLen > 0.001f) ? (horizVec / horizLen) : bossFwd;
        float horizSpeed = horizLen / 3.50f;
        // 2次ベジェ曲線の t2=0 における接線速度 (2 * |P1 - P0| / 1.30s) を前半の水平速度に一致させる
        float midTangentDist = (horizSpeed * 1.30f) * 0.5f;
        Vector3 shipPassMid  = shipOverHead + horizUnit * midTangentDist;
        Vector3 shipSkyEnd   = shipOverHead + horizUnit * (midTangentDist * 1.65f) + bossUp * 46.3f;

        Vector3 shipPos = shipStart;
        Vector3 moveDir = shipOverHead - shipStart;
        float rollAngle = 0.0f;

        if (phaseTimer_ < 3.50f) {
            float t1 = std::clamp(phaseTimer_ / 3.50f, 0.0f, 1.0f);
            shipPos = shipStart + (shipOverHead - shipStart) * t1;
            moveDir = shipOverHead - shipStart;
            // 先頭車両（1車両目）に入るまで (0.0s〜1.50s) は水平姿勢のまま直進し、
            // 先頭車両に入ったタイミング (1.50s〜2.80s) でゆっくり1回転バレルロールを行う
            float tRoll = std::clamp((phaseTimer_ - 1.50f) / 1.30f, 0.0f, 1.0f);
            float easeRoll = tRoll * tRoll * (3.0f - 2.0f * tRoll);
            rollAngle = easeRoll * (3.14159265f * 2.0f);
        } else {
            float t2 = std::clamp((phaseTimer_ - 3.50f) / 1.30f, 0.0f, 1.0f);
            float invT2 = 1.0f - t2;
            shipPos = shipOverHead * (invT2 * invT2) + shipPassMid * (2.0f * invT2 * t2) + shipSkyEnd * (t2 * t2);

            // 2次ベジェ曲線の正確な接線ベクトルを使用（t2=1.0f 終端でもゼロベクトルにならず上向き姿勢を維持）
            moveDir = (shipPassMid - shipOverHead) * (2.0f * invT2) + (shipSkyEnd - shipPassMid) * (2.0f * t2);
            rollAngle = 0.0f; // 上昇中はローリングせず水平姿勢のまま上昇
        }

        // クォータニオンで「進行方向を向く姿勢」×「機体ローカル前方Z軸周りの純粋なバレルロール」を合成
        Quaternion baseOrientation = Quaternion::LookRotation(moveDir, { 0.0f, 1.0f, 0.0f });
        Quaternion barrelRoll = Quaternion::FromAxisAngle({ 0.0f, 0.0f, 1.0f }, rollAngle);
        Quaternion shipQuat = Quaternion::Multiply(baseOrientation, barrelRoll);

        player->SetCutsceneOverrideQuaternion(true, shipPos, shipQuat);
        player->Update3DObjectOnly();

        // ローリング完了（1.50s〜2.80s）に合わせてカメラを少しプレイヤー側へ近づけて機体全体を見せる（地形貫通回避付き）
        Vector3 shipNearHead = bossHeadLocal + bossRight * 5.0f + bossUp * 12.7f + bossFwd * 10.0f;
        Vector3 desiredClosePos = flybyCamPos_ + (shipNearHead - flybyCamPos_) * 0.38f;
        Vector3 camClosePos = ResolveSafeCameraLocalPos(desiredClosePos, anchorOrigin, pRight, pUp, pFwd, bossHeadLocal, bossFwd);
        float camApproachT = std::clamp((phaseTimer_ - 0.85f) / 0.65f, 0.0f, 1.0f);
        float easeApproach = camApproachT * camApproachT * (3.0f - 2.0f * camApproachT);
        Vector3 curCamPos = flybyCamPos_ + (camClosePos - flybyCamPos_) * easeApproach;

        // 向き（LookAt）は最初から素早く（0.15sで）ボスの真上を飛ぶプレイヤーへ合わせて2回転すべてを見せる
        float lockOnPlayerT = std::clamp(phaseTimer_ / 0.15f, 0.0f, 1.0f);
        float smoothLock = lockOnPlayerT * lockOnPlayerT * (3.0f - 2.0f * lockOnPlayerT);
        Vector3 lookTarget = flybyLookStart_ + (shipPos - flybyLookStart_) * smoothLock;
        railCam_->SetCinematicCamera(true, curCamPos, lookTarget, 1.0f);

        playerAscentPos_ = shipPos;
        playerAscentRot_ = { 0.0f, 0.0f, rollAngle };

        // --- フェーズ3に入った瞬間からデジタル・グリッド・ディゾルブを開始し、3.90秒かけてゆっくり消滅させて帳尻を合わせる ---
        hasDissolveStarted_ = true;
        if (hasDissolveStarted_) {
            dissolveTimer_ += dt;
            float dissolveProgress = std::clamp(dissolveTimer_ / 3.90f, 0.0f, 1.0f);
            boss->SetDissolveAlpha(1.0f - dissolveProgress);

            // シェーダーのディゾルブ・スキャンライン高度 (lerp(18.0f, -20.0f, dissolveProgress)) と同期し、
            // 実際にモデル（高さ約17.5m）が上から消え始めてからパーティクルを発生させる
            Vector3 camWorldPos = anchorOrigin + pRight * curCamPos.x + pUp * curCamPos.y + pFwd * curCamPos.z;
            float scanWorldY = camWorldPos.y + (18.0f - 38.0f * dissolveProgress);
            float bossBaseY = boss->GetCarriagePosition(0).y;
            float bossTopWorldY = bossBaseY + 17.5f;
            float bossBottomWorldY = bossBaseY - 1.0f;
            bool isModelDissolving = (scanWorldY <= bossTopWorldY + 1.5f) && (scanWorldY >= bossBottomWorldY - 4.5f);

            // 消えている最中：上方向に細かい粒子やボクセルブロックを大量に立ち昇らせる
            if (isModelDissolving && dissolveProgress < 0.96f) {
                voxelSpawnAccum_ += dt;
                while (voxelSpawnAccum_ >= 0.02f) {
                    voxelSpawnAccum_ -= 0.02f;
                    for (int carIdx = 0; carIdx < 4; ++carIdx) {
                        for (int n = 0; n < 3; ++n) {
                            if (collapseShards_.size() >= 420) break;
                            Vector3 carWorld = boss->GetCarriagePosition(carIdx);
                            CollapseShard s;
                            s.isCube = true;
                            s.obj = std::make_unique<Object3d>();
                            s.obj->Initialize(object3dCommon_);
                            s.obj->SetModel("cube.obj");

                            // ボス本体のカラー（エレクトリックバイオレット・サイバーインディゴ）＆ディゾルブ境界色（シアン・マゼンタ）に統一
                            int colPick = rand() % 5;
                            if (colPick <= 1) s.color = { 0.76f, 0.14f, 1.0f, 1.0f };       // エレクトリックバイオレット（ボス主力色）
                            else if (colPick == 2) s.color = { 0.22f, 0.48f, 1.0f, 1.0f };  // サイバーインディゴ（ボスサブ色）
                            else if (colPick == 3) s.color = { 0.05f, 0.95f, 1.0f, 1.0f };  // ネオンシアン（ディゾルブグリッド色）
                            else s.color = { 1.0f, 0.18f, 0.60f, 1.0f };                    // ネオンマゼンタ（ディゾルブレーザー色）

                            s.obj->SetColor(s.color);
                            s.obj->SetEnableLighting(false);
                            s.obj->SetEnvironmentCoefficient(0.0f);
                            s.obj->SetSelectLightings(10); // 純粋自己発光シェーダーを使用

                            // 細かい粒子(0.55〜0.95)〜ボクセルブロック(1.0〜1.65)のサイズ混在
                            float sz = (n == 0) ? (1.0f + (rand() % 10) * 0.065f) : (0.55f + (rand() % 10) * 0.04f);
                            s.scale = { sz, sz, sz };
                            s.obj->SetScale(s.scale);

                            // 実際にモデルが消えているスキャンライン高度付近から上向きに生成
                            float scanY = std::clamp(scanWorldY - carWorld.y, 0.5f, 17.5f);
                            Vector3 offset = {
                                ((rand() % 100) - 50) * 0.14f,
                                scanY + ((rand() % 100) - 50) * 0.04f,
                                ((rand() % 100) - 50) * 0.28f
                            };
                            s.position = carWorld + offset;
                            // 上方向へ勢いよく立ち昇る速度
                            s.velocity = {
                                ((rand() % 100) - 50) * 0.06f,
                                14.0f + (rand() % 100) * 0.14f,
                                ((rand() % 100) - 50) * 0.06f
                            };
                            s.rotation = { (rand() % 628) * 0.01f, (rand() % 628) * 0.01f, (rand() % 628) * 0.01f };
                            s.rotSpeed = {
                                ((rand() % 100) - 50) * 0.08f,
                                ((rand() % 100) - 50) * 0.08f,
                                ((rand() % 100) - 50) * 0.08f
                            };
                            s.life = 0.0f;
                            s.maxLife = 1.0f + (rand() % 10) * 0.07f;
                            s.obj->SetTranslate(s.position);
                            s.obj->SetRotation(s.rotation);
                            s.obj->Update();
                            collapseShards_.push_back(std::move(s));
                        }
                    }
                }
            }

            if (dissolveProgress >= 1.0f) {
                boss->SetActive(false);
            }
        }
    }

    // 崩壊ボクセル・デジタル粒子の更新（上空へ昇りながら縮小・フェードアウト）
    for (auto it = collapseShards_.begin(); it != collapseShards_.end(); ) {
        auto& s = *it;
        s.life += dt;
        if (s.life >= s.maxLife) {
            it = collapseShards_.erase(it);
            continue;
        }
        s.position = s.position + s.velocity * dt;
        s.rotation = s.rotation + s.rotSpeed * dt;
        s.velocity.y += 4.5f * dt; // 上方向へ加速しながら霧散
        float alpha = std::clamp(1.0f - s.life / s.maxLife, 0.0f, 1.0f);
        s.color.w = 1.0f;
        if (s.obj) {
            s.obj->SetTranslate(s.position);
            s.obj->SetRotation(s.rotation);
            s.obj->SetColor(s.color);
            s.obj->SetScale({ s.scale.x * alpha, s.scale.y * alpha, s.scale.z * alpha });
            s.obj->Update();
        }
        ++it;
    }

    // 4.80秒経過で完了（COMPLETED）し、任務完了UI展開フェーズへ引き渡す
    if (phaseTimer_ >= 4.80f) {
        if (boss) boss->SetActive(false);
        phase_ = Phase::COMPLETED;
        phaseTimer_ = 0.0f;
        if (player) {
            player->SetExecutionMode(false);
        }
        if (auto textMgr = UITextManager::GetInstance()) {
            textMgr->SetVisible("ExecuteCritical", false, "GAMEPLAY");
            textMgr->SetVisible("ExecuteGuide", false, "GAMEPLAY");
            textMgr->SetVisible("MissionAccomplishedTitle", false, "GAMEPLAY");
            textMgr->SetVisible("MissionAccomplishedSub", false, "GAMEPLAY");
        }
    }
}

void BossFinisherSequence::Draw3D() {
    if (phase_ == Phase::INACTIVE) return;

    if (object3dCommon_) {
        object3dCommon_->SetCommonDrawSetting();
    }
    if (auto srvMgr = SrvManager::GetInstance()) {
        srvMgr->PreDraw();
    }

    // ※弱点コアの3D球体（黄色い弾に見えるオブジェクト）は邪魔にならないよう非表示にし、2Dの電脳ヘキサゴン照準で表現

    // 赤データメモリ
    if (phase_ == Phase::PHASE_1_TRANSITION && !isRedMemoryAbsorbed_ && redMemoryObj_) {
        redMemoryObj_->Draw();
    }

    // チャージ粒子
    if (phase_ == Phase::PHASE_2_AUTO_LOCKON) {
        for (const auto& cp : chargeParticles_) {
            if (cp.obj) cp.obj->Draw();
        }
    }

    // 処刑ミサイル
    if (phase_ == Phase::PHASE_3_FINISHER_FIRE) {
        for (const auto& m : missiles_) {
            if (!m.isHit && m.obj) {
                m.obj->Draw();
            }
        }
        for (const auto& ring : shockwaveRings_) {
            if (ring) ring->Draw();
        }
    }

    // 崩壊シャード・翼端螺旋トレイル粒子
    if (phase_ == Phase::PHASE_4_DATA_COLLAPSE || phase_ == Phase::PHASE_5_MISSION_ACCOMPLISHED) {
        for (const auto& s : collapseShards_) {
            if (s.obj && s.color.w > 0.01f) {
                s.obj->Draw();
            }
        }
        for (const auto& wp : wingTrailParticles_) {
            if (wp.obj && wp.color.w > 0.01f) {
                wp.obj->Draw();
            }
        }
    }
}

void BossFinisherSequence::DrawUI() {
    if (phase_ == Phase::INACTIVE) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;
    ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
    if (!drawList) return;

    // ゲーム画面のビューポート領域を取得（エディタモード／全画面両対応）
    ImVec2 viewPos(0.0f, 0.0f);
    ImVec2 viewSize = viewport->Size;
#ifdef ENABLE_EDITOR
    if (EngineServices::GetInstance()->GetEditorMode()) {
        viewPos = EditorSystem::GetInstance()->GetViewportPos();
        viewSize = EditorSystem::GetInstance()->GetViewportSize();
        if (viewSize.x <= 10.0f || viewSize.y <= 10.0f) return;
    }
#endif

    float screenW = viewSize.x;
    float screenH = viewSize.y;
    float scaleX = screenW / 1280.0f;
    float scaleY = screenH / 720.0f;
    float uniformScale = std::min(scaleX, scaleY);
    if (uniformScale < 0.35f) uniformScale = 0.35f;

    drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

    auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
    auto textMgr = UITextManager::GetInstance();

    // -------------------------------------------------------------
    // 処刑ミサイル発射後（フェーズ③〜⑤）：シネマティック・ムービー上下黒帯（レターボックス）
    // -------------------------------------------------------------
    if (IsMoviePhase() && phase_ != Phase::COMPLETED) {
        float barHeightMax = screenH * 0.12f;
        float barProgress = 1.0f;
        if (phase_ == Phase::PHASE_3_FINISHER_FIRE) {
            barProgress = std::clamp(phaseTimer_ / 0.25f, 0.0f, 1.0f);
        }
        float barHeight = barHeightMax * barProgress;
        if (barHeight > 0.5f) {
            drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barHeight), IM_COL32(0, 0, 0, 255));
            drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barHeight), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
        }
    }

    // -------------------------------------------------------------
    // ① チャージ進行バー（スペース押下時のみ表示）
    // ※警告・ガイドテキストは UITextManager (エディタ一括管理) により描画
    // -------------------------------------------------------------
    if (phase_ == Phase::PHASE_2_AUTO_LOCKON && spaceHoldTimer_ > 0.0f) {
        float barW = 320.0f * uniformScale;
        float barH = 12.0f * uniformScale;
        float barX = viewPos.x + screenW * 0.5f - barW * 0.5f;
        float barY = viewPos.y + screenH * 0.72f;
        float holdRate = std::clamp(spaceHoldTimer_ / kRequiredHoldTime_, 0.0f, 1.0f);
        float pulse = 0.7f + 0.3f * std::sin(phaseTimer_ * 14.0f);

        ImU32 fillCol = isAllCoresLocked_ ?
            IM_COL32(255, (int)(215 + 40 * pulse), 40, 255) :
            IM_COL32(255, 40, 160, 240);

        drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(10, 20, 30, 200), 3.0f);
        drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * holdRate, barY + barH), fillCol, 3.0f);
        drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), isAllCoresLocked_ ? fillCol : IM_COL32(0, 220, 255, 220), 3.0f, 0, 1.5f);
    }

    // 【電脳仮想空間・処刑オーバーライド照準】スペース押下でヘキサゴンが順次展開・収束（発射前のみ表示）
    // -------------------------------------------------------------
    if (phase_ == Phase::PHASE_2_AUTO_LOCKON && camera_) {
        const Matrix4x4& vMat = camera_->GetViewMatrix();
        const Matrix4x4& pMat = camera_->GetProjectionMatrix();
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

        // 自機のスクリーン座標（処刑データストリーム射出起点）
        ImVec2 ptShip(viewPos.x + screenW * 0.5f, viewPos.y + screenH * 0.80f);
        if (player_) {
            auto [okShip, ptS] = WorldToScreen(player_->GetWorldPosition());
            if (okShip) ptShip = ptS;
        }

        ImFont* fontMplus = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::Japanese_MPLUS) : ImGui::GetFont();
        if (!fontMplus) fontMplus = ImGui::GetFont();

        // 処刑専用：電脳ヘキサゴン（六角形ハニカムゲート）描画ラムダ
        auto DrawCyberHexagon = [&](const ImVec2& c, float radius, float rotAngle, ImU32 col, float thick) {
            ImVec2 pts[6];
            for (int k = 0; k < 6; ++k) {
                float a = rotAngle + k * (3.14159265f / 3.0f);
                pts[k] = ImVec2(c.x + std::cos(a) * radius, c.y + std::sin(a) * radius);
            }
            for (int k = 0; k < 6; ++k) {
                drawList->AddLine(pts[k], pts[(k + 1) % 6], col, thick);
            }
        };

        // 処刑専用：外側サイバーノッチリング（反時計回りダイヤル目盛り）描画ラムダ
        auto DrawDialNotches = [&](const ImVec2& c, float radius, float rotAngle, ImU32 col, float notchLen, float thick) {
            drawList->AddCircle(c, radius, col, 24, thick * 0.75f);
            for (int k = 0; k < 8; ++k) {
                float a = rotAngle + k * (3.14159265f / 4.0f);
                float cosA = std::cos(a);
                float sinA = std::sin(a);
                ImVec2 p0(c.x + cosA * (radius - notchLen), c.y + sinA * (radius - notchLen));
                ImVec2 p1(c.x + cosA * (radius + notchLen), c.y + sinA * (radius + notchLen));
                drawList->AddLine(p0, p1, col, thick);
            }
        };

        // スペースを押している時、または発射後のみ処刑照準を表示（押す前は非表示）
        if (spaceHoldTimer_ > 0.0f || phase_ == Phase::PHASE_3_FINISHER_FIRE) {
            for (size_t i = 0; i < cores_.size(); ++i) {
                const auto& core = cores_[i];
                auto [okCore, ptCore] = WorldToScreen(core.worldPos);
                if (!okCore) continue;

                // まだこのコアのロックオン順番が回っていない時は非表示
                if (core.lockProgress <= 0.001f && !core.isLocked) {
                    continue;
                }

                float baseSize = 32.0f * uniformScale;
                float strokeW = 2.4f * uniformScale;
                float shadowStroke = strokeW + 2.5f;
                float pulse = 0.6f + 0.4f * std::sin(phaseTimer_ * 14.0f + i * 1.2f);
                ImU32 shadowCol = IM_COL32(0, 0, 0, 230);

                if (core.isLocked) {
                    // =========================================================
                    // 【処刑確定ロックオン】：電脳ヘキサゴン＋多重ダイヤル＋データストリーム
                    // =========================================================
                    float snap = core.snapTimer; // 1.0 -> 0.0
                    float currentSize = baseSize * (1.0f + 0.5f * snap);

                    ImU32 hexCol = isAllCoresLocked_ ?
                        IM_COL32(255, (int)(215 + 40 * pulse), 40, 255) : // 全完了時は金熱発光
                        IM_COL32(255, 30, 140, 255);                      // ネオンマゼンタ
                    ImU32 dialCol = isAllCoresLocked_ ?
                        IM_COL32(255, 235, 100, 240) :
                        IM_COL32(0, 230, 255, (int)(200 + 55 * pulse));  // サイバーシアン

                    // 1. 内側：時計回りにゆっくり旋回する電脳ヘキサゴン
                    float hexAngle = phaseTimer_ * 2.0f + i * 0.5f;
                    DrawCyberHexagon(ptCore, currentSize, hexAngle, shadowCol, shadowStroke);
                    DrawCyberHexagon(ptCore, currentSize, hexAngle, hexCol, strokeW);

                    // 3. 外側：反時計回りに回転するダイヤル目盛りリング
                    float dialAngle = -phaseTimer_ * 1.5f;
                    DrawDialNotches(ptCore, currentSize * 1.35f, dialAngle, shadowCol, 4.0f * uniformScale, shadowStroke);
                    DrawDialNotches(ptCore, currentSize * 1.35f, dialAngle, dialCol, 4.0f * uniformScale, strokeW * 0.85f);

                    // 4. 中心：高エネルギー処刑クロスヘア（十文字）
                    float crossSize = 7.0f * uniformScale;
                    drawList->AddLine(ImVec2(ptCore.x - crossSize, ptCore.y), ImVec2(ptCore.x + crossSize, ptCore.y), shadowCol, strokeW + 2.0f);
                    drawList->AddLine(ImVec2(ptCore.x, ptCore.y - crossSize), ImVec2(ptCore.x, ptCore.y + crossSize), shadowCol, strokeW + 2.0f);
                    drawList->AddLine(ImVec2(ptCore.x - crossSize, ptCore.y), ImVec2(ptCore.x + crossSize, ptCore.y), hexCol, strokeW);
                    drawList->AddLine(ImVec2(ptCore.x, ptCore.y - crossSize), ImVec2(ptCore.x, ptCore.y + crossSize), hexCol, strokeW);
                    drawList->AddCircleFilled(ptCore, 2.5f * uniformScale, IM_COL32(255, 255, 255, 255));

                    // 5. 上部サイバーラベル「EXECUTE LOCK [01]」
                    char label[48];
                    snprintf(label, sizeof(label), "[ EXECUTE LOCK #%d ]", static_cast<int>(i + 1));
                    float lFontSize = 13.5f * uniformScale;
                    ImVec2 lSize = fontMplus->CalcTextSizeA(lFontSize, FLT_MAX, -1.0f, label);
                    ImVec2 lPos(ptCore.x - lSize.x * 0.5f, ptCore.y - currentSize * 1.35f - lSize.y - 6.0f * uniformScale);

                    // ラベル背景の半透明ミニプレート
                    drawList->AddRectFilled(
                        ImVec2(lPos.x - 4.0f * uniformScale, lPos.y - 1.0f),
                        ImVec2(lPos.x + lSize.x + 4.0f * uniformScale, lPos.y + lSize.y + 1.0f),
                        IM_COL32(10, 2, 20, 190), 2.0f
                    );
                    drawList->AddText(fontMplus, lFontSize, ImVec2(lPos.x + 1.2f, lPos.y + 1.2f), shadowCol, label);
                    drawList->AddText(fontMplus, lFontSize, lPos, hexCol, label);
                } else {
                    // =========================================================
                    // 【処刑オーバーライド接近中】：自機からヘキサゴンが旋回飛来
                    // =========================================================
                    float t = core.lockProgress; // 0.0 -> 1.0
                    float s = t * t * (3.0f - 2.0f * t); // スムーズステップ

                    // 自機から扇状に飛び出す中間制御点（ベジェ曲線）
                    float sweepSpread = 180.0f * uniformScale;
                    ImVec2 ptMid(
                        (ptShip.x + ptCore.x) * 0.5f + std::sin(core.startAngle) * sweepSpread,
                        (ptShip.y + ptCore.y) * 0.5f - std::abs(std::cos(core.startAngle)) * sweepSpread * 0.65f
                    );

                    // 2次ベジェ曲線上の現在位置
                    float invS = 1.0f - s;
                    ImVec2 curPt(
                        invS * invS * ptShip.x + 2.0f * invS * s * ptMid.x + s * s * ptCore.x,
                        invS * invS * ptShip.y + 2.0f * invS * s * ptMid.y + s * s * ptCore.y
                    );

                    // ヘキサゴンサイズ：最初は巨大、コアに近づくにつれて急激に収束
                    float currentSize = baseSize * (2.8f - 1.8f * s);
                    float hexAngle = s * 6.28f + phaseTimer_ * 3.0f; // 飛来中に激しくスピン

                    // 軌跡ライン（自機からファンネル現在位置へ流れるネオン深紅・マゼンタライン）
                    ImU32 trailCol = IM_COL32(255, 40, (int)(80 + 175 * s), (int)(200 * (1.0f - s * 0.4f)));
                    drawList->AddLine(ptShip, curPt, IM_COL32(0, 0, 0, 80), 2.5f * uniformScale);
                    drawList->AddLine(ptShip, curPt, trailCol, 1.4f * uniformScale);

                    // 目標コアへのロック誘導破線
                    drawList->AddLine(curPt, ptCore, IM_COL32(0, 240, 255, (int)(150 * s)), 1.2f * uniformScale);

                    // 接近中の回転ヘキサゴン
                    DrawCyberHexagon(curPt, currentSize, hexAngle, shadowCol, shadowStroke);
                    DrawCyberHexagon(curPt, currentSize, hexAngle, trailCol, strokeW);

                    // 外側収束リング
                    float ringR = baseSize * (3.6f - 2.5f * s);
                    drawList->AddCircle(curPt, ringR, IM_COL32(0, 240, 255, (int)(220 * s)), 20, 1.6f * uniformScale);

                    // カウントアップするオーバーライド率（例: OVERRIDE 65%）
                    char seekText[48];
                    int percent = static_cast<int>(s * 99.0f);
                    snprintf(seekText, sizeof(seekText), "OVERRIDE %d%%", percent);
                    float sFontSize = 12.0f * uniformScale;
                    ImVec2 sTextSize = fontMplus->CalcTextSizeA(sFontSize, FLT_MAX, -1.0f, seekText);
                    ImVec2 sPos(curPt.x - sTextSize.x * 0.5f, curPt.y - currentSize - sTextSize.y - 4.0f * uniformScale);
                    drawList->AddText(fontMplus, sFontSize, ImVec2(sPos.x + 1.0f, sPos.y + 1.0f), shadowCol, seekText);
                    drawList->AddText(fontMplus, sFontSize, sPos, trailCol, seekText);
                }
            }
        }
    }

    // -------------------------------------------------------------
    // ⑤ 任務完了表示は UITextManager (エディタ一括管理) によりビューポート内に描画されます
    // -------------------------------------------------------------

    drawList->PopClipRect();
}

void BossFinisherSequence::DrawImGuiDebug(ArmoredTrainBoss* boss, Player* player) {
    if (ImGui::CollapsingHeader("クリア演出・電脳フィニッシャー", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* phaseName = "通常戦闘中";
        switch (phase_) {
        case Phase::PHASE_1_TRANSITION: phaseName = "① 処刑移行（赤メモリ飛来・換装）"; break;
        case Phase::PHASE_2_AUTO_LOCKON: phaseName = "② 自動ロックオン・チャージ待機中"; break;
        case Phase::PHASE_3_FINISHER_FIRE: phaseName = "③ 処刑ミサイル一斉発射"; break;
        case Phase::PHASE_4_DATA_COLLAPSE: phaseName = "④ 処刑カメラ＆データ崩壊"; break;
        case Phase::PHASE_5_MISSION_ACCOMPLISHED: phaseName = "⑤ 任務完了表示"; break;
        case Phase::COMPLETED: phaseName = "シークエンス完了（リザルトへ）"; break;
        default: break;
        }
        ImGui::Text("現在のフェーズ: %d (%s)", static_cast<int>(phase_), phaseName);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.15f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.25f, 0.50f, 1.0f));
        if (ImGui::Button("【処刑テスト】ボスHPを1にする（処刑可能化 & プレイヤー射撃へ）", ImVec2(-1, 38))) {
            if (boss) {
                boss->SetHpToOneForExecution();
                boss->SetActive(true);
            }
            StartSequence(boss, player);
        }
        ImGui::PopStyleColor(2);

        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "※押すとボスHPが1になり、赤メモリが自機に飛来して特殊弾へ換装されます。");
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "  その後、[SPACEキー長押し] で弱点コアをロックオンし、離して発射できます。");

        ImGui::Spacing();
        if (ImGui::Button("停止・通常状態へリセット", ImVec2(240, 28))) {
            Reset(player);
        }

        ImGui::Separator();
        ImGui::Text("各フェーズ直接ジャンプ:");
        if (ImGui::Button("① 処刑移行")) { phase_ = Phase::PHASE_1_TRANSITION; phaseTimer_ = 0.0f; }
        ImGui::SameLine();
        if (ImGui::Button("② ロックオン")) { phase_ = Phase::PHASE_2_AUTO_LOCKON; phaseTimer_ = 0.0f; spaceHoldTimer_ = 0.0f; }
        ImGui::SameLine();
        if (ImGui::Button("③ ミサイル発射")) { FireExecutionMissiles(player); phase_ = Phase::PHASE_3_FINISHER_FIRE; phaseTimer_ = 0.0f; }
        ImGui::SameLine();
        if (ImGui::Button("④ データ崩壊")) { phase_ = Phase::PHASE_4_DATA_COLLAPSE; phaseTimer_ = 0.0f; SpawnCollapseShards(boss); }
        ImGui::SameLine();
        if (ImGui::Button("⑤ 任務完了")) { phase_ = Phase::PHASE_5_MISSION_ACCOMPLISHED; phaseTimer_ = 0.0f; }
    }
}
