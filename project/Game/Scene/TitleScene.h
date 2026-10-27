#pragma once
#include <vector>
#include <memory>
#include <random>
#include "KHEngine/Graphics/2d/Sprite.h"
#include "KHEngine/Core/Framework/BaseScene.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Skybox/Skybox.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Graphics/3d/GPUParticle/GPUParticleSystem.h"
#include "KHEngine/Math/Vector3.h"
#include "KHEngine/Math/Vector4.h"

class TitleScene : public BaseScene
{
public:
    void Initialize();
    void Update();
    void Draw();
    void DrawUI() override;
    void Finalize();

private:
    void UpdateDebugCamera(float dt);

    // 3D環境
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Camera> debugCamera_;
    bool isDebugCamera_ = false;
    float debugCameraMoveSpeed_ = 10.0f;
    std::unique_ptr<Skybox> skybox_;
    std::unique_ptr<Object3d> playerObj_;

    // パーティクルエフェクト（スラスターブースト・スピードライン）
    ParticleEffect thrusterEffect_;
    ParticleEffect windEffect_; // スピードライン（高速で後方に流れる光の筋）
    std::mt19937 randomEngine_;

    // スラスターノズル位置オフセット（モデル原点からのローカル座標。機体後端から手前・カメラ側へ離す）
    Vector3 nozzleOffset_ = { 0.0f, -0.22f, -3.00f };

    // 自機パラメータ（定位置: (0, 5.5, -3.0)、地面 Graund Y=1.0 の上空を余裕を持って巡航）
    Vector3 targetPos_ = { 0.0f, 5.5f, -3.0f };
    Vector3 playerPos_ = { 0.0f, 5.5f, -3.0f };
    Vector3 playerRot_ = { 0.0f, 0.0f, 0.0f }; // すべて 0
    Vector3 playerScale_ = { 0.5f, 0.5f, 0.5f };
    Vector4 playerColor_ = { 0.155f, 0.155f, 0.155f, 1.0f };
    float envCoefficient_ = 0.6f;

    // カメラパラメータ（自機後方上空から前方地形・自機を見下ろす構図）
    Vector3 cameraPos_ = { 0.0f, 6.3f, -9.5f };
    Vector3 cameraRot_ = { 0.04f, 0.0f, 0.0f };

    // 起動時手前飛び込み登場演出パラメータ（カメラ・地形は前進し続け、機体が手前画面外から追い抜いて定位置へ入る）
    bool isIntro_ = true;
    float introTimer_ = 0.0f;
    float introDuration_ = 1.4f;
    float introStartOffsetZ_ = -16.0f;
    float introStartOffsetY_ = -1.2f;
    float introBlendTimer_ = 0.0f;       // 巡航モーションへの移行タイマー
    float introBlendDuration_ = 1.5f;    // 巡航モーションへの移行時間（1.5秒かけて滑らかにフェードイン）

    // スタート演出パラメータ（決定ボタン押下時演出）
    enum class StartTransitionType {
        VirtualShatter = 0,     // 新演出: 機体が連射して画面に亀裂が入り、仮想空間が砕け散る演出
        SonicAfterburner = 1,   // 案1: 超音速アフターバーナー突入（画面奥へ一閃突破）
        CameraBreakFlyby = 2,   // 案2: カメラブレイク・ローパス（画面手前へ豪快オーバーテイク）
    };
    StartTransitionType startTransitionType_ = StartTransitionType::VirtualShatter;
    bool isVirtualSpace_ = true;        // スターフォックス風サイバー仮想空間ビジュアル有効フラグ
    bool isPreviewTransition_ = false;  // シーン遷移なしプレビュー中フラグ
    bool isDiving_ = false;
    float diveTimer_ = 0.0f;
    float diveDuration_ = 1.45f;
    Vector3 diveStartPos_ = { 0.0f, 5.5f, -3.0f };
    Vector3 diveStartRot_ = { 0.0f, 0.0f, 0.0f };
    Vector3 diveStartCamPos_ = { 0.0f, 6.3f, -9.5f };
    Vector3 diveStartCamRot_ = { 0.04f, 0.0f, 0.0f };
    float diveStartProgressZ_ = 0.0f;

// GPUボクセルパーティクルシステム (3Dキューブインスタンシング)
    std::unique_ptr<GPUParticleSystem> gpuParticleSystem_;
    float dissolveProgress_ = 0.0f;

    // 前回のボクセル演出シミュレーション（GPUインスタンシングで高速描画）
    struct VoxelCubeParticle {
        Vector3 position;
        Vector3 velocity;
        Vector3 rotation;
        Vector3 rotSpeed;
        Vector3 scale;
        float baseScale = 0.12f;
        Vector4 color;
        float delay = 0.0f;
        float life = 0.0f;
        float maxLife = 1.0f;
        float noiseSeed = 0.0f;
        bool isForwardDiffuse = false;
    };
    static constexpr size_t kMaxVoxelCubes = 640;
    std::vector<VoxelCubeParticle> voxelCubes_;

    Microsoft::WRL::ComPtr<ID3D12Resource> voxelBufferResource_;
    GPUParticleData* mappedVoxelBuffer_ = nullptr;
    uint32_t voxelSrvIndex_ = 0;

    void SpawnShipVoxelCubes(const Vector3& shipPos);
    void UpdateShipVoxelCubes(float dt, const Vector3& curPos);

    // 案2（左旋回クライムブレイク）リアルタイム調整用パラメータ
    Vector3 prop2Rot_ = { -0.60f, 0.0f, -1.15f }; // ピッチ(X), ヨー(Y), ロール(Z) [rad]
    float prop2MoveX_ = -25.0f;                    // 左方向移動距離 (m)
    float prop2MoveY_ = 28.0f;                     // 上昇高度 (m)
    float prop2MoveZ_ = 18.0f;                     // 前進距離 (m)
    bool prop2PausePreview_ = false;               // プレビュー一時停止（ポーズ）
    float prop2PreviewProgress_ = 0.65f;           // 一時停止中の演出進行度シークバー (0.0 ~ 1.0)

    void TriggerStartTransition(bool isPreview);
    float promptBlinkTimer_ = 0.0f;

    // デモプレイ演出用タイマー・巡航時間設定
    float demoTimer_ = 0.0f;
    float demoCruiseDuration_ = 10.0f; // 巡航飛行をじっくり眺める時間（秒）

    // 飛行・巡航演出パラメータ（左右上下のゆったりとした移動とバンク）
    float idleTimer_ = 0.0f;
    bool isFlightMotion_ = true;
    float flightDriftX_ = 2.00f;         // 左右移動の振幅 (画像値: 2.00)
    float flightDriftY_ = 1.00f;         // 上下移動の振幅 (画像値: 1.00)
    float flightSpeed_ = 1.75f;          // 飛行ゆらぎの速度 (画像値: 1.75)
    float flightBankAmount_ = 0.200f;    // 旋回に伴うロール傾き (画像値: 0.200)

    // 前進巡航飛行パラメータ（自機とカメラの前方移動、時速300km ≒ 83.33 m/s）
    float forwardSpeed_ = 300.0f / 3.6f; // 前進巡航速度 (m/s) (画像値: 300.0 km/h)
    float flightProgressZ_ = 0.0f;       // 前進飛行の累積距離 (m)

    // -------------------------------------------------------------
    // アクロバット建物スタイリッシュ回避演出（Acrobatic Obstacle Dodge）
    // -------------------------------------------------------------
    bool isAcrobaticDodge_ = true;       // スタイリッシュ回避の有効フラグ
    float dodgeIntensity_ = 1.0f;        // 回避動作のダイナミック倍率 (0.0 ~ 2.0)
    float dodgeCameraRollAmount_ = 0.02f;// カメラの追従バンク傾き係数（安定重視で控えめに設定）
    float dodgeThrustBoost_ = 0.0f;      // 回避中のアフターバーナー追加ブースト度合い (0.0 ~ 1.5)

    // -------------------------------------------------------------
    // タイトルシーン用マップ（title.json から読み込んだ地形・建物ループオブジェクト群）
    // -------------------------------------------------------------
    struct TitleMapObjectNode {
        std::unique_ptr<Object3d> object;
        std::string name;
        Vector3 baseTranslation;
        Vector3 rotation;
        Vector3 scale;
        float loopPeriod = 4000.0f; // ループ周期 (Graund/Terrainは2000m、建造物は4000m)
        int slotIndex = 0;          // スロット番号 (0, 1, 2)
    };
    std::vector<TitleMapObjectNode> titleMapNodes_;
};
