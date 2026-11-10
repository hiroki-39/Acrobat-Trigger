#include "Enemy.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Bullet/EnemyBullet.h"
#include "EnemyPresetManager.h"

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
    if (isDead_) return;

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

    // --- 行動パターンの実行 ---
    if (behavior_ == "PATH") {
        if (movePath_ && movePath_->IsValid()) {
            pathProgress_ += (moveSpeed_ * 0.5f * gameSpeed) / movePath_->GetTotalLength();
            if (pathProgress_ > 1.0f) {
                isDead_ = true; // パスの終端で消滅
            } else {
                position_ = movePath_->GetPosition(pathProgress_);
                Vector3 forward = movePath_->GetForward(pathProgress_);
                float yaw = std::atan2(forward.x, forward.z);
                float pitch = std::asin(std::clamp(-forward.y, -1.0f, 1.0f));
                object_->SetRotation({pitch, yaw, 0.0f});
            }
        }
    } else if (behavior_ == "PATROL_H") {
        // ★左右往復（水平パトロール）: 初期位置を中心に左右へ行き来し、プレイヤーを正面に狙う
        float amp = (moveAmplitude_ > 0.0f) ? moveAmplitude_ : 15.0f;
        position_.x = spawnPos_.x + std::sin(activeTimer_ * moveSpeed_ * 0.04f) * amp;
        position_.y = spawnPos_.y;
        position_.z = spawnPos_.z;

        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        object_->SetRotation({ pitch, yaw, 0.0f });

        // プレイヤー／カメラが大きく通り過ぎたら消滅
        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    } else if (behavior_ == "PATROL_V") {
        // ★上下往復（垂直パトロール）: 初期位置を中心に上下へ行き来し、プレイヤーを正面に狙う
        float amp = (moveAmplitude_ > 0.0f) ? moveAmplitude_ : 8.0f;
        position_.x = spawnPos_.x;
        position_.y = spawnPos_.y + std::sin(activeTimer_ * moveSpeed_ * 0.04f) * amp;
        position_.z = spawnPos_.z;

        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        object_->SetRotation({ pitch, yaw, 0.0f });

        // プレイヤー／カメラが大きく通り過ぎたら消滅
        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    } else if (behavior_ == "HOVER_SHOOT" || behavior_ == "HOVER") {
        // ★滞空浮遊（ホバリング）: 初期位置付近に留まってフワフワ浮遊し、プレイヤーをじっくり狙撃
        float amp = (moveAmplitude_ > 0.0f) ? moveAmplitude_ : 8.0f;
        position_.x = spawnPos_.x + std::sin(activeTimer_ * 0.03f) * (amp * 0.3f);
        position_.y = spawnPos_.y + std::sin(activeTimer_ * 0.06f) * (amp * 0.2f);
        position_.z = spawnPos_.z;

        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        object_->SetRotation({ pitch, yaw, 0.0f });

        // プレイヤー／カメラが通り過ぎたら消滅
        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    } else if (behavior_ == "SIN_WAVE") {
        // ★蛇行前進: サイン波で左右に揺れながら穏やかな速度で接近
        float amp = (moveAmplitude_ > 0.0f) ? moveAmplitude_ : 12.0f;
        position_.z -= moveSpeed_ * 0.5f * gameSpeed;
        position_.x = spawnPos_.x + std::sin(activeTimer_ * 0.04f) * amp;

        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        object_->SetRotation({ pitch, yaw, 0.0f });

        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    } else if (behavior_ == "STRAIGHT") {
        // ★直進突撃: 適正速度で前進し、正面から撃ち落としやすく
        position_.z -= moveSpeed_ * 0.7f * gameSpeed;
        
        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        object_->SetRotation({ pitch, yaw, 0.0f });

        // カメラの後ろを通り過ぎたら消滅
        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    } else if (behavior_ == "TURRET" || behavior_ == "STAY") {
        // 固定砲台（動かずプレイヤーを向く）
        Vector3 rot = object_->GetRotation();
        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
        float pitch = std::asin(std::clamp(-toPlayer.y / (std::max)(1.0f, std::sqrt(distXZ * distXZ + toPlayer.y * toPlayer.y)), -1.0f, 1.0f));
        rot.y += (yaw - rot.y) * 0.1f;
        rot.x += (pitch - rot.x) * 0.1f;
        object_->SetRotation(rot);

        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    } else {
        // フォールバック: プレイヤー方向へ移動しながらプレイヤーを向く
        position_.z -= moveSpeed_ * 0.5f * gameSpeed;
        Vector3 toPlayer = { playerWorldPos.x - position_.x, playerWorldPos.y - position_.y, playerWorldPos.z - position_.z };
        float yaw = std::atan2(toPlayer.x, toPlayer.z);
        object_->SetRotation({ 0.0f, yaw, 0.0f });
        if (position_.z < cameraPos.z - 30.0f) {
            isDead_ = true;
        }
    }

    // (isAutoAI_ のブロックは削除済)

    // 高さの制限（クランプ：明示的に制限が設定されている移動敵のみ適用）
    if (hasHeightLimit_ && behavior_ != "TURRET" && behavior_ != "STAY") {
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
    if (!isDead_ && isActive_ && object_) {
        object_->Draw();
    }
}

void Enemy::DrawCollider() {
    if (!isDead_ && colliderObject_) {
        colliderObject_->Draw();
    }
}

void Enemy::OnCollision(float damage) {
    hp_ -= damage;
    invincibilityTimer_ = 6.0f; // 被弾フラッシュ演出用タイマー（約0.1秒）
    
    if (hp_ <= 0.0f) {
        isDead_ = true;
    }
}

void Enemy::Kill() {
    hp_ = 0;
    isDead_ = true;
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
