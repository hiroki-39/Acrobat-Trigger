#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <memory>
#include <vector>

class Enemy;
class ArmoredTrainBoss;

/// <summary>
/// ファンネル兵器（オールレンジ・サイバービット / 4基展開）
/// 自機周囲に4基展開し、敵を索敵すると敵へ飛翔・包囲してビームを連射後に帰還する
/// </summary>
class FunnelWeapon : public IWeapon {
public:
    enum class BitState {
        IDLE_HOVER, // 自機周囲で待機・追従
        APPROACH,   // 敵へ向かって高速飛翔
        ATTACKING,  // 敵の周囲で射撃攻撃
        RETURNING   // 自機へ帰還
    };

    struct FunnelBit {
        BitState state = BitState::IDLE_HOVER;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 velocity = { 0.0f, 0.0f, 0.0f };
        Vector3 rotation = { 0.0f, 0.0f, 0.0f };

        // 各パーツの回転角度（バラバラに自転）
        float coreRotY = 0.0f;
        float coreRotSpeed = 0.06f;
        float ring1RotZ = 0.0f;
        float ring1RotSpeed = 0.09f;
        float ring2RotZ = 0.0f;
        float ring2RotSpeed = -0.07f; // 逆回転

        // 目標位置（敵包囲位置または自機追従位置）
        Vector3 targetStationPos = { 0.0f, 0.0f, 0.0f };

        // 攻撃シーケンス用
        int shotsFired = 0;
        int maxShots = 3;         // 敵1体につき各端末が3発射撃
        float shotTimer = 0.0f;
        float shotInterval = 18.0f; // 射撃間隔（フレーム）

        // 4パーツの3Dモデルオブジェクト
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
    bool IsDepleted() const override { return remainingTime_ <= 0.0f; }
    MemoryType GetType() const override { return MemoryType::FUNNEL; }
    float GetRemainingRatio() const override {
        return maxTime_ > 0.0f ? std::clamp(remainingTime_ / maxTime_, 0.0f, 1.0f) : 0.0f;
    }
    std::string GetAmmoText() const override;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    float remainingTime_ = 900.0f; // 15秒 (60FPS換算)
    float maxTime_ = 900.0f;
    float playerAttackTimer_ = 0.0f;
    float hoverAnimTimer_ = 0.0f;

    static constexpr size_t kFunnelCount = 4;
    std::vector<FunnelBit> funnels_;

    // 現在攻撃中のターゲット敵情報
    const Enemy* lockedEnemy_ = nullptr;
    int lockedBossCarriage_ = -1;
    Vector3 lockedTargetPos_ = { 0.0f, 0.0f, 0.0f };
    bool hasTarget_ = false;

    // ファンネルからの弾発射バッファ
    std::vector<Vector3> pendingBulletMuzzles_;
    std::vector<Vector3> pendingBulletDirs_;

    // 索敵ヘルパー
    bool FindBestTarget(const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, const Vector3& playerPos, Vector3& outTargetPos, const Enemy*& outEnemy, int& outCarriage);
};
