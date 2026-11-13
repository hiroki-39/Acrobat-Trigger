#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <array>
#include <vector>
#include <memory>
#include <string>

class Enemy;
class ArmoredTrainBoss;

/// <summary>
/// サイバービット（ファンネル）兵器
/// 自機周囲に4基展開。スペースキー長押しで敵をマルチロックオンし、
/// キー解放でファンネル端末自体が外側へ美しい弧を描いて敵へ突翔・急襲攻撃し、自機へ帰還する
/// </summary>
class CyberBitWeapon : public IWeapon {
public:
    static const int MAX_TARGETS = 8;
    static const int BIT_COUNT = 4;

    enum class BitFlightState {
        IDLE_HOVER,    // 自機周囲で待機・追従ホバリング
        ARC_OUTWARD,   // 外側へ弧を描いて展開・加速
        HOMING_ATTACK, // ターゲットへ向かって弧を描きながら急襲
        RETURNING      // 攻撃完了後、弧を描いて自機へ帰還
    };

    struct LockOnTarget {
        Enemy* enemy = nullptr;
        ArmoredTrainBoss* boss = nullptr;
        int carriageIndex = -1;
        float lockedTime = 0.0f;
    };

    struct BitUnit {
        BitFlightState state = BitFlightState::IDLE_HOVER;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 prevPosition = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 lastLookDir = { 0.0f, 0.0f, 1.0f };
        float currentScale = 0.45f;

        Enemy* targetEnemy = nullptr;
        ArmoredTrainBoss* targetBoss = nullptr;
        int targetCarriageIndex = -1;

        float flightTimer = 0.0f;
        bool hasHitTarget = false;

        // 各パーツの自転角度
        float coreRotY = 0.0f;
        float coreRotSpeed = 0.06f;
        float ring1RotZ = 0.0f;
        float ring1RotSpeed = 0.09f;
        float ring2RotZ = 0.0f;
        float ring2RotSpeed = -0.07f; // 逆回転

        // 4パーツ構成の3Dモデルオブジェクト
        std::unique_ptr<Object3d> bodyObject = nullptr;
        std::unique_ptr<Object3d> coreObject = nullptr;
        std::unique_ptr<Object3d> ring1Object = nullptr;
        std::unique_ptr<Object3d> ring2Object = nullptr;
    };

    void Initialize(Player* owner, Object3dCommon* object3dCommon) override;
    void Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* parentCamera) override;
    void Attack(Input* input,
                std::list<std::unique_ptr<PlayerBullet>>& bullets,
                std::list<std::unique_ptr<PlayerMissile>>& missiles,
                const std::list<std::unique_ptr<Enemy>>& enemies,
                ArmoredTrainBoss* boss,
                Object3d* parentCamera,
                float gameSpeed) override;
    void Draw() override;
    void DrawUI() override;
    void Reset() override;
    bool IsDepleted() const override;
    MemoryType GetType() const override { return MemoryType::CYBER_BIT; }
    float GetRemainingRatio() const override {
        return maxAmmo_ > 0 ? (static_cast<float>(ammo_) / static_cast<float>(maxAmmo_)) : 0.0f;
    }
    std::string GetAmmoText() const override;

    int GetAmmo() const override { return ammo_; }
    int GetMaxAmmo() const override { return maxAmmo_; }

    std::vector<LockOnTargetHUD> GetLockOnTargets() const override;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 12;            // 残弾数
    int maxAmmo_ = 12;         // 最大弾数
    int maxSimultaneous_ = 4;  // 同時ロックオン可能数（4基のファンネル）

    std::array<BitUnit, BIT_COUNT> bits_;
    std::array<std::unique_ptr<Object3d>, MAX_TARGETS> lockOnReticles_;
    std::vector<LockOnTarget> multiLockedEnemies_;

    float reloadTimer_ = 0.0f;
    float reloadTime_ = 30.0f;          // 斉射後のリロード待機時間
    float lockOnAnimTimer_ = 0.0f;
    float lockOnDelayTimer_ = 0.0f;
    float lockOnCompleteTime_ = 12.0f;  // ロックオン完了時間
    float lockOnInterval_ = 10.0f;      // 次の敵をロックするまでの間隔
    float lockOnMaxDistance_ = 350.0f;  // 最大射程 (m)
    float hoverAnimTimer_ = 0.0f;
    float normalShotTimer_ = 0.0f;      // 単押し通常弾タイマー
    bool spaceKeyWasPressed_ = false;   // 前フレームのキー押下状態
};
