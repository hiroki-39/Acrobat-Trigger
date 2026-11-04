#pragma once
#include "MemoryType.h"
#include <list>
#include <memory>
#include <string>

class Player;
class Enemy;
class ArmoredTrainBoss;
class PlayerBullet;
class PlayerMissile;
class Input;
class Object3dCommon;
class Object3d;

/// <summary>
/// 武器の基底インターフェース
/// 各武器の攻撃ロジック、残弾/エネルギー管理、描画演出をカプセル化する
/// </summary>
class IWeapon {
public:
    virtual ~IWeapon() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    virtual void Initialize(Player* owner, Object3dCommon* object3dCommon) = 0;

    /// <summary>
    /// 毎フレームの更新処理
    /// </summary>
    virtual void Update(float gameSpeed, const std::list<std::unique_ptr<Enemy>>& enemies, ArmoredTrainBoss* boss, Object3d* parentCamera) = 0;

    /// <summary>
    /// 入力処理および攻撃処理
    /// </summary>
    virtual void Attack(Input* input,
                        std::list<std::unique_ptr<PlayerBullet>>& bullets,
                        std::list<std::unique_ptr<PlayerMissile>>& missiles,
                        const std::list<std::unique_ptr<Enemy>>& enemies,
                        ArmoredTrainBoss* boss,
                        Object3d* parentCamera,
                        float gameSpeed) = 0;

    /// <summary>
    /// 3Dオブジェクト描画（追加パーツ・浮遊ビット・照準等）
    /// </summary>
    virtual void Draw() = 0;

    /// <summary>
    /// 2D UI描画（残弾数・ゲージ等）
    /// </summary>
    virtual void DrawUI() = 0;

    /// <summary>
    /// 武器を装備した時の初期化・リセット（残弾数回復等）
    /// </summary>
    virtual void Reset() = 0;

    /// <summary>
    /// 弾切れまたは時間切れ判定
    /// </summary>
    virtual bool IsDepleted() const = 0;

    /// <summary>
    /// 武器タイプ取得
    /// </summary>
    virtual MemoryType GetType() const = 0;

    /// <summary>
    /// 残量割合取得（0.0f〜1.0f、UIゲージ用）
    /// </summary>
    virtual float GetRemainingRatio() const { return 1.0f; }

    /// <summary>
    /// 残弾テキスト取得（UI表示用）
    /// </summary>
    virtual std::string GetAmmoText() const { return ""; }
};
