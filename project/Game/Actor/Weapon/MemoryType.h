#pragma once

/// <summary>
/// メモリ（武器形態）の種類定義
/// </summary>
enum class MemoryType {
    NONE = 0,      // 基本形態（通常弾）
    CHARGE_SHOT,   // チャージショット（長押し高威力狙撃弾）
    BOMB,          // 爆弾（画面広範囲・敵弾消去）
    SPREAD,        // 拡散弾（多連装スプレッドショット）
    FUNNEL,        // ファンネル（自律追尾・連続射撃端末）
    CYBER_BIT,     // ビット（サイバー粒子マルチロックオン）
    LASER_BEAM     // レーザービーム（持続照射スイープビーム）
};
