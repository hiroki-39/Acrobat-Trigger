#pragma once
#include "IWeapon.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include <array>
#include <vector>

/// <summary>
/// ビット兵器（サイバー粒子マルチロックオン）
/// 照準を合わせた複数の敵を自動ロックオンし、追尾粒子ビットを一斉射出する
/// </summary>
class CyberBitWeapon : public IWeapon {
public:
    static const int MAX_TARGETS = 8;

    struct LockOnTarget {
        Enemy* enemy = nullptr;
        ArmoredTrainBoss* boss = nullptr;
        int carriageIndex = -1;
        float lockedTime = 0.0f;
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
    float GetRemainingRatio() const override;
    std::string GetAmmoText() const override;

    int GetAmmo() const { return ammo_; }
    int GetMaxAmmo() const { return maxAmmo_; }

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    int ammo_ = 24;            // 現在の残弾数（粒子数）
    int maxAmmo_ = 24;         // 最大弾数
    int maxSimultaneous_ = 4;  // 同時ロックオン可能数（初期4、強化で8）

    std::array<std::unique_ptr<Object3d>, MAX_TARGETS> mountedBits_;
    std::array<std::unique_ptr<Object3d>, MAX_TARGETS> lockOnReticles_;
    std::vector<LockOnTarget> multiLockedEnemies_;

    float reloadTimer_ = 0.0f;
    float reloadTime_ = 60.0f;        // 斉射後のディレイ
    float lockOnAnimTimer_ = 0.0f;
    float lockOnDelayTimer_ = 0.0f;
    float lockOnCompleteTime_ = 20.0f; // ロックオン完了所要時間
    float lockOnInterval_ = 20.0f;     // 次の敵をロックするまでの間隔
    float lockOnMaxDistance_ = 350.0f; // 最大射程
};
