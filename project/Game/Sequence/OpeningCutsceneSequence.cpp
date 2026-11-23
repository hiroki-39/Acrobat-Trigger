#define NOMINMAX
#include "OpeningCutsceneSequence.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/System/RailCameraController.h"
#include "Game/Sequence/BossFinisherSequence.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Graphics/3d/Object/Object3dCommon.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleEffect.h"
#include "KHEngine/Graphics/PostProcess/PostProcess.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/UI/UITextManager.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
void OpeningCutsceneSequence::Start(const OpeningCutsceneContext& ctx)
{
		cutsceneTimer_ = 0.0f;
	missionStartTextTimer_ = 0.0f;
	(*ctx.terrainMaterializeProgress) = 0.0f;
	playerReconstructAlpha_ = 0.0f;

	// 再スタート時に処刑演出およびクリア関連UIテキストを確実にリセット
	if (ctx.bossFinisherSequence)
	{
		ctx.bossFinisherSequence->Reset(ctx.player);
	}
	if (auto textMgr = UITextManager::GetInstance())
	{
		textMgr->SetVisible("ExecuteCritical", false, "GAMEPLAY");
		textMgr->SetVisible("ExecuteGuide", false, "GAMEPLAY");
		textMgr->SetVisible("MissionAccomplishedTitle", false, "GAMEPLAY");
		textMgr->SetVisible("MissionAccomplishedSub", false, "GAMEPLAY");
	}

	// ポリゴン破片モデルのロード
	auto modelMgr = ModelManager::GetInstance();
	if (modelMgr)
	{
		modelMgr->LoadModel("polygon_shard.obj");
	}

	// スラスターおよびスピードラインを進んでいない期間中非表示にするためクリア
	(*ctx.thrusterEffect).ClearParticles();
	(*ctx.windEffect).ClearParticles();

	// 自機を初期位置（レール原点 0, 0, 0）に待機させ、完全ディゾルブ状態（アルファ0）に設定
	if (ctx.player)
	{
		ctx.player->SetTranslate({ 0.0f, 0.0f, 0.0f });
		ctx.player->SetRotation({ 0.0f, 0.0f, 0.0f });
		ctx.player->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f }); // アルファ0で完全消滅ディゾルブ
		ctx.player->SetSelectLightings(8); // サイバー仮想空間 自機シェーダー（case 8）
		ctx.player->SetCutsceneBoosting(false);
		ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
		ctx.player->Update3DObjectOnly();
	}

	// 地形オブジェクト群の初期アルファを 0.0（未形成・暗黒ワイヤーフレーム）に設定
	for (auto& model : (*ctx.modelInstances))
	{
		if (model)
		{
			if (model->IsBackgroundMountain())
			{
				model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // Terrain_Background_Mountainsはスタート演出を行わず最初から通常表示
			}
			else
			{
				model->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
			}
		}
	}
	for (auto& obstacle : (*ctx.obstacles))
	{
		if (obstacle && obstacle->GetObject3d())
		{
			obstacle->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		}
	}

	// 自機復元用 36個のポリゴン破片（ReconstructShard）の生成
	// タイトル画面で四散した破片が逆再生で自機各パーツへと吸い込まれる
	reconstructShards_.clear();
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services ? services->GetObject3dCommon() : nullptr;

	if (object3dCommon)
	{
		std::mt19937 rng(13579);
		std::uniform_real_distribution<float> distRot(-3.14f, 3.14f);
		std::uniform_real_distribution<float> distRotSpd(-10.0f, 10.0f);
		std::uniform_real_distribution<float> distNoise(-0.25f, 0.25f);

		// タイトル画面の四散プリズムと完全に一致させた自機パーツ目標地点リスト（64箇所）
		const Vector3 targetOffsets[64] = {
			// 機首・ノーズコーン（先端〜付け根）
			{  0.0f,  0.02f,  2.5f }, {  0.0f, -0.06f,  2.2f }, {  0.0f,  0.10f,  1.9f }, {  0.0f, -0.10f,  1.6f },
			{ -0.15f, 0.05f,  1.8f }, {  0.15f, 0.05f,  1.8f }, { -0.12f,-0.05f,  1.5f }, {  0.12f,-0.05f,  1.5f },
			// コックピット・キャノピー周辺
			{  0.0f,  0.38f,  1.0f }, {  0.0f,  0.42f,  0.6f }, {  0.0f,  0.30f,  0.2f },
			{ -0.25f, 0.28f,  0.8f }, {  0.25f, 0.28f,  0.8f }, { -0.28f, 0.22f,  0.4f }, {  0.28f, 0.22f,  0.4f },
			// 胴体コア・上面・下面
			{  0.0f,  0.15f,  0.0f }, {  0.0f, -0.22f,  0.0f }, {  0.0f,  0.12f, -0.5f }, {  0.0f, -0.18f, -0.5f },
			{ -0.35f, 0.02f, -0.1f }, {  0.35f, 0.02f, -0.1f }, { -0.42f, 0.05f, -0.6f }, {  0.42f, 0.05f, -0.6f },
			// 左主翼（付け根〜中間〜翼端〜翼後縁）
			{ -0.85f, 0.02f,  0.4f }, { -1.45f, 0.04f,  0.1f }, { -2.10f, 0.06f, -0.3f },
			{ -2.80f, 0.09f, -0.7f }, { -3.50f, 0.12f, -1.1f }, { -3.95f, 0.14f, -1.3f },
			{ -1.80f,-0.02f, -0.7f }, { -2.60f,-0.02f, -1.1f }, { -3.30f, 0.03f, -1.5f },
			// 右主翼（付け根〜中間〜翼端〜翼後縁）
			{  0.85f, 0.02f,  0.4f }, {  1.45f, 0.04f,  0.1f }, {  2.10f, 0.06f, -0.3f },
			{  2.80f, 0.09f, -0.7f }, {  3.50f, 0.12f, -1.1f }, {  3.95f, 0.14f, -1.3f },
			{  1.80f,-0.02f, -0.7f }, {  2.60f,-0.02f, -1.1f }, {  3.30f, 0.03f, -1.5f },
			// 垂直尾翼・V字尾翼
			{  0.0f,  0.65f, -0.8f }, {  0.0f,  1.05f, -1.2f }, {  0.0f,  1.35f, -1.6f },
			{ -0.35f, 0.75f, -1.1f }, {  0.35f, 0.75f, -1.1f }, { -0.55f, 1.15f, -1.4f }, {  0.55f, 1.15f, -1.4f },
			// 左右エアインテーク・エンジンナセル
			{ -0.65f,-0.18f,  0.3f }, {  0.65f,-0.18f,  0.3f },
			{ -0.88f,-0.08f, -0.5f }, {  0.88f,-0.08f, -0.5f },
			// エンジン排気ノズル・アフターバーナー周辺
			{ -0.52f, 0.00f, -1.8f }, {  0.52f, 0.00f, -1.8f },
			{ -0.52f,-0.05f, -2.1f }, {  0.52f,-0.05f, -2.1f },
			{  0.0f, -0.08f, -1.7f }, {  0.0f,  0.05f, -2.0f },
			// 下部ベントラルフィン・キール
			{  0.0f, -0.45f, -0.6f }, {  0.0f, -0.65f, -1.1f },
			{ -0.22f,-0.40f, -0.9f }, {  0.22f,-0.40f, -0.9f }
		};

		for (int i = 0; i < 64; ++i)
		{
			ReconstructShard shard;
			// 目標位置（自機パーツ位置 ＋ わずかな散乱ノイズ）
			shard.targetPos = {
				targetOffsets[i].x + distNoise(rng) * 0.4f,
				targetOffsets[i].y + distNoise(rng) * 0.4f,
				targetOffsets[i].z + distNoise(rng) * 0.4f
			};

			// 開始位置：自機ローカル空間（自機スケール0.5考慮：ローカル半径 18m〜34m → ワールド 9m〜17m）
			// カメラの視野外・手前ギリギリから一気に自機へ飛び込むダイナミックな配置
			float phi = std::acosf(1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / 64.0f);
			float theta = 3.14159265f * (1.0f + std::sqrtf(5.0f)) * static_cast<float>(i);
			float radius = 18.0f + (i % 8) * 2.2f;

			shard.startPos = {
				radius * std::sinf(phi) * std::cosf(theta),
				radius * std::cosf(phi) + 1.2f,
				radius * std::sinf(phi) * std::sinf(theta) + 1.8f
			};

			shard.rotation = { distRot(rng), distRot(rng), distRot(rng) };
			shard.rotSpeed = { distRotSpd(rng) * 1.5f, distRotSpd(rng) * 1.5f, distRotSpd(rng) * 1.5f };

			// スケール：画面ではっきり視認できるよう大幅拡大！（ローカル 2.4〜4.5 → ワールド換算 1.2m〜2.25m）
			// 自機のノーズや主翼パーツと同等サイズの巨大な三角形クリスタルプリズム！
			float sc = 2.4f + (i % 5) * 0.52f;
			shard.scale = { sc, sc, sc };

			// カラー：ブルームとプラズマ発光を強調した超高輝度ネオンカラー
			int cMod = i % 4;
			if (cMod == 0)
				shard.color = { 0.20f, 1.60f, 1.35f, 1.0f }; // エメラルドプラズマ
			else if (cMod == 1)
				shard.color = { 0.35f, 1.45f, 1.80f, 1.0f }; // ネオンエレクトリックシアン
			else if (cMod == 2)
				shard.color = { 0.10f, 1.30f, 0.95f, 1.0f }; // ディーププラズマ
			else
				shard.color = { 1.20f, 1.50f, 1.70f, 1.0f }; // クールホワイトコア（超高エネルギー閃光）

			shard.object = std::make_unique<Object3d>();
			shard.object->Initialize(object3dCommon);
			if (ctx.player && ctx.player->GetObject3d())
			{
				shard.object->SetParent(ctx.player->GetObject3d()); // 自機モデルを親に設定！
			}
			shard.object->SetModel("polygon_shard.obj");
			shard.object->SetSelectLightings(6); // ネオン高輝度プラズマ発光シェーダー
			shard.object->SetColor(shard.color);
			shard.object->SetScale({ 0.0f, 0.0f, 0.0f }); // 最初は非表示
			shard.object->SetRotation(shard.rotation);
			shard.object->SetTranslate(shard.startPos);
			shard.object->Update();

			reconstructShards_.push_back(std::move(shard));
		}
	}

	// 初期カメラ：地面スレスレの超ローアングルから、眼前を走る光の波を見送るドラマチックな構図！
	if (ctx.railCameraController)
	{
		if (services && services->IsDemoMode())
		{
			// デモプレイ時はレールの進行度を 39% の位置から開始し、ミッションスタート直後にボス戦へ移行
			ctx.railCameraController->SetProgress(0.39f);
			ctx.railCameraController->Update(0.0f, { 0.0f, 0.0f, 0.0f });
			if (ctx.player) ctx.player->SetGodMode(true);
		}
		Vector3 camLocalPos = { 2.0f, 0.45f, -12.0f };
		Vector3 lookTarget = { 0.0f, 1.5f, 35.0f };
		ctx.railCameraController->SetCinematicCamera(true, camLocalPos, lookTarget, 1.0f);
	}


}


bool OpeningCutsceneSequence::Update(float dt, const OpeningCutsceneContext& ctx)
{
	bool finished = false;
	auto services = EngineServices::GetInstance();
	auto input = services->GetInput();

	// スキップ判定（SPACEキーまたはゲームパッドA/START）
	// ミッションスタートの文字が出る手前（3.18s）までスキップ
	if (input && (input->TriggerKey(DIK_SPACE) || input->TriggerPadButton(XINPUT_GAMEPAD_A) || input->TriggerPadButton(XINPUT_GAMEPAD_START)))
	{
		const float kMissionStartHandOffTime = 3.18f;
		if (cutsceneTimer_ < kMissionStartHandOffTime)
		{
			cutsceneTimer_ = kMissionStartHandOffTime;
			// 地形マテリアライズ、自機復元、破片クリアを一気に整える
			(*ctx.terrainMaterializeProgress) = 1.0f;
			for (auto& model : (*ctx.modelInstances)) if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			for (auto& obs : (*ctx.obstacles)) if (obs && obs->GetObject3d()) obs->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

			playerReconstructAlpha_ = 1.0f;
			if (ctx.player)
			{
				ctx.player->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				ctx.player->SetCutsceneBoosting(true);
				ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
			}
			reconstructShards_.clear();

			if (auto pp = services ? services->GetPostProcess() : nullptr)
			{
				pp->SetEffectActive("RadialBlur", false);
				if (auto* d = pp->GetData()) d->radialBlurIntensity = 0.0f;
			}
		}
		else
		{
			// MISSION START表示中に再度スキップされた場合は即座にプレイ開始へ
			cutsceneTimer_ = kCutsceneDuration;
		}
	}
	else
	{
		cutsceneTimer_ += dt;
	}

	if (cutsceneTimer_ >= kCutsceneDuration)
	{
		// 演出終了：通常ゲームプレイへ遷移（操作解放）
		finished = true;
		missionStartTextTimer_ = 0.0f; // MISSION START表示をプレイ開始後もしばらく維持
		(*ctx.terrainMaterializeProgress) = 1.0f;
		playerReconstructAlpha_ = 1.0f;
		reconstructShards_.clear();

		// ラジアルブラー等のポストプロセス演出を通常復帰
		if (auto pp = services->GetPostProcess())
		{
			pp->SetEffectActive("RadialBlur", false);
			pp->GetData()->radialBlurIntensity = 0.0f;
		}

		// 地形・障害物のマテリアルアルファを通常（1.0）へ完全復帰
		for (auto& model : (*ctx.modelInstances))
		{
			if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
		for (auto& obstacle : (*ctx.obstacles))
		{
			if (obstacle && obstacle->GetObject3d())
			{
				obstacle->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		if (ctx.player)
		{
			ctx.player->SetCutsceneOverride(false);
			ctx.player->SetCutsceneBoosting(false);
			ctx.player->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			ctx.player->SetTranslate({ 0.0f, 0.0f, 0.0f });
			ctx.player->SetRotation({ 0.0f, 0.0f, 0.0f });
			ctx.player->ResetPosture();
		}
		if (ctx.railCameraController)
		{
			ctx.railCameraController->SetCinematicCamera(false, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f);
		}
		return finished;
	}

	// --- タイムライン進行（全体 4.7秒） ---
	const float kPhase1Duration = 2.20f; // [Phase 1: 0.0s〜2.20s] 地形マテリアライズ（虚空からワイヤー形成、余韻の後にスキャン実体化）
	const float kPhase2Duration = 1.20f; // [Phase 2: 2.20s〜3.40s] 自機リコンストラクト（0.8s集束 ＋ 0.4sバーニア点火）
	const float kPhase3Duration = 0.90f; // [Phase 3: 3.40s〜4.30s] カメラ回り込み＆照準起動
	// 残り0.40s: [Phase 4: 4.30s〜4.70s] レターボックス解放＆MISSION START突入

	if (cutsceneTimer_ < kPhase1Duration)
	{
		// =========================================================================
		// === Phase 1 (0.0s〜1.70s): 地形マテリアライズ（仮想空間の形成をじっくり見せる） ===
		// =========================================================================
		float t = cutsceneTimer_ / kPhase1Duration; // 0.0 -> 1.0
		float u = t * t * (3.0f - 2.0f * t); // SmoothStep
		(*ctx.terrainMaterializeProgress) = u;

		// 地形オブジェクト・障害物に進行度を反映（シェーダー側で手前から奥850mへ光の波が走る）
		for (auto& model : (*ctx.modelInstances))
		{
			if (model)
			{
				if (model->IsBackgroundMountain())
				{
					model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // Terrain_Background_Mountainsはスタート演出を行わず最初から通常表示
				}
				else
				{
					model->SetColor({ 1.0f, 1.0f, 1.0f, (*ctx.terrainMaterializeProgress) });
				}
			}
		}
		for (auto& obstacle : (*ctx.obstacles))
		{
			if (obstacle && obstacle->GetObject3d())
			{
				obstacle->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, (*ctx.terrainMaterializeProgress) });
			}
		}

		// 自機はまだ未出現（完全透明）
		playerReconstructAlpha_ = 0.0f;
		if (ctx.player)
		{
			ctx.player->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
			ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
		}

		// カメラ：地面スレスレ至近から徐々に上昇・前進しながら、前方のビル群と地平線を見渡す壮大なクレーンアップ！
		Vector3 camPos = {
			2.8f + u * 0.8f,
			1.6f + u * 1.6f,
			-14.0f + u * 8.0f
		};
		Vector3 lookTarget = {
			0.0f,
			1.8f + u * 1.8f,
			30.0f + u * 15.0f
		};
		if (ctx.railCameraController)
		{
			ctx.railCameraController->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
		}
	}
	else if (cutsceneTimer_ < kPhase1Duration + kPhase2Duration)
	{
		// =========================================================================
		// === Phase 2 (1.70s〜2.90s): 自機リコンストラクト（ポリゴン集束＆実体化＆バーニア点火） ===
		// =========================================================================
		// 地形形成は完了（通常表示）
		(*ctx.terrainMaterializeProgress) = 1.0f;
		for (auto& model : (*ctx.modelInstances))
		{
			if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}
		for (auto& obstacle : (*ctx.obstacles))
		{
			if (obstacle && obstacle->GetObject3d())
			{
				obstacle->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		float tPhase2 = (cutsceneTimer_ - kPhase1Duration); // 0.0 -> 1.20s
		const float kReconstructTime = 0.85f; // 2.20s〜3.05s（0.85秒）で自機ポリゴン集束＆逆ディゾルブ実体化

		if (tPhase2 < kReconstructTime)
		{
			// --- 三角形プリズムの高速集束（インプロージョン）＆ 自機パズル型実体化 ---
			float tRec = tPhase2 / kReconstructTime; // 0.0 -> 1.0

			// 【演出順序の決定版】：
			// 1. 前半〜中盤（0.0〜0.70）：自機は一切出現せず完全透明（虚空）！
			//    四方八方から64個の三角形プリズムが激しくスピンしながら中心へと猛烈に集結！
			// 2. 終盤（0.70〜0.95）：プリズムがパーツ位置へとギュッと集まりきった瞬間、
			//    閃光とともに自機がパズルのように急激に実体化して出現！
			// 3. フラッシュ発光：合体完了時（0.78〜0.98）に機体エッジ・コアが白熱超発光！
			float flashBoost = 0.0f;
			if (tRec >= 0.70f)
			{
				float subT = (tRec - 0.70f) / 0.25f;
				float alphaT = std::clamp(subT, 0.0f, 1.0f);
				// 鋭く立ち上がるイージングで機体が出現
				playerReconstructAlpha_ = alphaT * alphaT * (3.0f - 2.0f * alphaT);

				// ポリゴンが組み合わさった瞬間の強烈フラッシュ発光（0.78〜0.98）
				if (tRec >= 0.78f && tRec <= 0.98f)
				{
					float fProg = (tRec - 0.78f) / 0.20f;
					flashBoost = std::sinf(fProg * 3.14159265f) * 2.8f;
				}
			}
			else
			{
				playerReconstructAlpha_ = 0.0f; // 集束完了直前までは影も形もない完全透明！
			}

			if (ctx.player)
			{
				Vector4 shipCol = {
					1.0f + flashBoost * 1.2f,
					1.0f + flashBoost * 1.5f,
					1.0f + flashBoost * 1.8f,
					playerReconstructAlpha_
				};
				ctx.player->SetColor(shipCol);
				ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
				ctx.player->Update3DObjectOnly(); // 親オブジェクト（自機）のワールド行列を最新化！
			}

			// 64個の三角形プリズムの座標・姿勢更新（空間全方位から中心へ向かって吸入）
			for (size_t i = 0; i < reconstructShards_.size(); ++i)
			{
				auto& shard = reconstructShards_[i];
				if (!shard.object) continue;

				// 破片ごとに時間差（ディレイ 0.0〜0.14s）を設けて有機的に集束
				float shardDelay = (i % 8) * 0.018f;
				// 0.80sまでに全破片が目標位置に到達するように規格化
				float shardProgress = std::clamp((tRec - shardDelay) / (0.80f - shardDelay), 0.0f, 1.0f);

				// 【キュッと吸い込まれるインプロージョン急加速イージング】：
				// 0.15までタメ、その後パーツ地点へ向かって3.5次急加速で「キュッ！」と一気に吸入！
				float convergeT = 0.0f;
				if (shardProgress > 0.15f)
				{
					float subT = (shardProgress - 0.15f) / 0.85f;
					convergeT = std::pow(subT, 3.5f);
				}

				// 開始位置から目標パーツ位置へ補間
				Vector3 currentPos = {
					shard.startPos.x + (shard.targetPos.x - shard.startPos.x) * convergeT,
					shard.startPos.y + (shard.targetPos.y - shard.startPos.y) * convergeT,
					shard.startPos.z + (shard.targetPos.z - shard.startPos.z) * convergeT
				};

				// 【パズル・スナップ回転】：
				// 破片が空間にある間は高速ランダム回転
				// 機体パーツへ吸い込まれる（shardProgress > 0.55）と回転が急制動され、
				// 自機パーツの姿勢（0, 0, 0）へカチッと吸着スナップ！
				shard.rotation.x += shard.rotSpeed.x * dt;
				shard.rotation.y += shard.rotSpeed.y * dt;
				shard.rotation.z += shard.rotSpeed.z * dt;

				Vector3 currentRot = shard.rotation;
				if (shardProgress > 0.55f)
				{
					float snapT = std::clamp((shardProgress - 0.55f) / 0.38f, 0.0f, 1.0f);
					float snapSmooth = snapT * snapT * (3.0f - 2.0f * snapT); // SmoothStep
					currentRot.x = currentRot.x * (1.0f - snapSmooth);
					currentRot.y = currentRot.y * (1.0f - snapSmooth);
					currentRot.z = currentRot.z * (1.0f - snapSmooth);
				}

				// スケール：出現時は瞬時にフル展開し、自機パーツに合体・同化する瞬間（shardProgress > 0.85f）に機体へ吸収
				float appearT = std::clamp(shardProgress / 0.08f, 0.0f, 1.0f);
				float shardScale = (appearT * appearT * (3.0f - 2.0f * appearT)) * shard.scale.x;
				if (shardProgress > 0.85f)
				{
					float absorbT = (shardProgress - 0.85f) / 0.15f;
					shardScale *= (1.0f - absorbT * absorbT);
				}

				shard.object->SetCamera(ctx.activeCamera);
				shard.object->SetScale({ shardScale, shardScale, shardScale });
				shard.object->SetRotation(currentRot);
				shard.object->SetTranslate(currentPos);
				shard.object->Update();
			}

			// カメラ：自機にグッと寄った迫力あるクローズアップ＆ワイド構図！
			// 自機が画面の大半を占め、カメラの手前や周囲から巨大な三角形プリズムが飛び交う迫真の映像！
			float q = tRec * tRec * (3.0f - 2.0f * tRec);
			Vector3 camPos = {
				2.6f * (1.0f - q * 0.30f),  // 2.6 -> 1.82
				1.5f * (1.0f - q * 0.42f),  // 1.5 -> 0.87
				3.8f * (1.0f - q * 0.32f)   // 3.8 -> 2.58
			};
			Vector3 lookTarget = { 0.0f, 0.12f * (1.0f - q * 0.2f), 0.35f * (1.0f - q * 0.7f) };
			if (ctx.railCameraController)
			{
				ctx.railCameraController->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
			}
		}
		else
		{
			// --- 自機復元完了後のバーニア点火＆待機フェーズ（3.05s〜3.40s） ---
			playerReconstructAlpha_ = 1.0f;
			const float kBoosterIgnitionTime = kPhase1Duration + kReconstructTime;
			float ignitionElapsed = cutsceneTimer_ - kBoosterIgnitionTime; // 0.0 -> 0.35s

			// 点火余熱フラッシュ（点火瞬間に白熱し、0.25秒で通常色へクールダウン）
			float ignFlash = std::clamp(1.0f - (ignitionElapsed / 0.25f), 0.0f, 1.0f);
			Vector4 ignCol = {
				1.0f + ignFlash * 1.4f,
				1.0f + ignFlash * 1.8f,
				1.0f + ignFlash * 2.2f,
				1.0f
			};

			if (ctx.player)
			{
				ctx.player->SetColor(ignCol);
				ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

				// 自機集束完了（kPhase1Duration + kReconstructTime）でバーニア点火！
				if (cutsceneTimer_ >= kBoosterIgnitionTime)
				{
					if (!ctx.player->IsCutsceneBoosting())
					{
						ctx.player->SetCutsceneBoosting(true);

						// 【点火インパクト強化 1: 出撃カメラシェイク（微小な手応え振動）】
						(*ctx.cameraShakeTimer) = 8.0f; // 約8フレームの微小インパルス

						// 【点火インパクト強化 2: ラジアルブラー発動（出撃の急加速推進感）】
						if (auto pp = services->GetPostProcess())
						{
							pp->SetEffectActive("RadialBlur", true);
							pp->GetData()->radialBlurIntensity = 0.08f;
							pp->GetData()->radialBlurCenterX = 0.5f;
							pp->GetData()->radialBlurCenterY = 0.55f;
						}

						// 【点火インパクト強化 3: スピード線（突風・衝撃波）を機体周囲に走らせる】
						Vector3 shipWPos = ctx.player->GetWorldPosition();
						(*ctx.windEffect).SetPosition(shipWPos);
						for (int w = 0; w < 4; ++w)
						{
							(*ctx.windEffect).Play();
						}
					}
				}
			}

			// ラジアルブラーの滑らかなフェードアウト（0.35秒かけて 0.08 -> 0.0 へ）
			if (auto pp = services->GetPostProcess())
			{
				float blurFade = std::clamp(1.0f - (ignitionElapsed / 0.35f), 0.0f, 1.0f);
				if (blurFade > 0.001f)
				{
					pp->GetData()->radialBlurIntensity = 0.08f * blurFade * blurFade;
				}
				else
				{
					pp->SetEffectActive("RadialBlur", false);
					pp->GetData()->radialBlurIntensity = 0.0f;
				}
			}

			// 破片は非表示
			for (auto& shard : reconstructShards_)
			{
				if (shard.object) shard.object->SetScale({ 0.0f, 0.0f, 0.0f });
			}

			// カメラ：自機正面斜め前から力強く機体を捉える（前フェーズからシームレス接続）
			Vector3 camPos = { 1.82f, 0.87f, 2.58f };
			Vector3 lookTarget = { 0.0f, 0.10f, 0.10f };
			if (ctx.railCameraController)
			{
				ctx.railCameraController->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
			}
		}
	}
	else if (cutsceneTimer_ < kPhase1Duration + kPhase2Duration + kPhase3Duration)
	{
		// =========================================================================
		// === Phase 3 (2.90s〜3.80s): カメラが斜め前から背後へ滑らかに回り込み ===
		// =========================================================================
		float tPhase3 = (cutsceneTimer_ - (kPhase1Duration + kPhase2Duration)) / kPhase3Duration; // 0.0 -> 1.0
		float s = tPhase3 * tPhase3 * (3.0f - 2.0f * tPhase3); // SmoothStep

		// 自機はバーニア全開で前進態勢を維持
		if (ctx.player)
		{
			ctx.player->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			ctx.player->SetCutsceneBoosting(true);
			ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
		}

		// カメラの回り込み軌道（2次ベジェ曲線）:
		// 始点 P0: 自機斜め前至近 { 2.2f, 0.8f, 3.8f }
		// 経由点 P1: 自機右横・斜め後ろ { 5.8f, 2.2f, -2.0f }
		// 終点 P2: 通常カメラ位置 { 0.0f, 2.5f, -8.0f }
		Vector3 p0 = { 2.2f, 0.8f, 3.8f };
		Vector3 p1 = { 5.8f, 2.2f, -2.0f };
		Vector3 p2 = { 0.0f, 2.5f, -8.0f };

		float invS = 1.0f - s;
		Vector3 camPos = {
			invS * invS * p0.x + 2.0f * invS * s * p1.x + s * s * p2.x,
			invS * invS * p0.y + 2.0f * invS * s * p1.y + s * s * p2.y,
			invS * invS * p0.z + 2.0f * invS * s * p1.z + s * s * p2.z
		};

		// ブレンド率: 1.0 -> 0.0（背後に回り込みながら通常カメラの姿勢へ完全同期）
		float blend = 1.0f - s;
		if (ctx.railCameraController)
		{
			ctx.railCameraController->SetCinematicCamera(true, camPos, { 0.0f, 0.0f, 0.0f }, blend);
		}
	}
	else
	{
		// =========================================================================
		// === Phase 4 (3.80s〜4.20s): 背後通常視点に完全合流（直後に操作解禁） ===
		// =========================================================================
		if (ctx.player)
		{
			ctx.player->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			ctx.player->SetCutsceneBoosting(true);
			ctx.player->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
		}
		if (ctx.railCameraController)
		{
			ctx.railCameraController->SetCinematicCamera(true, { 0.0f, 2.5f, -8.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f);
		}
	}
	return finished;
}


void OpeningCutsceneSequence::DrawUI(bool isCutscenePhase, bool isPlayingPhase)
{
#ifdef USE_IMGUI
	if (ImGui::GetCurrentContext() == nullptr) return;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) return;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	if (!drawList) return;

	// ゲーム画面の表示領域を取得（エディタモード時はゲーム画面ビューポート内、フルスクリーン時は全画面）
	ImVec2 viewPos(0.0f, 0.0f);
	ImVec2 viewSize = viewport->Size;
#ifdef ENABLE_EDITOR
	if (EngineServices::GetInstance()->GetEditorMode())
	{
		viewPos = EditorSystem::GetInstance()->GetViewportPos();
		viewSize = EditorSystem::GetInstance()->GetViewportSize();
		if (viewSize.x <= 10.0f || viewSize.y <= 10.0f) return;
	}
#endif

	float screenW = viewSize.x;
	float screenH = viewSize.y;

	// トランジション（画面遷移）中のアルファ制御
	float transitionAlpha = 1.0f;
	auto sceneManager = EngineServices::GetInstance()->GetSceneManager();
	if (sceneManager && sceneManager->IsTransitioning())
	{
		auto state = sceneManager->GetTransitionState();
		float prog = sceneManager->GetTransitionProgress();
		switch (state)
		{
		case SceneManager::TransitionState::FadeOut:
			transitionAlpha = (std::max)(0.0f, 1.0f - prog);
			break;
		case SceneManager::TransitionState::FadeOutHold:
		case SceneManager::TransitionState::Loading:
		case SceneManager::TransitionState::FadeInWait:
			transitionAlpha = 0.0f;
			break;
		case SceneManager::TransitionState::FadeIn:
			transitionAlpha = (std::min)(1.0f, prog);
			break;
		default:
			transitionAlpha = 1.0f;
			break;
		}
	}

	// ゲーム画面の枠外にはみ出さないようにクリッピング
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	// 画面遷移の完全暗転中はUIテキストを描画せず、ビューポートを真っ黒にしてテキストを完全に「画面遷移の後ろ」に隠す
	if (transitionAlpha <= 0.001f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
		drawList->PopClipRect();
		return;
	}

	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.3f) scale = 0.3f;

	// === 上下の黒帯（シネマスコープ・映画風レターボックス演出） ===
	float barHeightMax = screenH * 0.11f;
	float barHeight = 0.0f;

	if (isCutscenePhase)
	{
		if (cutsceneTimer_ < 0.30f)
		{
			barHeight = barHeightMax * (cutsceneTimer_ / 0.30f);
		}
		else if (cutsceneTimer_ >= (kCutsceneDuration - 0.50f))
		{
			float t = (cutsceneTimer_ - (kCutsceneDuration - 0.50f)) / 0.50f;
			barHeight = barHeightMax * (1.0f - std::clamp(t, 0.0f, 1.0f));
		}
		else
		{
			barHeight = barHeightMax;
		}
	}
	else if (isPlayingPhase && missionStartTextTimer_ > 0.0f)
	{
		barHeight = 0.0f;
	}

	if (barHeight > 0.5f)
	{
		// 上の黒帯
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barHeight), IM_COL32(0, 0, 0, 255));
		// 下の黒帯
		drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barHeight), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
	}

	// カットシーン中のみスキップ案内（ゲーム画面右下）
	if (isCutscenePhase)
	{
		const char* skipText = "[SPACE] SKIP";
		drawList->AddText(ImVec2(viewPos.x + screenW - 140.0f * scale, viewPos.y + screenH - barHeight - 30.0f * scale), IM_COL32(200, 200, 200, 180), skipText);

		// サイバーHUDステータス表示（左下レターボックス上）
		char statusBuf[64];
		if (cutsceneTimer_ < 1.70f)
		{
			float p = std::clamp(cutsceneTimer_ / 1.70f * 100.0f, 0.0f, 100.0f);
			snprintf(statusBuf, sizeof(statusBuf), "GRID INITIALIZE: %3.0f%%", p);
		}
		else if (cutsceneTimer_ < 2.50f)
		{
			float p = std::clamp((cutsceneTimer_ - 1.70f) / 0.80f * 100.0f, 0.0f, 100.0f);
			snprintf(statusBuf, sizeof(statusBuf), "VESSEL MATERIALIZING: %3.0f%%", p);
		}
		else if (cutsceneTimer_ < 3.20f)
		{
			snprintf(statusBuf, sizeof(statusBuf), "ALL SYSTEMS ONLINE");
		}
		else
		{
			snprintf(statusBuf, sizeof(statusBuf), "COMBAT MODE ENGAGED");
		}
		drawList->AddText(ImVec2(viewPos.x + 30.0f * scale, viewPos.y + screenH - barHeight - 30.0f * scale), IM_COL32(80, 220, 255, 210), statusBuf);
	}

	auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
	auto textMgr = UITextManager::GetInstance();

	// 汎用アイテム描画ラムダ（UITextManager設定に完全同期）
	auto renderTextItem = [&](const UITextItem* item, float alpha, float customScale = 1.0f, const std::string& overrideText = "") {
		if (!item || item->text.empty() || alpha <= 0.001f) return;
		ImFont* font = imguiManager ? imguiManager->GetFont(item->fontType) : ImGui::GetFont();
		if (!font) font = ImGui::GetFont();

		float fSize = item->fontSize * scale * customScale;
		const std::string& str = overrideText.empty() ? item->text : overrideText;
		ImVec2 tSize = font->CalcTextSizeA(fSize, FLT_MAX, -1.0f, str.c_str());

		float posX = viewPos.x + item->position.x * (screenW / 1280.0f);
		float posY = viewPos.y + item->position.y * (screenH / 720.0f);
		ImVec2 tPos(posX, posY);
		if (item->align == UITextAlign::Center)
		{
			tPos.x -= tSize.x * 0.5f;
		}
		else if (item->align == UITextAlign::Right)
		{
			tPos.x -= tSize.x;
		}

		if (item->hasShadow)
		{
			ImU32 shadowCol = ImColor(item->shadowColor.x, item->shadowColor.y, item->shadowColor.z, item->shadowColor.w * alpha);
			ImVec2 sPos(tPos.x + item->shadowOffset.x * scale, tPos.y + item->shadowOffset.y * scale);
			drawList->AddText(font, fSize, sPos, shadowCol, str.c_str());
		}

		if (item->hasOutline)
		{
			ImU32 outlineCol = ImColor(item->outlineColor.x, item->outlineColor.y, item->outlineColor.z, item->outlineColor.w * alpha);
			float thick = item->outlineThickness * scale;
			const float offsets[8][2] = {
				{ -thick, -thick }, { 0.0f, -thick }, { thick, -thick },
				{ -thick, 0.0f },                     { thick, 0.0f },
				{ -thick, thick },  { 0.0f, thick },  { thick, thick }
			};
			for (int k = 0; k < 8; ++k)
			{
				drawList->AddText(font, fSize, ImVec2(tPos.x + offsets[k][0], tPos.y + offsets[k][1]), outlineCol, str.c_str());
			}
		}

		ImU32 textCol = ImColor(item->color.x, item->color.y, item->color.z, item->color.w * alpha);
		drawList->AddText(font, fSize, tPos, textCol, str.c_str());
	};

	// 演出中のテキスト表示（UITextManagerの登録プロパティを参照）
	if (isCutscenePhase && cutsceneTimer_ >= 2.50f && cutsceneTimer_ < 3.20f)
	{
		float t = (cutsceneTimer_ - 2.50f) / 0.70f;
		float alpha = std::clamp(std::sinf(t * 3.14159265f), 0.0f, 1.0f);

		const UITextItem* readyItem = textMgr ? textMgr->GetTextItem("CutsceneReady", "GAMEPLAY") : nullptr;
		if (readyItem)
		{
			renderTextItem(readyItem, alpha);
		}
	}
	else if (isCutscenePhase && cutsceneTimer_ >= 3.20f && cutsceneTimer_ < 4.60f)
	{
		float alpha = 1.0f;
		if (cutsceneTimer_ >= 4.20f)
		{
			alpha = 1.0f - std::clamp((cutsceneTimer_ - 4.20f) / 0.40f, 0.0f, 1.0f);
		}

		const UITextItem* startItem = textMgr ? textMgr->GetTextItem("CutsceneMissionStart", "GAMEPLAY") : nullptr;
		if (startItem)
		{
			renderTextItem(startItem, alpha);
		}
	}

	// 画面遷移（トランジション）の黒カーテンをテキストの手前に被せることで、テキストを完全に「画面遷移の後ろ」に配置
	if (transitionAlpha < 0.999f)
	{
		float blackAlpha = std::clamp(1.0f - transitionAlpha, 0.0f, 1.0f);
		drawList->AddRectFilled(
			viewPos,
			ImVec2(viewPos.x + screenW, viewPos.y + screenH),
			IM_COL32(0, 0, 0, static_cast<int>(255.0f * blackAlpha))
		);
	}

	drawList->PopClipRect();
#endif
}

void OpeningCutsceneSequence::Draw3D(Camera* activeCamera, Object3dCommon* object3dCommon)
{
	if (cutsceneTimer_ >= 2.20f && cutsceneTimer_ < 3.05f)
	{
		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
		for (auto& shard : reconstructShards_)
		{
			if (shard.object)
			{
				shard.object->SetCamera(activeCamera);
				shard.object->Draw();
			}
		}
	}
}

void OpeningCutsceneSequence::TickMissionStartText(float dt)
{
	if (missionStartTextTimer_ > 0.0f)
	{
		missionStartTextTimer_ -= dt;
	}
}

void OpeningCutsceneSequence::ForceComplete(float& terrainMaterializeProgress)
{
	terrainMaterializeProgress = 1.0f;
	playerReconstructAlpha_ = 1.0f;
	reconstructShards_.clear();
}
