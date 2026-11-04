#pragma once
#include "IWeapon.h"
#include "MemoryType.h"
#include <map>
#include <memory>
#include <string>

class Player;
class Input;

/// <summary>
/// 武器管理システム（メモリシステム）
/// プレイヤーの単一武器スロットを管理し、メモリ取得による変化、
/// 弾切れ・時間切れ・Rキーによるパージ（通常弾への復帰）を制御する
/// </summary>
class WeaponManager {
public:
    void Initialize(Player* owner, Object3dCommon* object3dCommon);
    
    void Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* parentCamera);

    void Attack(Input* input,
                std::list<std::unique_ptr<PlayerBullet>>& bullets,
                std::list<std::unique_ptr<PlayerMissile>>& missiles,
                const std::list<std::unique_ptr<Enemy>>& enemies,
                ArmoredTrainBoss* boss,
                Object3d* parentCamera,
                float gameSpeed);

    void Draw();
    void DrawUI();

    /// <summary>
    /// メモリを拾った時に通常弾を変化（上書き装備）
    /// </summary>
    void EquipWeapon(MemoryType type);

    /// <summary>
    /// メモリをパージ（廃棄）して通常弾に戻す（Rキーまたは弾切れ時）
    /// </summary>
    void PurgeMemory();

    MemoryType GetCurrentType() const { return currentType_; }
    IWeapon* GetCurrentWeapon() const { return currentWeapon_; }

    float GetRemainingRatio() const;
    std::string GetAmmoText() const;

private:
    Player* owner_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;
    Input* input_ = nullptr;

    MemoryType currentType_ = MemoryType::NONE;
    IWeapon* currentWeapon_ = nullptr;

    std::map<MemoryType, std::unique_ptr<IWeapon>> weapons_;
};
