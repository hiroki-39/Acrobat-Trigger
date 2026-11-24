#include "ParticleEmitter.h"
#include <numbers>
#include <algorithm>

ParticleEmitter::ParticleEmitter()
{
    std::random_device rd;
    randomEngine_.seed(rd());
}

void ParticleEmitter::Update(float dt)
{
    // パーティクルの更新と寿命チェック
    for (auto it = particles_.begin(); it != particles_.end(); )
    {
        Particle& p = *it;
        p.currentTime += dt;

        if (p.currentTime >= p.lifeTime)
        {
            it = particles_.erase(it);
            continue;
        }

        // 物理挙動の適用 (Physics)
        if (parameter_.useGravity)
        {
            p.velocity.y += parameter_.gravity * dt;
        }
        if (parameter_.drag > 0.0f)
        {
            // 簡易的な空気抵抗 (v = v * (1 - drag * dt))
            float dragFactor = 1.0f - (parameter_.drag * dt);
            if (dragFactor < 0.0f) dragFactor = 0.0f;
            p.velocity.x *= dragFactor;
            p.velocity.y *= dragFactor;
            p.velocity.z *= dragFactor;
        }

        // 加速度フィールドの適用
        if (accelField_.IsInArea(p.transform.translate))
        {
            p.velocity += accelField_.acceleration * dt;
        }

        // 移動・回転
        p.transform.translate += p.velocity * dt;
        p.transform.rotation += p.angularVelocity * dt;

        // 寿命による変化 (Over Lifetime)
        float progress = p.currentTime / p.lifeTime; // 0.0 から 1.0
        
        if (parameter_.isScaleOverLifetime)
        {
            p.transform.scale.x = std::lerp(p.initialScale.x, parameter_.endScale.x, progress);
            p.transform.scale.y = std::lerp(p.initialScale.y, parameter_.endScale.y, progress);
            p.transform.scale.z = std::lerp(p.initialScale.z, parameter_.endScale.z, progress);
        }

        if (parameter_.isColorOverLifetime)
        {
            p.color.x = std::lerp(p.initialColor.x, parameter_.endColor.x, progress);
            p.color.y = std::lerp(p.initialColor.y, parameter_.endColor.y, progress);
            p.color.z = std::lerp(p.initialColor.z, parameter_.endColor.z, progress);
            p.color.w = std::lerp(p.initialColor.w, parameter_.endColor.w, progress);
        }
        
        ++it;
    }

    // 自動生成（Frequency が 0 より大きい場合）
    if (parameter_.frequency > 0.0f)
    {
        frequencyTimer_ += dt;
        while (frequencyTimer_ >= parameter_.frequency)
        {
            Emit(parameter_.count);
            frequencyTimer_ -= parameter_.frequency;
        }
    }
}

void ParticleEmitter::Emit(uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        particles_.push_back(MakeNewParticle());
    }
}

Particle ParticleEmitter::MakeNewParticle()
{
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    auto randomVec3 = [&](const Vector3& min, const Vector3& max) {
        return Vector3{
            std::lerp(min.x, max.x, dist01(randomEngine_)),
            std::lerp(min.y, max.y, dist01(randomEngine_)),
            std::lerp(min.z, max.z, dist01(randomEngine_))
        };
    };

    auto randomVec4 = [&](const Vector4& min, const Vector4& max) {
        return Vector4{
            std::lerp(min.x, max.x, dist01(randomEngine_)),
            std::lerp(min.y, max.y, dist01(randomEngine_)),
            std::lerp(min.z, max.z, dist01(randomEngine_)),
            std::lerp(min.w, max.w, dist01(randomEngine_))
        };
    };

    Particle p;
    p.currentTime = 0.0f;
    p.lifeTime = std::lerp(parameter_.minLifeTime, parameter_.maxLifeTime, dist01(randomEngine_));
    
    p.transform.translate = position_; // 基本はエミッタの位置
    p.transform.scale = randomVec3(parameter_.minScale, parameter_.maxScale);
    p.initialScale = p.transform.scale; // 初期スケールを保存
    
    p.transform.rotation = randomVec3(parameter_.minRotation, parameter_.maxRotation);
    p.angularVelocity = randomVec3(parameter_.minAngularVelocity, parameter_.maxAngularVelocity);
    
    p.velocity = randomVec3(parameter_.minVelocity, parameter_.maxVelocity);
    
    p.color = randomVec4(parameter_.minColor, parameter_.maxColor);
    p.initialColor = p.color; // 初期色を保存

    return p;
}

uint32_t ParticleEmitter::FillInstancingBuffer(ParticleForGPU* outBuffer, uint32_t maxInstances,
    const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Matrix4x4& billboardMatrix)
{
    uint32_t numInstance = 0;
    Matrix4x4 viewProjection = viewMatrix * projectionMatrix;

    for (const auto& p : particles_)
    {
        if (numInstance >= maxInstances) break;

        Matrix4x4 scaleMat = Matrix4x4::Scale(p.transform.scale);
        Matrix4x4 rotMat = Matrix4x4::RotateX(p.transform.rotation.x) * 
                           Matrix4x4::RotateY(p.transform.rotation.y) * 
                           Matrix4x4::RotateZ(p.transform.rotation.z);
        Matrix4x4 transMat = Matrix4x4::Translation(p.transform.translate);

        Matrix4x4 worldMat;
        if (parentMatrix_)
        {
            if (useBillboard_)
            {
                // 親のワールド行列でローカル位置をワールド位置へ変換 (行ベクトル形式: v * M)
                const Matrix4x4& m = *parentMatrix_;
                Vector3 worldPos = {
                    p.transform.translate.x * m.m[0][0] + p.transform.translate.y * m.m[1][0] + p.transform.translate.z * m.m[2][0] + m.m[3][0],
                    p.transform.translate.x * m.m[0][1] + p.transform.translate.y * m.m[1][1] + p.transform.translate.z * m.m[2][1] + m.m[3][1],
                    p.transform.translate.x * m.m[0][2] + p.transform.translate.y * m.m[1][2] + p.transform.translate.z * m.m[2][2] + m.m[3][2]
                };
                worldMat = scaleMat * rotMat * billboardMatrix * Matrix4x4::Translation(worldPos);
            }
            else
            {
                // 非ビルボード：親の回転・移動に完全連動
                worldMat = scaleMat * rotMat * transMat * (*parentMatrix_);
            }
        }
        else
        {
            if (useBillboard_)
            {
                // Z軸回転を活かすため、回転をビルボード行列の前に適用
                worldMat = scaleMat * rotMat * billboardMatrix * transMat;
            }
            else
            {
                worldMat = scaleMat * rotMat * transMat;
            }
        }

        outBuffer[numInstance].WVP = worldMat * viewProjection;
        outBuffer[numInstance].World = worldMat;
        outBuffer[numInstance].color = p.color;

        numInstance++;
    }

    return numInstance;
}

// --- プリセット ---

ParticleEmitterParameter ParticleEmitter::CreateFirePreset()
{
    ParticleEmitterParameter p;
    p.name = "Fire";
    p.count = 1;
    p.frequency = 0.02f;
    p.minLifeTime = 0.5f; p.maxLifeTime = 1.5f;
    p.minScale = { 0.2f, 0.2f, 0.2f }; p.maxScale = { 0.8f, 0.8f, 0.8f };
    p.minVelocity = { -0.2f, 1.0f, -0.2f }; p.maxVelocity = { 0.2f, 2.0f, 0.2f };
    p.minColor = { 1.0f, 0.4f, 0.0f, 1.0f }; p.maxColor = { 1.0f, 0.8f, 0.2f, 1.0f };
    return p;
}

ParticleEmitterParameter ParticleEmitter::CreateSnowPreset()
{
    ParticleEmitterParameter p;
    p.name = "Snow";
    p.count = 1;
    p.frequency = 0.1f;
    p.minLifeTime = 5.0f; p.maxLifeTime = 10.0f;
    p.minScale = { 0.1f, 0.1f, 0.1f }; p.maxScale = { 0.3f, 0.3f, 0.3f };
    p.minVelocity = { -0.5f, -1.0f, -0.5f }; p.maxVelocity = { 0.5f, -0.5f, 0.5f };
    p.minColor = { 0.9f, 0.9f, 1.0f, 1.0f }; p.maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    return p;
}

ParticleEmitterParameter ParticleEmitter::CreateExplosionPreset()
{
    ParticleEmitterParameter p;
    p.name = "Explosion";
    p.count = 30;
    p.frequency = 0.0f; // 一発
    p.minLifeTime = 0.5f; p.maxLifeTime = 1.0f;
    p.minScale = { 0.2f, 0.2f, 0.2f }; p.maxScale = { 1.0f, 1.0f, 1.0f };
    p.minVelocity = { -5.0f, -5.0f, -5.0f }; p.maxVelocity = { 5.0f, 5.0f, 5.0f };
    p.minColor = { 1.0f, 0.2f, 0.0f, 1.0f }; p.maxColor = { 1.0f, 0.9f, 0.1f, 1.0f };
    return p;
}

ParticleEmitterParameter ParticleEmitter::CreateSlashPreset()
{
    ParticleEmitterParameter p;
    p.name = "Slash";
    p.count = 8;        // 一度に8本出す
    p.frequency = 0.0f; // 一発
    p.minLifeTime = 0.2f; p.maxLifeTime = 0.4f; // 短く消える

    // 画像の通り、横を細く、縦を長くする
    // X を 0.05 程度に潰す
    p.minScale = { 0.05f, 0.5f, 1.0f }; 
    p.maxScale = { 0.08f, 2.0f, 1.0f };

    // 速度はほとんどなし、または少しだけ外に広がる程度
    p.minVelocity = { -0.1f, -0.1f, -0.1f }; 
    p.maxVelocity = { 0.1f, 0.1f, 0.1f };

    // Z軸にランダム回転（これが星型に見えるポイント）
    p.minRotation = { 0.0f, 0.0f, -std::numbers::pi_v<float> };
    p.maxRotation = { 0.0f, 0.0f,  std::numbers::pi_v<float> };

    p.minColor = { 0.8f, 0.9f, 1.0f, 1.0f }; // 少し青みがかった白
    p.maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    return p;
}

ParticleEmitterParameter ParticleEmitter::CreateRingPreset()
{
    ParticleEmitterParameter p;
    p.name = "Ring";
    p.count = 1;
    p.frequency = 0.5f;
    p.minLifeTime = 1.0f; p.maxLifeTime = 1.5f;
    p.minScale = { 1.0f, 1.0f, 1.0f }; p.maxScale = { 1.2f, 1.2f, 1.2f };
    p.minVelocity = { 0.0f, 0.5f, 0.0f }; p.maxVelocity = { 0.0f, 1.0f, 0.0f };
    p.minRotation = { 1.57f, 0.0f, 0.0f }; p.maxRotation = { 1.57f, 0.0f, 0.0f };
    p.minColor = { 1.0f, 1.0f, 1.0f, 1.0f }; p.maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    return p;
}

ParticleEmitterParameter ParticleEmitter::CreateCylinderPreset()
{
    ParticleEmitterParameter p;
    p.name = "Cylinder";
    p.count = 1;
    p.frequency = 0.5f;
    p.minLifeTime = 1.0f; p.maxLifeTime = 2.0f;
    p.minScale = { 1.0f, 1.0f, 1.0f }; p.maxScale = { 1.5f, 1.5f, 1.5f };
    p.minVelocity = { 0.0f, 0.1f, 0.0f }; p.maxVelocity = { 0.0f, 0.3f, 0.0f };
    p.minRotation = { 0.0f, 0.0f, 0.0f }; p.maxRotation = { 0.0f, 6.28f, 0.0f };
    p.minColor = { 1.0f, 1.0f, 1.0f, 1.0f }; p.maxColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    return p;
}