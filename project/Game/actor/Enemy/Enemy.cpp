#include "Enemy.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "EnemyPresetManager.h"
#include <algorithm>
#include <cmath>

void IEnemyBehavior::LookAtPlayer(Object3d* object, const Vector3& pos, const Vector3& target) {
    if (!object) return;
    Vector3 toPlayer = { target.x - pos.x, target.y - pos.y, target.z - pos.z };
    float yaw = std::atan2(toPlayer.x, toPlayer.z);
    float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
    float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
    object->SetRotation({ pitch, yaw, 0.0f });
}

namespace {

class PathBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        if (ctx.movePath && ctx.movePath->IsValid()) {
            ctx.pathProgress += (ctx.moveSpeed * 0.5f * ctx.gameSpeed) / ctx.movePath->GetTotalLength();
            if (ctx.pathProgress > 1.0f) {
                ctx.isDead = true;
            } else {
                ctx.position = ctx.movePath->GetPosition(ctx.pathProgress);
                Vector3 forward = ctx.movePath->GetForward(ctx.pathProgress);
                float yaw = std::atan2(forward.x, forward.z);
                float pitch = std::asin(std::clamp(-forward.y, -1.0f, 1.0f));
                if (ctx.object) ctx.object->SetRotation({ pitch, yaw, 0.0f });
            }
        }
    }
};

class PatrolHorizontalBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        float amp = (ctx.moveAmplitude > 0.0f) ? ctx.moveAmplitude : 15.0f;
        ctx.position.x = ctx.spawnPos.x + std::sin(ctx.activeTimer * ctx.moveSpeed * 0.04f) * amp;
        ctx.position.y = ctx.spawnPos.y;
        ctx.position.z = ctx.spawnPos.z;
        LookAtPlayer(ctx.object, ctx.position, ctx.playerWorldPos);
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
};

class PatrolVerticalBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        float amp = (ctx.moveAmplitude > 0.0f) ? ctx.moveAmplitude : 8.0f;
        ctx.position.x = ctx.spawnPos.x;
        ctx.position.y = ctx.spawnPos.y + std::sin(ctx.activeTimer * ctx.moveSpeed * 0.04f) * amp;
        ctx.position.z = ctx.spawnPos.z;
        LookAtPlayer(ctx.object, ctx.position, ctx.playerWorldPos);
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
};

class HoverShootBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        float amp = (ctx.moveAmplitude > 0.0f) ? ctx.moveAmplitude : 8.0f;
        ctx.position.x = ctx.spawnPos.x + std::sin(ctx.activeTimer * 0.03f) * (amp * 0.3f);
        ctx.position.y = ctx.spawnPos.y + std::sin(ctx.activeTimer * 0.06f) * (amp * 0.2f);
        ctx.position.z = ctx.spawnPos.z;
        LookAtPlayer(ctx.object, ctx.position, ctx.playerWorldPos);
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
};

class SinWaveBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        float amp = (ctx.moveAmplitude > 0.0f) ? ctx.moveAmplitude : 12.0f;
        ctx.position.z -= ctx.moveSpeed * 0.5f * ctx.gameSpeed;
        ctx.position.x = ctx.spawnPos.x + std::sin(ctx.activeTimer * 0.04f) * amp;
        LookAtPlayer(ctx.object, ctx.position, ctx.playerWorldPos);
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
};

class StraightBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        ctx.position.z -= ctx.moveSpeed * 0.7f * ctx.gameSpeed;
        LookAtPlayer(ctx.object, ctx.position, ctx.playerWorldPos);
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
};

class TurretBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        if (!ctx.object) return;
        Vector3 rot = ctx.object->GetRotation();
        Vector3 toPlayer = { ctx.playerWorldPos.x - ctx.position.x, ctx.playerWorldPos.y - ctx.position.y, ctx.playerWorldPos.z - ctx.position.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        rot.y += (yaw - rot.y) * 0.1f;
        rot.x += (pitch - rot.x) * 0.1f;
        ctx.object->SetRotation(rot);
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
    bool ShouldClampHeight() const override { return false; }
};

class FallbackBehavior final : public IEnemyBehavior {
public:
    void Update(Context& ctx) override {
        ctx.position.z -= ctx.moveSpeed * 0.5f * ctx.gameSpeed;
        if (ctx.object) {
            Vector3 toPlayer = { ctx.playerWorldPos.x - ctx.position.x, ctx.playerWorldPos.y - ctx.position.y, ctx.playerWorldPos.z - ctx.position.z };
            float yaw = std::atan2(toPlayer.x, toPlayer.z);
            ctx.object->SetRotation({ 0.0f, yaw, 0.0f });
        }
        if (ctx.position.z < ctx.cameraPos.z - 30.0f) ctx.isDead = true;
    }
};

std::unique_ptr<IEnemyBehavior> CreateEnemyBehavior(const std::string& behavior) {
    if (behavior == "PATH") return std::make_unique<PathBehavior>();
    if (behavior == "PATROL_H") return std::make_unique<PatrolHorizontalBehavior>();
    if (behavior == "PATROL_V") return std::make_unique<PatrolVerticalBehavior>();
    if (behavior == "HOVER_SHOOT" || behavior == "HOVER") return std::make_unique<HoverShootBehavior>();
    if (behavior == "SIN_WAVE") return std::make_unique<SinWaveBehavior>();
    if (behavior == "STRAIGHT") return std::make_unique<StraightBehavior>();
    if (behavior == "TURRET" || behavior == "STAY") return std::make_unique<TurretBehavior>();
    return std::make_unique<FallbackBehavior>();
}

} // namespace

Enemy::~Enemy() = default;

void Enemy::Initialize(Object3dCommon* object3dCommon, const LevelObjectData& nodeData, uint32_t skyboxTexIndex) {
    object3dCommon_ = object3dCommon;
    position_ = nodeData.translation;
    spawnPos_ = position_;
    typeName_ = nodeData.enemyType;
    if (typeName_.empty()) typeName_ = "RUSHER";

    targetPos_ = nodeData.enemyTargetPos;
    hasHeightLimit_ = nodeData.hasHeightLimit;
    maxY_ = nodeData.enemyMaxY;
    minY_ = nodeData.enemyMinY;
    formationId_ = nodeData.enemyFormationId;
    
    // 拡張AI用プロパティの初期値
    behavior_ = nodeData.enemyBehavior;
    moveSpeed_ = nodeData.enemySpeed;
    shootInterval_ = static_cast<float>(nodeData.enemyShootInterval);
    spawnDist_ = static_cast<float>(nodeData.enemySpawnDist);
    isHomingBullet_ = (typeName_ == "HOMING");
    bulletSpeed_ = 2.0f;
    
    isDead_ = false;
    isDissolving_ = false;
    deathRewardGiven_ = false;
    dissolveTimer_ = 0.0f;
    collider_ = nodeData.collider;

    // ゲーム側のエネミープリセットマネージャーから設定を取得（ゲーム側オーサリング優先）
    std::string modelName = "cube.obj";
    Vector3 modelScale = nodeData.scale;
    Vector4 modelColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    const EnemyPresetData* preset = EnemyPresetManager::GetInstance()->GetPreset(typeName_);
    if (preset) {
        modelName = preset->modelName;
        modelScale = preset->scale;
        modelPosOffset_ = preset->modelPosOffset;
        modelColor = preset->color;
        hp_ = static_cast<float>(preset->hp);
        moveSpeed_ = preset->moveSpeed;
        moveAmplitude_ = preset->moveAmplitude;
        shootInterval_ = preset->shootInterval;
        behavior_ = preset->behavior;
        isHomingBullet_ = preset->isHomingBullet;
        bulletSpeed_ = preset->bulletSpeed;

        // ★ゲーム側エディタで設定したコライダー（当たり判定）を適用
        collider_.type = preset->colliderType;
        collider_.center = preset->colliderCenter;
        collider_.radius = preset->colliderRadius;
        collider_.size = preset->colliderSize;
    } else {
        // フォールバック（従来の初期値設定）
        hp_ = 2;
        if (typeName_ == "RUSHER") {
            modelName = "suzanne.obj";
        } else if (typeName_ == "SHOOTER" || typeName_ == "HOMING") {
            modelName = "suzanne.obj";
        } else if (typeName_ == "TURRET") {
            modelName = "cube.obj";
        } else {
            if (nodeData.fileName == "Asteroid") {
                modelName = "monsterBall.obj";
            } else {
                modelName = "cube.obj";
            }
        }
    }

    // モデルがロードされているか確認してロード
    ModelManager::GetInstance()->LoadModel(modelName);

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(modelName);
    object_->SetTranslate(GetVisualPosition());
    object_->SetScale(modelScale);
    object_->SetEnvironmentTextureIndex(skyboxTexIndex);
    color_ = modelColor;
    object_->SetColor(color_);

    // 敵エディター（EnemyStudio）のプレビュー向きと統一し、画面手前のプレイヤーに向くようにY軸180度（PI）回転を基準とする
    rotation_ = { nodeData.rotation.x, nodeData.rotation.y + 3.14159265f, nodeData.rotation.z };
    object_->SetRotation(rotation_);

    // デバッグ用コライダーオブジェクトの初期化
    colliderObject_ = std::make_unique<Object3d>();
    colliderObject_->Initialize(object3dCommon);
    if (collider_.type == "SPHERE") {
        ModelManager::GetInstance()->LoadModel("collider_sphere_enemy.obj");
        colliderObject_->SetModel("collider_sphere_enemy.obj"); // 球の代用
        colliderObject_->SetScale({collider_.radius, collider_.radius, collider_.radius});
    } else {
        ModelManager::GetInstance()->LoadModel("collider_cube_enemy.obj");
        colliderObject_->SetModel("collider_cube_enemy.obj");
        colliderObject_->SetScale({collider_.size.x, collider_.size.y, collider_.size.z});
    }
    
    // コライダー専用のモデルなので、色を赤にしても他のモデルに影響しない
    colliderObject_->GetModel()->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
    colliderObject_->SetEnvironmentCoefficient(0.0f);

    // 初期位置にコライダーオブジェクトを追従させる
    object_->Update();
    if (colliderObject_) {
        colliderObject_->SetTranslate(GetColliderCenter());
        colliderObject_->SetRotation(object_->GetRotation());
        colliderObject_->Update();
    }
}

void Enemy::SetMovePath(std::unique_ptr<Rail> path) {
    movePath_ = std::move(path);
    pathProgress_ = 0.0f;
    if (movePath_ && movePath_->IsValid()) {
        position_ = movePath_->GetPosition(0.0f);
        spawnPos_ = position_;
        if (object_) {
            object_->SetTranslate(position_);
        }
    }
}

void Enemy::Update(const Vector3& cameraPos, const Vector3& cameraForward, Player* player, std::list<std::unique_ptr<EnemyBullet>>& enemyBullets, float gameSpeed) {
    if (isDead_) {
        if (isDissolving_ && object_) {
            dissolveTimer_ += (1.0f / 60.0f) * gameSpeed;
            float progress = std::clamp(dissolveTimer_ / dissolveDuration_, 0.0f, 1.0f);
            Vector4 col = color_;
            col.w = 1.0f - progress;
            object_->SetSelectLightings(14);
            object_->SetShininess(GetVisualPosition().y);
            object_->SetColor(col);
            object_->Update();
            if (progress >= 1.0f) {
                isDissolving_ = false;
            }
        }
        return;
    }

    Vector3 playerWorldPos = cameraPos;
    if (player && player->GetObject3d()) {
        const Matrix4x4& mat = player->GetObject3d()->GetmatWorld();
        playerWorldPos = { mat.m[3][0], mat.m[3][1], mat.m[3][2] };
    }

    if (invincibilityTimer_ > 0.0f) {
        invincibilityTimer_ -= gameSpeed;
        // 被弾フラッシュ（白く発光してヒット感を強調）
        object_->SetColor({ 2.0f, 2.0f, 2.0f, 1.0f });
    } else {
        object_->SetColor(color_);
    }

    if (!isActive_) {
        // アクティブ化判定: プレイヤー（またはカメラ）との距離が一定以内になったら動き出す
        float distance = std::sqrt((position_.x - playerWorldPos.x)*(position_.x - playerWorldPos.x) + (position_.z - playerWorldPos.z)*(position_.z - playerWorldPos.z));
        float distThreshold = (spawnDist_ > 0.0f) ? spawnDist_ : 800.0f; 
        if (distance < distThreshold) {
            isActive_ = true;
        } else {
            return; // まだ出番ではない
        }
    }

    // スポナーによる遅延待機
    if (spawnDelay_ > 0.0f) {
        spawnDelay_ -= gameSpeed;
        // 遅延中は画面に映らないように奥に配置するか、更新をスキップする
        return; 
    }

    activeTimer_ += gameSpeed;

    // --- 行動パターンの実行（ポリモーフィズム） ---
    if (!behaviorStrategy_) {
        behaviorStrategy_ = CreateEnemyBehavior(behavior_);
    }
    IEnemyBehavior::Context behaviorCtx{
        position_, spawnPos_, object_.get(), movePath_.get(), pathProgress_,
        moveSpeed_, moveAmplitude_, activeTimer_, isDead_,
        playerWorldPos, cameraPos, gameSpeed
    };
    behaviorStrategy_->Update(behaviorCtx);

    // 高さの制限（クランプ：明示的に制限が設定されている移動敵のみ適用）
    if (hasHeightLimit_ && behaviorStrategy_->ShouldClampHeight()) {
        if (position_.y > maxY_) position_.y = maxY_;
        if (position_.y < minY_) position_.y = minY_;
    }

    // 弾の発射処理
    if (shootInterval_ > 0.0f && isActive_ && !isDead_) {
        attackTimer_ += gameSpeed;
        if (attackTimer_ >= shootInterval_) {
            attackTimer_ = 0.0f;
            auto bullet = std::make_unique<EnemyBullet>();
            bool isHoming = isHomingBullet_;
            Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
            float len = std::sqrt(toPlayer.x*toPlayer.x + toPlayer.y*toPlayer.y + toPlayer.z*toPlayer.z);
            if (len > 0.0f) {
                toPlayer = { toPlayer.x/len, toPlayer.y/len, toPlayer.z/len };
            }
            Vector3 velocity = { toPlayer.x * bulletSpeed_, toPlayer.y * bulletSpeed_, toPlayer.z * bulletSpeed_ };
            bullet->Initialize(object3dCommon_, position_, velocity, isHoming, player);
            enemyBullets.push_back(std::move(bullet));
        }
    }

    object_->SetTranslate(GetVisualPosition());
    object_->Update();

    // コライダーオブジェクトも追従させる
    if (colliderObject_) {
        colliderObject_->SetTranslate(GetColliderCenter());
        colliderObject_->SetRotation(object_->GetRotation());
        colliderObject_->Update();
    }
}

void Enemy::Draw() {
    if ((!isDead_ || isDissolving_) && isActive_ && object_) {
        object_->Draw();
    }
}

void Enemy::DrawCollider() {
    if (!isDead_ && colliderObject_) {
        colliderObject_->Draw();
    }
}

void Enemy::OnCollision(float damage) {
    if (isDead_) return;
    hp_ -= damage;
    invincibilityTimer_ = 6.0f; // 被弾フラッシュ演出用タイマー（約0.1秒）
    
    if (hp_ <= 0.0f) {
        isDead_ = true;
        isDissolving_ = true;
        dissolveTimer_ = 0.0f;
    }
}

void Enemy::Kill() {
    if (isDead_) return;
    hp_ = 0;
    isDead_ = true;
    isDissolving_ = true;
    dissolveTimer_ = 0.0f;
}

bool Enemy::CheckCollision(const Sphere& bulletSphere) const {
    if (isDead_) return false;

    Vector3 colCenter = GetColliderCenter();

    if (collider_.type == "SPHERE") {
        Sphere enemySphere = { colCenter, collider_.radius };
        return CollisionMath::IsCollision(bulletSphere, enemySphere);
    } else if (collider_.type == "OBB") {
        Matrix4x4 identity = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
        OBB enemyOBB = CollisionMath::CreateOBB(colCenter, collider_.size, identity);
        return CollisionMath::IsCollision(bulletSphere, enemyOBB);
    } else {
        // AABB
        AABB enemyAABB = {
            { colCenter.x - collider_.size.x * 0.5f, colCenter.y - collider_.size.y * 0.5f, colCenter.z - collider_.size.z * 0.5f },
            { colCenter.x + collider_.size.x * 0.5f, colCenter.y + collider_.size.y * 0.5f, colCenter.z + collider_.size.z * 0.5f }
        };
        return CollisionMath::IsCollision(bulletSphere, enemyAABB);
    }
}

bool Enemy::CheckRaycast(const Ray& ray, float* outDist) const {
    if (isDead_) return false;

    Vector3 colCenter = GetColliderCenter();

    if (collider_.type == "SPHERE") {
        Sphere enemySphere = { colCenter, collider_.radius };
        return CollisionMath::Raycast(ray, enemySphere, outDist);
    } else if (collider_.type == "OBB") {
        Matrix4x4 identity = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };
        OBB enemyOBB = CollisionMath::CreateOBB(colCenter, collider_.size, identity);
        return CollisionMath::Raycast(ray, enemyOBB, outDist);
    } else {
        // AABB
        AABB enemyAABB = {
            { colCenter.x - collider_.size.x * 0.5f, colCenter.y - collider_.size.y * 0.5f, colCenter.z - collider_.size.z * 0.5f },
            { colCenter.x + collider_.size.x * 0.5f, colCenter.y + collider_.size.y * 0.5f, colCenter.z + collider_.size.z * 0.5f }
        };
        return CollisionMath::Raycast(ray, enemyAABB, outDist);
    }
}

void Enemy::SetTexturePath(const std::string& path) {
    texturePath_ = path;
    if (!texturePath_.empty() && object_ && object_->GetModel()) {
        TextureManager::GetInstance()->LoadTexture(texturePath_);
        uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_);
        if (texIndex != TextureManager::GetInstance()->GetDefaultTextureIndex()) {
            object_->GetModel()->SetTextureIndex(texIndex);
        }
    }
}
