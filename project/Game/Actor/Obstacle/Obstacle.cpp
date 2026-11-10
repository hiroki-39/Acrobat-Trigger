#include "Obstacle.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"

void Obstacle::Initialize(Object3dCommon* object3dCommon, const Vector3& pos, const Vector3& scale, const Vector3& rot, const std::string& fileName, uint32_t skyboxTexIndex, const LevelCollider& colliderInfo, bool isDestructible) {
    position_ = pos;
    collider_ = colliderInfo;
    // Blenderからの設定値にモデルのスケールを乗算して実際のワールドサイズにする
    collider_.center.x *= scale.x;
    collider_.center.y *= scale.y;
    collider_.center.z *= scale.z;
    collider_.size.x *= scale.x;
    collider_.size.y *= scale.y;
    collider_.size.z *= scale.z;
    collider_.radius *= (scale.x > scale.y ? (scale.x > scale.z ? scale.x : scale.z) : (scale.y > scale.z ? scale.y : scale.z)); // 最大スケールを適用
    isDestructible_ = isDestructible; // パラメータからフラグをセット

    // Blender側でコライダー設定が省略された場合のデフォルト処理
    if (collider_.type.empty()) {
        collider_.type = "BOX";
        collider_.center = {0.0f, 0.0f, 0.0f};
        collider_.size = scale; // スケールをそのままBOXのサイズ(半辺長)として扱う
    }

    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetTranslate(position_);
    
    rotation_ = rot;
    baseScale_ = scale;
    object_->SetScale(scale);
    object_->SetRotation(rotation_);

    std::string modelName = fileName;
    if (modelName.find("ColliderOnly") != std::string::npos || modelName.find("Invisible") != std::string::npos) {
        // 見えない壁（当たり判定専用）の場合はモデルを読み込まない
        isVisible_ = false;
    } else {
        if (modelName.empty()) {
            modelName = "cube.obj"; // デフォルト
        } else if (modelName.find(".obj") == std::string::npos) {
            modelName += ".obj";
        }
        
        ModelManager::GetInstance()->LoadModel(modelName);
        if (ModelManager::GetInstance()->FindModel(modelName)) {
            object_->SetModel(modelName);
            isVisible_ = true;
        }
    }
    
    if (isVisible_) {
        object_->SetEnvironmentTextureIndex(skyboxTexIndex);
    }

    // デバッグ用コライダー表示の初期化
    if (collider_.type == "SPHERE" || (collider_.type == "BOX" || collider_.type == "OBB" || collider_.type == "AABB")) {
        colliderObject_ = std::make_unique<Object3d>();
        colliderObject_->Initialize(object3dCommon);
        
        if (collider_.type == "SPHERE") {
            colliderObject_->SetModel("collider_sphere.obj");
            colliderObject_->SetScale({ collider_.radius, collider_.radius, collider_.radius });
        } else if ((collider_.type == "BOX" || collider_.type == "OBB" || collider_.type == "AABB")) {
        Vector3 colSize = collider_.size;
            colliderObject_->SetModel("cube.obj"); // 境界線のみの箱モデルを想定
            colliderObject_->SetScale({ collider_.size.x, collider_.size.y, collider_.size.z });
        }
        
        if (colliderObject_->GetModel()) {
            colliderObject_->GetModel()->SetColor({ 0.0f, 1.0f, 1.0f, 1.0f }); // 水色
        }
    }
    
    Update();
}

void Obstacle::Update() {
    if (isShrinking_) {
        shrinkScale_ -= 0.1f;
        if (shrinkScale_ <= 0.0f) {
            shrinkScale_ = 0.0f;
            isDead_ = true;
        }
        Vector3 newScale = { baseScale_.x * shrinkScale_, baseScale_.y * shrinkScale_, baseScale_.z * shrinkScale_ };
        object_->SetScale(newScale);
    }
    // 将来的なアニメーションや移動処理をここに追加する
    // 例: position_.y -= 0.1f; // 落下など
    
    if (object_) {
        object_->SetTranslate(position_);
        object_->SetRotation(rotation_);
        object_->Update();
    }
    
    if (colliderObject_) {
        Vector3 colliderPos = {
            position_.x + collider_.center.x,
            position_.y + collider_.center.y,
            position_.z + collider_.center.z
        };
        colliderObject_->SetTranslate(colliderPos);
        // OBBとして機能させる場合、親と同じ回転を適用する
        colliderObject_->SetRotation(rotation_);
        colliderObject_->Update();
    }
}

void Obstacle::Draw() {
    if (!isVisible_) return;
    
    if (object_ && !isDead_) {
        object_->Draw();
    }
}

void Obstacle::DrawCollider() {
    if (colliderObject_ && !isDead_) {
        colliderObject_->Draw();
    } else if (object_ && !isDead_) {
        // メッシュコリジョンを持つ障害物はモデル自身をワイヤーフレーム描画
        object_->Draw();
    }
}

void Obstacle::OnCollision(float /*damage*/) {
    if (isDestructible_) {
        isDead_ = true;
    }
}

void Obstacle::Kill() {
    if (isDestructible_) {
        isDead_ = true;
    }
}

void Obstacle::SetTexturePath(const std::string& path) {
    texturePath_ = path;
    if (!texturePath_.empty() && object_ && object_->GetModel()) {
        TextureManager::GetInstance()->LoadTexture(texturePath_);
        uint32_t texIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath_);
        if (texIndex != TextureManager::GetInstance()->GetDefaultTextureIndex()) {
            object_->GetModel()->SetTextureIndex(texIndex);
        }
    }
}

void Obstacle::EnsureTriangles() const {
    if (isTrianglesInitialized_) return;
    isTrianglesInitialized_ = true;

    if (object_ && object_->GetModel()) {
        triangles_ = object_->GetModel()->GetWorldTriangles(object_->GetmatWorld());
        if (!triangles_.empty()) {
            hasMeshCollider_ = true;
            broadAABB_.min = { 1e9f, 1e9f, 1e9f };
            broadAABB_.max = { -1e9f, -1e9f, -1e9f };
            for (const auto& tri : triangles_) {
                for (const auto& p : { tri.p0, tri.p1, tri.p2 }) {
                    broadAABB_.min.x = (std::min)(broadAABB_.min.x, p.x);
                    broadAABB_.min.y = (std::min)(broadAABB_.min.y, p.y);
                    broadAABB_.min.z = (std::min)(broadAABB_.min.z, p.z);
                    broadAABB_.max.x = (std::max)(broadAABB_.max.x, p.x);
                    broadAABB_.max.y = (std::max)(broadAABB_.max.y, p.y);
                    broadAABB_.max.z = (std::max)(broadAABB_.max.z, p.z);
                }
            }
        }
    }
}

bool Obstacle::CheckCollisionWithSphere(const Sphere& sphere, CollisionResult* outResult) const {
    if (isDead_) return false;
    EnsureTriangles();

    if (hasMeshCollider_) {
        // AABBによるカリング
        if (!CollisionMath::IsCollision(sphere, broadAABB_)) {
            return false;
        }

        bool hitAny = false;
        CollisionResult bestResult;
        bestResult.penetrationDepth = -1.0f;

        for (const auto& tri : triangles_) {
            CollisionResult res;
            if (CollisionMath::IsCollision(sphere, tri, &res)) {
                hitAny = true;
                if (res.penetrationDepth > bestResult.penetrationDepth) {
                    bestResult = res;
                }
            }
        }

        if (hitAny) {
            if (outResult) *outResult = bestResult;
            return true;
        }
        return false;
    }

    Vector3 centerPos = {
        position_.x + collider_.center.x,
        position_.y + collider_.center.y,
        position_.z + collider_.center.z
    };

    if (collider_.type == "SPHERE") {
        Sphere mySphere = { centerPos, collider_.radius };
        if (CollisionMath::IsCollision(sphere, mySphere)) {
            if (outResult) {
                outResult->isHit = true;
                Vector3 diff = { sphere.center.x - centerPos.x, sphere.center.y - centerPos.y, sphere.center.z - centerPos.z };
                float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (dist > 0.0001f) {
                    outResult->normal = { diff.x / dist, diff.y / dist, diff.z / dist };
                } else {
                    outResult->normal = { 0.0f, 1.0f, 0.0f };
                }
                outResult->hitPoint = { centerPos.x + outResult->normal.x * collider_.radius,
                                        centerPos.y + outResult->normal.y * collider_.radius,
                                        centerPos.z + outResult->normal.z * collider_.radius };
                outResult->penetrationDepth = (sphere.radius + collider_.radius) - dist;
            }
            return true;
        }
    } else if ((collider_.type == "BOX" || collider_.type == "OBB" || collider_.type == "AABB")) {
        Vector3 colSize = collider_.size;
        Matrix4x4 rotMat = Matrix4x4::RotateX(rotation_.x) * Matrix4x4::RotateY(rotation_.y) * Matrix4x4::RotateZ(rotation_.z);
        OBB obb = CollisionMath::CreateOBB(centerPos, colSize, rotMat);
        if (CollisionMath::IsCollision(sphere, obb)) {
            if (outResult) {
                outResult->isHit = true;
                Vector3 diff = { sphere.center.x - centerPos.x, sphere.center.y - centerPos.y, sphere.center.z - centerPos.z };
                float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (dist > 0.0001f) {
                    outResult->normal = { diff.x / dist, diff.y / dist, diff.z / dist };
                } else {
                    outResult->normal = { 0.0f, 1.0f, 0.0f };
                }
                outResult->hitPoint = centerPos;
                outResult->penetrationDepth = sphere.radius;
            }
            return true;
        }
    }

    return false;
}

bool Obstacle::CheckCollisionWithOBB(const OBB& obb, CollisionResult* outResult, std::vector<Triangle>* outTestedTriangles, std::vector<Triangle>* outHitTriangles) const {
    if (isDead_) return false;
    EnsureTriangles();

    if (hasMeshCollider_) {
        if (!CollisionMath::IsCollision(obb, broadAABB_)) {
            return false;
        }

        bool hitAny = false;
        CollisionResult bestResult;
        bestResult.penetrationDepth = -1.0f;

        for (const auto& tri : triangles_) {
            if (outTestedTriangles) {
                outTestedTriangles->push_back(tri);
            }
            CollisionResult res;
            if (CollisionMath::IsCollision(obb, tri, &res)) {
                hitAny = true;
                if (outHitTriangles) {
                    outHitTriangles->push_back(tri);
                }
                if (res.penetrationDepth > bestResult.penetrationDepth) {
                    bestResult = res;
                }
            }
        }

        if (hitAny) {
            if (outResult) *outResult = bestResult;
            return true;
        }
        return false;
    }

    Vector3 centerPos = {
        position_.x + collider_.center.x,
        position_.y + collider_.center.y,
        position_.z + collider_.center.z
    };

    if (collider_.type == "SPHERE") {
        Sphere mySphere = { centerPos, collider_.radius };
        if (CollisionMath::IsCollision(mySphere, obb)) {
            if (outResult) {
                outResult->isHit = true;
                Vector3 diff = { obb.center.x - centerPos.x, obb.center.y - centerPos.y, obb.center.z - centerPos.z };
                float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (dist > 0.0001f) {
                    outResult->normal = { diff.x / dist, diff.y / dist, diff.z / dist };
                } else {
                    outResult->normal = { 0.0f, 1.0f, 0.0f };
                }
                outResult->hitPoint = { centerPos.x + outResult->normal.x * collider_.radius,
                                        centerPos.y + outResult->normal.y * collider_.radius,
                                        centerPos.z + outResult->normal.z * collider_.radius };
                outResult->penetrationDepth = collider_.radius;
            }
            return true;
        }
    } else if ((collider_.type == "BOX" || collider_.type == "OBB" || collider_.type == "AABB")) {
        AABB aabb = {
            { centerPos.x - collider_.size.x * 0.5f, centerPos.y - collider_.size.y * 0.5f, centerPos.z - collider_.size.z * 0.5f },
            { centerPos.x + collider_.size.x * 0.5f, centerPos.y + collider_.size.y * 0.5f, centerPos.z + collider_.size.z * 0.5f }
        };
        if (CollisionMath::IsCollision(obb, aabb)) {
            if (outResult) {
                outResult->isHit = true;
                Vector3 diff = { obb.center.x - centerPos.x, obb.center.y - centerPos.y, obb.center.z - centerPos.z };
                float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (dist > 0.0001f) {
                    outResult->normal = { diff.x / dist, diff.y / dist, diff.z / dist };
                } else {
                    outResult->normal = { 0.0f, 1.0f, 0.0f };
                }
                outResult->hitPoint = centerPos;
                outResult->penetrationDepth = 0.5f;
            }
            return true;
        }
    }

    return false;
}

bool Obstacle::CheckCollision(const Sphere& bulletSphere) const {
    if (isDead_) return false;
    EnsureTriangles();

    if (hasMeshCollider_) {
        return CheckCollisionWithSphere(bulletSphere, nullptr);
    }

    Vector3 centerPos = {
        position_.x + collider_.center.x,
        position_.y + collider_.center.y,
        position_.z + collider_.center.z
    };

    if (collider_.type == "SPHERE") {
        Sphere mySphere = { centerPos, collider_.radius };
        return CollisionMath::IsCollision(bulletSphere, mySphere);
    } 
    else if ((collider_.type == "BOX" || collider_.type == "OBB" || collider_.type == "AABB")) {
        Vector3 colSize = collider_.size;
        // 回転がある場合はOBB、ない場合はAABBとして扱う
        if (rotation_.x == 0.0f && rotation_.y == 0.0f && rotation_.z == 0.0f) {
            AABB aabb = {
                { centerPos.x - colSize.x, centerPos.y - colSize.y, centerPos.z - colSize.z },
                { centerPos.x + colSize.x, centerPos.y + colSize.y, centerPos.z + colSize.z }
            };
            return CollisionMath::IsCollision(bulletSphere, aabb);
        } else {
            // OBBを生成
            Matrix4x4 rotMat = Matrix4x4::RotateX(rotation_.x) * Matrix4x4::RotateY(rotation_.y) * Matrix4x4::RotateZ(rotation_.z);
            OBB obb = CollisionMath::CreateOBB(centerPos, colSize, rotMat);
            return CollisionMath::IsCollision(bulletSphere, obb);
        }
    }

    return false;
}

bool Obstacle::CheckRaycast(const Ray& ray, float* outDist) const {
    if (isDead_) return false;

    Vector3 centerPos = {
        position_.x + collider_.center.x,
        position_.y + collider_.center.y,
        position_.z + collider_.center.z
    };

    if (collider_.type == "SPHERE") {
        Sphere mySphere = { centerPos, collider_.radius };
        return CollisionMath::Raycast(ray, mySphere, outDist);
    } 
    else if ((collider_.type == "BOX" || collider_.type == "OBB" || collider_.type == "AABB")) {
        Vector3 colSize = collider_.size;
        if (rotation_.x == 0.0f && rotation_.y == 0.0f && rotation_.z == 0.0f) {
            AABB aabb = {
                { centerPos.x - colSize.x, centerPos.y - colSize.y, centerPos.z - colSize.z },
                { centerPos.x + colSize.x, centerPos.y + colSize.y, centerPos.z + colSize.z }
            };
            return CollisionMath::Raycast(ray, aabb, outDist);
        } else {
            Matrix4x4 rotMat = Matrix4x4::RotateX(rotation_.x) * Matrix4x4::RotateY(rotation_.y) * Matrix4x4::RotateZ(rotation_.z);
            OBB obb = CollisionMath::CreateOBB(centerPos, colSize, rotMat);
            return CollisionMath::Raycast(ray, obb, outDist);
        }
    }

    return false;
}

void Obstacle::StartShrink() {
    isShrinking_ = true;
}