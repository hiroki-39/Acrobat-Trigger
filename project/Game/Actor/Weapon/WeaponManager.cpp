#include "WeaponManager.h"
#include "NormalWeapon.h"
#include "CyberBitWeapon.h"
#include "ChargeShotWeapon.h"
#include "BombWeapon.h"
#include "SpreadWeapon.h"
#include "FunnelWeapon.h"
#include "Game/Actor/Player/Player.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Input/Input.h"
#include "KHEngine/Sound/Core/SoundManager.h"

void WeaponManager::Initialize(Player* owner, Object3dCommon* object3dCommon) {
    owner_ = owner;
    object3dCommon_ = object3dCommon;
    input_ = EngineServices::GetInstance()->GetInput();

    // 武器テーブルの初期化（std::map を使用）
    weapons_[MemoryType::NONE]        = std::make_unique<NormalWeapon>();
    weapons_[MemoryType::CYBER_BIT]   = std::make_unique<CyberBitWeapon>();
    weapons_[MemoryType::CHARGE_SHOT] = std::make_unique<ChargeShotWeapon>();
    weapons_[MemoryType::BOMB]        = std::make_unique<BombWeapon>();
    weapons_[MemoryType::SPREAD]      = std::make_unique<SpreadWeapon>();
    weapons_[MemoryType::FUNNEL]      = std::make_unique<FunnelWeapon>();

    // 全武器の初期化
    for (auto& [type, weapon] : weapons_) {
        weapon->Initialize(owner_, object3dCommon_);
    }

    // 初期状態は通常弾
    currentType_ = MemoryType::NONE;
    currentWeapon_ = weapons_[currentType_].get();
}

void WeaponManager::Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* parentCamera) {
    // Rキーでメモリをパージ（廃棄）して通常弾に戻す
    if (input_ && input_->TriggerKey(DIK_R)) {
        if (currentType_ != MemoryType::NONE) {
            PurgeMemory();
        }
    }

    if (currentWeapon_) {
        currentWeapon_->Update(gameSpeed, enemies, boss, parentCamera);

        // 弾切れ／時間切れによる自動フォールバック
        if (currentType_ != MemoryType::NONE && currentWeapon_->IsDepleted()) {
            PurgeMemory();
        }
    }
}

void WeaponManager::Attack(Input* input,
                           std::list<std::unique_ptr<PlayerBullet>>& bullets,
                           std::list<std::unique_ptr<PlayerMissile>>& missiles,
                           const std::list<std::unique_ptr<Enemy>>& enemies,
                           ArmoredTrainBoss* boss,
                           Object3d* parentCamera,
                           float gameSpeed) {
    if (currentWeapon_) {
        currentWeapon_->Attack(input, bullets, missiles, enemies, boss, parentCamera, gameSpeed);
    }
}

void WeaponManager::Draw() {
    if (currentWeapon_) {
        currentWeapon_->Draw();
    }
}

void WeaponManager::DrawUI() {
    if (currentWeapon_) {
        currentWeapon_->DrawUI();
    }
}

bool WeaponManager::EquipWeapon(MemoryType type) {
    if (type == MemoryType::NONE) {
        PurgeMemory();
        return true;
    }

    auto it = weapons_.find(type);
    if (it != weapons_.end()) {
        currentType_ = type;
        currentWeapon_ = it->second.get();
        currentWeapon_->Reset();
        // 装備SE再生
        SoundManager::GetInstance()->PlaySE("shot.mp3", 0.6f);
        return true;
    }
    return false;
}

void WeaponManager::PurgeMemory() {
    if (currentType_ != MemoryType::NONE) {
        currentType_ = MemoryType::NONE;
        currentWeapon_ = weapons_[currentType_].get();
        if (currentWeapon_) {
            currentWeapon_->Reset();
        }
        // パージSE再生
        SoundManager::GetInstance()->PlaySE("shot.mp3", 0.4f);
    }
}

bool WeaponManager::HasActiveBombExplosion(Vector3* outPos, float* outRadius) const {
    if (currentType_ == MemoryType::BOMB && currentWeapon_) {
        auto bombWeapon = dynamic_cast<BombWeapon*>(currentWeapon_);
        if (bombWeapon) {
            return bombWeapon->HasActiveExplosion(outPos, outRadius);
        }
    }
    return false;
}

float WeaponManager::GetRemainingRatio() const {
    if (currentWeapon_) {
        return currentWeapon_->GetRemainingRatio();
    }
    return 1.0f;
}

std::string WeaponManager::GetAmmoText() const {
    if (currentWeapon_) {
        return currentWeapon_->GetAmmoText();
    }
    return "NORMAL";
}
