#define NOMINMAX
#include "GamePlayEditor.h"
#include "GamePlayScene.h"

#ifdef ENABLE_EDITOR
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include "externals/imgui/imgui.h"
#include "KHEngine/Debug/Editor/EffectStudio.h"
#include "KHEngine/Debug/Editor/EnemyStudio.h"
#include "KHEngine/Debug/Editor/EditorSystem.h"
#include "Game/Actor/Enemy/EnemyPresetManager.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/UI/UITextManager.h"
#include "externals/nlohmann/json.hpp"
#include "Game/System/TerrainCollisionDebugger.h"
#include "Game/System/CyberVfxConfig.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

void GamePlayEditor::Draw(GamePlayScene& scene)
{
	auto services = EngineServices::GetInstance();
	// フレーム描画スコープ外（NewFrame前）の場合はImGui描画をスキップ
	if (!ImGui::GetCurrentContext() || ImGui::GetFrameCount() <= 0)
	{
		return;
	}

	// エディターモードが無効（全画面プレイ時）はエディタUIを描画しない
	if (!services || !services->GetEditorMode())
	{
		return;
	}

	// =========================================================================
	// 統合エディタUI: [左] メニュー  [右] インスペクター  [下] タイムライン
	// =========================================================================
		const char* navItems[] = {
		"ゲーム・進行",
		"プレイヤー",
		"エネミー・装甲列車ボス",
		"演出・シェーダー",
		"シーン・照明",
		"UIテキスト配置"
	};

	// -------------------------------------------------------------
	// 1. [左ドック用] メニュー (Category Selector & Performance)
	// -------------------------------------------------------------
	if (ImGui::Begin("メニュー"))
	{
		ImGui::TextColored(ImVec4(0.3f, 0.75f, 1.0f, 1.0f), "CATEGORY");
		ImGui::Separator();

		for (int i = 0; i < IM_ARRAYSIZE(navItems); ++i)
		{
			bool isSelected = (currentNavIndex_ == i);
			if (ImGui::Selectable(navItems[i], isSelected, 0, ImVec2(0, 30)))
			{
				currentNavIndex_ = i;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("F1: UI表示切替");
		if (ImGui::Button("全画面プレイ (F1)", ImVec2(-1, 28)))
		{
			EngineServices::GetInstance()->SetEditorMode(false);
		}
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.22f, 0.22f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
		if (ImGui::Button("★ 大砲演出テスト (TimeOver Cannon)", ImVec2(-1, 28)))
		{
			scene.TriggerTimeOverCannonSequence(true);
		}
		ImGui::PopStyleColor(2);
		if (false)
		{
		}

		// オーディオ・ミュートコントロール
		SoundManager::GetInstance()->DrawImGuiAudioControls();

		// サイバーVFX設定ウィンドウの表示切替
		static bool showCyberVfxConfig = false;
		ImGui::Checkbox("サイバーVFX設定 (JSON)", &showCyberVfxConfig);
		if (showCyberVfxConfig)
		{
			CyberVfxConfig::GetInstance()->DrawImGui(&showCyberVfxConfig);
		}

		// 最下部にパフォーマンス情報を常時表示
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.5f, 1.0f), "PERFORMANCE");

		float fps = ImGui::GetIO().Framerate;
		float frameTime = 1000.0f / (fps > 0.0f ? fps : 1.0f);

		ImGui::Text("FPS: %.1f (%.2f ms)", fps, frameTime);
		if (scene.GetLastLoadTimeMs() > 0.0f)
		{
			ImGui::TextDisabled("Load: %.1f ms", scene.GetLastLoadTimeMs());
		}

						fpsHistory_[historyOffset_] = fps;
		historyOffset_ = (historyOffset_ + 1) % IM_ARRAYSIZE(fpsHistory_);

		ImGui::PlotLines("##FPSMiniGraph", fpsHistory_, IM_ARRAYSIZE(fpsHistory_), historyOffset_, nullptr, 0.0f, 120.0f, ImVec2(-1, 42));
	}
	ImGui::End();

	// -------------------------------------------------------------
	// 2. [右ドック用] インスペクター (Inspector Content)
	// -------------------------------------------------------------
	if (ImGui::Begin("インスペクター"))
	{
		// 1. ゲーム・進行
		if (currentNavIndex_ == 0)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ ゲーム・進行・レール制御");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("シーン切り替え (Scene Selector)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (auto sm = scene.GetSceneManager())
				{
					sm->DrawSceneSelectorUI();
				}
			}

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("ゲームプレイ制御 (Playback Control)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool doReset = false;

				if (scene.IsPlaying())
				{
					if (ImGui::Button("一時停止 (Pause)", ImVec2(130, 36)))
					{
						scene.SetPlaying(false);
						services->SetGamePlaying(false);
					}
					ImGui::SameLine();
					if (ImGui::Button("リセット (Reset)", ImVec2(110, 36)))
					{
						doReset = true;
						scene.SetPlaying(false);
						services->SetGamePlaying(false);
					}
					ImGui::SameLine();
					if (ImGui::Button("全画面 (F1で復帰)", ImVec2(140, 36)))
					{
						services->SetEditorMode(false);
						services->SetGamePlaying(true);
					}
					ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "状態: プレイ中 (PLAYING)");
				}
				else
				{
					if (ImGui::Button("開始 (Play)", ImVec2(130, 36)))
					{
						scene.SetPlaying(true);
						services->SetGamePlaying(true);
					}
					ImGui::SameLine();
					if (ImGui::Button("全画面プレイ", ImVec2(140, 36)))
					{
						scene.SetPlaying(true);
						services->SetGamePlaying(true);
						services->SetEditorMode(false);
					}
					ImGui::SameLine();
					if (ImGui::Button("リセット", ImVec2(100, 36)))
					{
						doReset = true;
						services->SetGamePlaying(false);
					}
					ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "状態: 停止中 (STOPPED - フリーカメラ可能)");
					ImGui::TextDisabled("※フリーカメラ: WASD/QE移動, 右クリックドラッグ回転");
				}

				ImGui::Spacing();
				ImGui::TextDisabled("--- シネマティック演出テスト ---");
				if (ImGui::Button("降下演出を再生 (Replay Cutscene)", ImVec2(240, 34)))
				{
					scene.SetPlaying(true);
					services->SetGamePlaying(true);
					scene.StartOpeningCutscene();
				}
				if (scene.GetGamePhase() == GamePlayScene::GamePhase::START_CUTSCENE)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "進行中: %.2f / %.2f 秒", scene.GetOpeningSequence()->GetTimer(), scene.GetOpeningSequence()->GetDuration());
				}

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.35f, 1.0f));
				if (ImGui::Button("クリア演出を再生 (Test Clear)", ImVec2(240, 34)))
				{
					scene.SetPlaying(true);
					services->SetGamePlaying(true);
					if (scene.GetPlayer()) scene.GetPlayer()->SetDead(false);
					scene.SetGamePhase(GamePlayScene::GamePhase::PLAYING);
					scene.StartClearSequence();
				}
				ImGui::PopStyleColor(2);
				if (scene.GetGamePhase() == GamePlayScene::GamePhase::CLEAR)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "クリア進行中 (Timer: %.2fs)", scene.GetClearSequence()->GetTotalTimer());
				}

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.12f, 0.35f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.20f, 0.50f, 1.0f));
				if (ImGui::Button("【処刑テスト】ボスHPを1にする（処刑可能化 & プレイヤー射撃へ）", ImVec2(380, 34)))
				{
					scene.SetPlaying(true);
					services->SetGamePlaying(true);

					// オープニングカットシーン中だった場合も即座に全地形・自機を実体化
					float terrainProg = scene.GetTerrainMaterializeProgress();
					scene.GetOpeningSequence()->ForceComplete(terrainProg);
					scene.SetTerrainMaterializeProgress(terrainProg);
					for (const auto& model : scene.GetModelInstances()) { if (model) model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); }
					for (const auto& obstacle : scene.GetObstacles()) { if (obstacle && obstacle->GetObject3d()) obstacle->GetObject3d()->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); }
					if (auto* player = scene.GetPlayer())
					{
						player->SetDead(false);
						player->SetCutsceneOverride(false);
						player->SetCutsceneBoosting(false);
						player->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
						player->SetVisible(true);
					}
					if (auto* railCam = scene.GetRailCameraController())
					{
						railCam->SetCinematicCamera(false, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f);
						railCam->SetBaseSpeed(50.0f);
						railCam->SetSpeedMultiplier(1.0f);
					}

					scene.SetGamePhase(GamePlayScene::GamePhase::PLAYING);
					auto* bossBattle = scene.GetBossBattleController();
					bool wasNotSpawned = (!scene.IsBossSpawned() || bossBattle->GetStep() == BossBattleController::Step::NOT_ACTIVE);
					scene.SetBossSpawned(true);
					bossBattle->ForceStartBattle();

					if (auto* boss = scene.GetArmoredTrainBoss())
					{
						if (wasNotSpawned)
						{
							boss->ResetChaseState();
							if (auto* bossRail = scene.GetBossRail(); bossRail && bossRail->IsValid())
							{
								if (auto* railCam = scene.GetRailCameraController())
								{
									if (railCam->GetProgress() < bossBattle->GetSpawnProgressThreshold())
									{
										railCam->SetProgress(bossBattle->GetSpawnProgressThreshold());
									}
								}
								boss->SetRail(bossRail);
								boss->SetRailProgress(0.12f);
							}
							else if (auto* railCam = scene.GetRailCameraController(); railCam && !scene.GetMainRails().empty())
							{
								float bCurProg = railCam->GetProgress();
								float bossProg = (std::min)(0.98f, bCurProg + 0.08f);
								boss->SetRail(scene.GetMainRails()[0].get());
								boss->SetRailProgress(bossProg);
							}
						}
						boss->SetHpToOneForExecution();
						boss->SetActive(true);
					}
					if (auto* finisher = scene.GetBossFinisherSequence())
					{
						finisher->StartSequence(scene.GetArmoredTrainBoss(), scene.GetPlayer());
					}
				}
				ImGui::PopStyleColor(2);

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.20f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.25f, 0.25f, 1.0f));
				if (ImGui::Button("ゲームオーバー演出を再生 (Test GameOver)", ImVec2(240, 34)))
				{
					scene.SetPlaying(true);
					services->SetGamePlaying(true);
					scene.SetGamePhase(GamePlayScene::GamePhase::PLAYING);
					scene.StartGameOverSequence();
				}
				ImGui::PopStyleColor(2);
				if (ImGui::TreeNode("ゲームオーバーUI・アンダーバー設定 (GameOver Underline)"))
				{
					auto* goSeq = scene.GetGameOverSequence();
					bool debugPreview = goSeq->IsDebugPreviewUI();
					if (ImGui::Checkbox("UIプレビュー表示 (Preview UI)", &debugPreview))
					{
						goSeq->SetDebugPreviewUI(debugPreview);
					}
					ImGui::SameLine();
					ImGui::TextDisabled("※即座に画面上で確認可能");

					auto textMgr = UITextManager::GetInstance();
					UITextItem* titleItemEdit = textMgr ? textMgr->GetTextItem("GameOverTitle", "GAMEPLAY") : nullptr;
					if (titleItemEdit)
					{
						bool changed = false;
						if (ImGui::Checkbox("アンダーバーを表示", &titleItemEdit->hasUnderline)) changed = true;
						if (ImGui::SliderFloat("線の幅 (Width)", &titleItemEdit->underlineWidth, 0.0f, 800.0f, "%.1f px")) changed = true;
						if (ImGui::SliderFloat("線の太さ (Thickness)", &titleItemEdit->underlineThickness, 0.5f, 20.0f, "%.1f px")) changed = true;
						if (ImGui::SliderFloat("Yオフセット (Offset Y)", &titleItemEdit->underlineOffsetY, -50.0f, 150.0f, "%.1f px")) changed = true;
						if (ImGui::ColorEdit4("線の色 (Color)", &titleItemEdit->underlineColor.x)) changed = true;
						if (ImGui::Button("初期値にリセット"))
						{
							titleItemEdit->hasUnderline = true;
							titleItemEdit->underlineWidth = 320.0f;
							titleItemEdit->underlineThickness = 2.5f;
							titleItemEdit->underlineOffsetY = 42.0f;
							titleItemEdit->underlineColor = { 0.92f, 0.12f, 0.18f, 0.86f };
							changed = true;
						}
						if (changed)
						{
							textMgr->SaveSettings();
						}
					}
					else
					{
						bool barVis = goSeq->IsBarVisible();
						if (ImGui::Checkbox("アンダーバーを表示", &barVis)) goSeq->SetBarVisible(barVis);
						float barW = goSeq->GetBarWidth();
						if (ImGui::SliderFloat("線の幅 (Width)", &barW, 0.0f, 800.0f, "%.1f px")) goSeq->SetBarWidth(barW);
						float barT = goSeq->GetBarThickness();
						if (ImGui::SliderFloat("線の太さ (Thickness)", &barT, 0.5f, 20.0f, "%.1f px")) goSeq->SetBarThickness(barT);
						float barY = goSeq->GetBarOffsetY();
						if (ImGui::SliderFloat("Yオフセット (Offset Y)", &barY, -50.0f, 150.0f, "%.1f px")) goSeq->SetBarOffsetY(barY);
						const Vector4& bc = goSeq->GetBarColor();
						float col[4] = { bc.x, bc.y, bc.z, bc.w };
						if (ImGui::ColorEdit4("線の色 (Color)", col))
						{
							goSeq->SetBarColor({ col[0], col[1], col[2], col[3] });
						}
					}
					ImGui::TreePop();
				}
				if (scene.GetGamePhase() == GamePlayScene::GamePhase::GAMEOVER)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "ゲームオーバー進行中 (Timer: %.2fs)", scene.GetGameOverSequence()->GetTimer());
				}

				ImGui::Separator();
				if (ImGui::Button("レベル再読込 (F5)", ImVec2(160, 32)))
				{
					scene.ReloadLevel();
				}
				ImGui::SameLine();
				if (ImGui::Button("敵を全リスポーン", ImVec2(160, 32)))
				{
					scene.ReloadEnemiesOnly();
				}

				ImGui::Separator();
				if (auto* railCam = scene.GetRailCameraController())
				{
					float p = railCam->GetProgress();
					if (ImGui::SliderFloat("レール進行度 (Progress)", &p, 0.0f, 1.0f, "%.3f"))
					{
						railCam->SetProgress(p);
					}
				}
				float baseSpeed = scene.GetBaseGameSpeed();
				if (ImGui::SliderFloat("ゲーム速度 (Game Speed)", &baseSpeed, 0.0f, 5.0f, "%.2fx"))
				{
					scene.SetBaseGameSpeed(baseSpeed);
				}
				float bgmVol = SoundManager::GetInstance()->GetBGMVolume();
				if (ImGui::SliderFloat("BGM音量", &bgmVol, 0.0f, 1.0f, "%.2f"))
				{
					SoundManager::GetInstance()->SetBGMVolume(bgmVol);
				}

				if (services && services->IsDemoMode())
				{
					auto* demoCtrl = scene.GetDemoController();
					ImGui::Separator();
					ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "【デモプレイ制御 (Demo Mode)】");
					float postIntro = demoCtrl->GetPostIntroDuration();
					if (ImGui::SliderFloat("スタート演出完了後の待機時間 (秒)##DemoPostIntro", &postIntro, 0.5f, 6.0f, "%.1f 秒"))
					{
						demoCtrl->SetPostIntroDuration(postIntro);
					}
					if (scene.GetGamePhase() != GamePlayScene::GamePhase::START_CUTSCENE)
					{
						ImGui::ProgressBar(std::clamp(demoCtrl->GetTimer() / demoCtrl->GetPostIntroDuration(), 0.0f, 1.0f), ImVec2(-1, 14));
						ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.5f, 1.0f), "タイトル帰還まで: %.1f / %.1f 秒", demoCtrl->GetTimer(), demoCtrl->GetPostIntroDuration());
					}
					else
					{
						ImGui::TextDisabled("※スタート演出（降下）再生中... 完了後にカウント開始");
					}
				}

				// リセット処理
				if (doReset)
				{
					scene.SetPlaying(false);
					scene.SetGamePhase(GamePlayScene::GamePhase::PLAYING);
					if (auto* finisher = scene.GetBossFinisherSequence())
					{
						finisher->Reset(scene.GetPlayer());
					}
					if (auto* railCam = scene.GetRailCameraController())
					{
						railCam->Reset();
					}
					if (!scene.GetMainRails().empty() && scene.GetMainRails()[0]->IsValid())
					{
						Vector3 railForward = scene.GetMainRails()[0]->GetForward(0.0f);
						float yaw = std::atan2(railForward.x, railForward.z);
						float pitch = std::asin(-railForward.y);
						float railTilt = scene.GetMainRails()[0]->GetTilt(0.0f);
						scene.ResetCameraState({ pitch, yaw, 0.0f }, yaw, railTilt);
					}
				}

				ImGui::Separator();
				bool drawRail = scene.IsDrawRail();
				if (ImGui::Checkbox("レール軌跡を表示 (Draw Rail)", &drawRail)) scene.SetDrawRail(drawRail);
				ImGui::SameLine();
				bool drawCol = scene.IsDrawCollider();
				if (ImGui::Checkbox("コライダーを表示 [F2]", &drawCol)) scene.SetDrawCollider(drawCol);
				ImGui::SameLine();
				bool drawWire = scene.IsDrawTerrainWireframe();
				if (ImGui::Checkbox("判定計算ポリゴンのみ表示 [F3]", &drawWire)) scene.SetDrawTerrainWireframe(drawWire);

				bool showColMonitor = TerrainCollisionDebugger::GetInstance()->IsShowImGuiMonitor();
				if (ImGui::Checkbox("地形コリジョン解析モニター [F6]", &showColMonitor))
				{
					TerrainCollisionDebugger::GetInstance()->SetShowImGuiMonitor(showColMonitor);
				}
			}

			if (auto* railCam = scene.GetRailCameraController())
			{
				ImGui::Spacing();
				if (ImGui::CollapsingHeader("レールカメラ設定 (Rail Camera)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					railCam->DrawImGuiContent();
				}
			}
		}
		// 2. プレイヤー
		else if (currentNavIndex_ == 1)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ プレイヤー設定・チート");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("強化リング獲得UIデバッグ (Enhance Ring Icons)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto* hud = scene.GetHUD();
				int ringCount = hud->GetAcquiredEnhanceRingCount();
				if (ImGui::SliderInt("獲得リング数", &ringCount, 0, GamePlayHUD::kMaxEnhanceRingIcons))
				{
					hud->SetAcquiredEnhanceRingCount(ringCount);
					if (scene.GetPlayer()) scene.GetPlayer()->SetPowerUpLevel(hud->GetAcquiredEnhanceRingCount());
				}
				ImGui::SameLine();
				if (ImGui::Button("0個")) { hud->SetAcquiredEnhanceRingCount(0); if (scene.GetPlayer()) scene.GetPlayer()->SetPowerUpLevel(0); }
				ImGui::SameLine();
				if (ImGui::Button("1個")) { hud->SetAcquiredEnhanceRingCount(1); if (scene.GetPlayer()) scene.GetPlayer()->SetPowerUpLevel(1); }
				ImGui::SameLine();
				if (ImGui::Button("2個")) { hud->SetAcquiredEnhanceRingCount(2); if (scene.GetPlayer()) scene.GetPlayer()->SetPowerUpLevel(2); }

				const Vector4& iconC = hud->GetRingIconColor();
				float iconCol[4] = { iconC.x, iconC.y, iconC.z, iconC.w };
				if (ImGui::ColorEdit4("中身アイコン色", iconCol))
				{
					hud->SetRingIconColor({ iconCol[0], iconCol[1], iconCol[2], iconCol[3] });
				}

				const Vector4& outC = hud->GetRingOutlineColor();
				float outlineCol[4] = { outC.x, outC.y, outC.z, outC.w };
				if (ImGui::ColorEdit4("外枠アウトライン色", outlineCol))
				{
					hud->SetRingOutlineColor({ outlineCol[0], outlineCol[1], outlineCol[2], outlineCol[3] });
				}
			}

			if (ImGui::CollapsingHeader("メモリドロップデバッグ (Memory Drop)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				Vector3 pPos = scene.GetPlayer() ? scene.GetPlayer()->GetWorldPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
				if (ImGui::Button("ボム (BOMB)"))
				{
					if (scene.GetPlayer())
					{
						scene.SpawnMemoryItem({ pPos.x, pPos.y, pPos.z + 18.0f }, MemoryType::BOMB);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("拡散弾 (SPREAD)"))
				{
					if (scene.GetPlayer())
					{
						scene.SpawnMemoryItem({ pPos.x, pPos.y, pPos.z + 18.0f }, MemoryType::SPREAD);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("サイバービット (CYBER_BIT)"))
				{
					if (scene.GetPlayer())
					{
						scene.SpawnMemoryItem({ pPos.x, pPos.y, pPos.z + 18.0f }, MemoryType::CYBER_BIT);
					}
				}

				{
					if (scene.GetPlayer())
					{



					}
				}
				ImGui::Text("現在フィールド上のメモリ数: %zu", scene.GetMemoryItems().size());
				if (ImGui::Button("フィールド上のメモリを消去 (Clear)"))
				{
					scene.ClearMemoryItems();
				}

				float memScale = MemoryItem::GetScaleMultiplier();
				if (ImGui::SliderFloat("メモリスケール倍率", &memScale, 0.5f, 3.0f, "%.2f"))
				{
					MemoryItem::SetScaleMultiplier(memScale);
				}
				const Vector4& bitCol = MemoryItem::GetCustomBitColor();
				float memCol[4] = { bitCol.x, bitCol.y, bitCol.z, bitCol.w };
				if (ImGui::ColorEdit4("ビットメモリ発光色", memCol))
				{
					MemoryItem::SetCustomBitColor({ memCol[0], memCol[1], memCol[2], memCol[3] });
				}
			}

			if (scene.GetPlayer())
			{
				scene.GetPlayer()->DrawImGuiContent();
			}
			else
			{
				ImGui::TextDisabled("プレイヤーが存在しません。");
			}
		}
		// 3. エネミー・装甲列車ボス
		else if (currentNavIndex_ == 2)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "■ エネミー・装甲列車ボス (Enemy & Boss)");
			ImGui::Separator();

			auto* bossBattle = scene.GetBossBattleController();

			// タイムオーバー演出テスト用パネル
			if (ImGui::CollapsingHeader("【演出デバッグ】巨大主砲タイムオーバー砲撃テスト", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "★ 自機拘束 -> 巨大主砲照準 -> チャージ球 -> 極太ビーム直撃爆破を再生");
				
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
				if (ImGui::Button("【演出再生】タイムオーバー巨大砲撃を開始 (何度でも押せます)", ImVec2(-1, 38)))
				{
					scene.TriggerTimeOverCannonSequence(bossBattle->IsTimeOverDebugNonLethal());
				}
				ImGui::PopStyleColor(3);

				bool nonLethal = bossBattle->IsTimeOverDebugNonLethal();
				if (ImGui::Checkbox("非致死テスト（演出完了後に死なず、何度でも再確認可能）", &nonLethal))
				{
					bossBattle->SetTimeOverDebugNonLethal(nonLethal);
				}
				bool loopTest = bossBattle->IsTimeOverDebugLoop();
				if (ImGui::Checkbox("自動ループ再生（演出完了後に最初から自動リピート）", &loopTest))
				{
					bossBattle->SetTimeOverDebugLoop(loopTest);
				}

				if (bossBattle->GetStep() == BossBattleController::Step::TIME_OVER_ATTACK)
				{
					float progress = std::clamp(bossBattle->GetTimeOverTimer() / bossBattle->GetTimeOverDuration(), 0.0f, 1.0f);
					char buf[64];
					snprintf(buf, sizeof(buf), "砲撃演出再生中: %.1f秒 / %.1f秒", bossBattle->GetTimeOverTimer(), bossBattle->GetTimeOverDuration());
					ImGui::ProgressBar(progress, ImVec2(-1, 24), buf);

					if (ImGui::Button("演出を中断して通常戦闘に戻す (Cancel & Resume)", ImVec2(-1, 28)))
					{
						bossBattle->AbortTimeOverTest();
						if (auto* player = scene.GetPlayer())
						{
							player->SetDead(false);
							player->SetHp(player->GetMaxHp());
							player->SetEnableInput(true);
						}
					}
				}
			}
			ImGui::Spacing();
			ImGui::Separator();

			// 電脳処刑フィニッシャー（クリア演出）デバッグUI
			if (auto* finisher = scene.GetBossFinisherSequence())
			{
				finisher->DrawImGuiDebug(scene.GetArmoredTrainBoss(), scene.GetPlayer());
			}

			ImGui::Spacing();
			ImGui::Separator();

			if (ImGui::CollapsingHeader("装甲列車（中ボス）制御 (Armored Train Boss)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				float curProg = scene.GetRailCameraController() ? scene.GetRailCameraController()->GetProgress() : 0.0f;
				ImGui::Text("現在カメラ進行度: %.3f (%.1f%%)", curProg, curProg * 100.0f);

				// ボス出現進行度のスライダー
				float spawnThresh = bossBattle->GetSpawnProgressThreshold();
				if (ImGui::SliderFloat("ボス出現進行度", &spawnThresh, 0.0f, 1.0f, "%.3f"))
				{
					bossBattle->SetSpawnProgressThreshold(spawnThresh);
				}

				// ワンクリックで現在の自機カメラ位置を出撃地点にするボタン
				if (ImGui::Button("現在のカメラ進行度を出撃地点にセット", ImVec2(-1, 28)))
				{
					bossBattle->SetSpawnProgressThreshold(curProg);
				}

				// 設定保存ボタン
				if (ImGui::Button("ボス出現設定を保存 (Save Settings)", ImVec2(-1, 26)))
				{
					bossBattle->SaveSettings(scene.GetArmoredTrainBoss());
				}

				ImGui::Spacing();
				if (auto* boss = scene.GetArmoredTrainBoss())
				{
					float bScale = boss->GetScaleMultiplier();
					if (ImGui::SliderFloat("ボス全体スケール倍率 (Scale)", &bScale, 0.5f, 3.0f, "%.2fx"))
					{
						boss->SetScaleMultiplier(bScale);
					}

					bool isFollow = boss->IsFollowPlayer();
					if (ImGui::Checkbox("プレイヤー連動モード (Follow Player)", &isFollow))
					{
						boss->SetFollowPlayer(isFollow);
					}
					if (isFollow)
					{
						float leadDist = boss->GetDesiredLeadDistance();
						if (ImGui::SliderFloat("並走基準距離(自機前方m)", &leadDist, 50.0f, 200.0f, "%.1f m"))
						{
							boss->SetDesiredLeadDistance(leadDist);
						}
						bool isRandomLead = boss->IsRandomLeadDistanceEnabled();
						if (ImGui::Checkbox("ランダム接近チャンス (短縮して攻撃部位を露出)", &isRandomLead))
						{
							boss->SetRandomLeadDistanceEnabled(isRandomLead);
						}
						float rushSpeed = boss->GetRushSpeedBonus();
						if (ImGui::SliderFloat("接近/追い抜き追加速度", &rushSpeed, 10.0f, 80.0f, "%.1f m/s"))
						{
							boss->SetRushSpeedBonus(rushSpeed);
						}

						bool isGroundSnap = boss->IsGroundSnapEnabled();
						if (ImGui::Checkbox("地面自動吸着 (レイキャスト接地)", &isGroundSnap))
						{
							boss->SetGroundSnapEnabled(isGroundSnap);
						}
						if (isGroundSnap)
						{
							float snapOffset = boss->GetGroundSnapOffset();
							if (ImGui::SliderFloat("接地高さ微調整オフセット(m)", &snapOffset, -5.0f, 5.0f, "%.2f m"))
							{
								boss->SetGroundSnapOffset(snapOffset);
							}
						}

						// リアルタイムチェイス監視情報
						ImGui::Text("チェイス状態: ");
						ImGui::SameLine();
						if (boss->IsOvertaking())
						{
							ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "【追い抜き急加速中】");
						}
						else if (boss->IsShortDistanceOpportunity())
						{
							ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "【至近距離攻撃チャンス中！】");
						}
						else if (boss->IsCloseToPlayer())
						{
							ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "【基準距離で並走巡航中】");
						}
						else
						{
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "【遠方から急接近中】");
						}

						ImGui::Text("目標距離: %.1f m (基準: %.1f m)", boss->GetDynamicLeadDistance(), boss->GetDesiredLeadDistance());
						ImGui::Text("自機との前後距離: %.1f m", boss->GetRelativeForwardDistance());
						ImGui::Text("ボス実速度: %.1f m/s (目標: %.1f m/s)", boss->GetCurrentSpeed(), boss->GetTargetSpeed());
						ImGui::Text("レール進行度: %.1f%%", boss->GetRailProgress() * 100.0f);
					}
				}

				ImGui::Spacing();
				if (scene.GetBossRail() && scene.GetBossRail()->IsValid())
				{
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "● ボス専用レール (BossRail): 検出済み");
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "○ ボス走行レール: メインレール追走 (BossRail未設定)");
				}
				ImGui::Separator();

				const char* stepStr = "未遭遇 (NOT_ACTIVE)";
				if (bossBattle->GetStep() == BossBattleController::Step::WARNING_ALERT) stepStr = "WARNING警告中 (ALERT)";
				else if (bossBattle->GetStep() == BossBattleController::Step::BATTLE) stepStr = "戦闘中 (BATTLE)";
				else if (bossBattle->GetStep() == BossBattleController::Step::TIME_OVER_ATTACK) stepStr = "タイムオーバー即死攻撃中 (TIME_OVER)";
				else if (bossBattle->GetStep() == BossBattleController::Step::DEFEATED_SEQUENCE) stepStr = "撃破演出中 (DEFEATED)";
				else if (bossBattle->GetStep() == BossBattleController::Step::FINISHED) stepStr = "撃破完了 (FINISHED)";
				ImGui::Text("ボス戦フェーズ: %s", stepStr);

				if (bossBattle->GetStep() == BossBattleController::Step::NOT_ACTIVE)
				{
					if (ImGui::Button("WARNING演出付きで出現 (Start Boss Encounter)", ImVec2(-1, 38)))
					{
						scene.StartBossWarningSequence();
					}
					if (ImGui::Button("即座に出現させる (Instant Spawn)", ImVec2(-1, 28)))
					{
						scene.SetBossSpawned(true);
						if (auto* railCam = scene.GetRailCameraController())
						{
							railCam->SetBaseSpeed(50.0f);
						}
						bossBattle->ForceStartBattle();
						if (auto* boss = scene.GetArmoredTrainBoss())
						{
							if (auto* bossRail = scene.GetBossRail(); bossRail && bossRail->IsValid())
							{
								boss->SetRail(bossRail);
								boss->SetRailProgress(0.0f);
							}
							else if (auto* railCam = scene.GetRailCameraController(); railCam && !scene.GetMainRails().empty())
							{
								float bCurProg = railCam->GetProgress();
								float bossProg = std::min(1.0f, bCurProg + 0.15f);
								boss->SetRailProgress(bossProg);
								boss->SetRail(scene.GetMainRails()[0].get());
							}
							boss->SetActive(true);
						}
					}
				}
				else
				{
					if (ImGui::Button("装甲列車ボスを退場・リセット (Reset Boss)", ImVec2(-1, 38)))
					{
						scene.SetBossSpawned(false);
						bossBattle->ResetToNotActive();
						if (auto* boss = scene.GetArmoredTrainBoss())
						{
							boss->ResetChaseState();
							boss->SetActive(false);
						}
						if (auto* finisher = scene.GetBossFinisherSequence())
						{
							finisher->Reset(scene.GetPlayer());
						}
					}

					if (auto* boss = scene.GetArmoredTrainBoss())
					{
						float hpRate = boss->GetTotalHpRate();
						ImGui::ProgressBar(hpRate, ImVec2(-1, 24), "BOSS TOTAL HP");

						ImGui::Separator();
						ImGui::Text("各車両ステータス:");
						const auto& cars = boss->GetCarriages();
						for (size_t c = 0; c < cars.size(); ++c)
						{
							ImGui::Text("[%zu] %s: HP %d / %d %s",
								c, cars[c].displayName.c_str(), cars[c].hp, cars[c].maxHp,
								cars[c].isDestroyed ? "(破壊済)" : "(稼働中)");
						}
					}
				}
			}

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("敵専用エディタ (Enemy Studio)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Button("エネミー専用画面 (Enemy Studio) を開く", ImVec2(-1, 36)))
				{
					EnemyStudio::GetInstance()->SetShowViewport(true);
					EnemyStudio::GetInstance()->SetShowEditor(true);
				}
				ImGui::TextDisabled("※敵モデル・陣形・当たり判定（コライダー）の編集は、中央上の「エネミー画面」および右側の「エネミーエディター」で行えます。");
			}
		}
		// 4. 演出・シェーダー
		else if (currentNavIndex_ == 3)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ 演出・トランジション・シェーダー");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("サイバー仮想空間ビジュアル (Star Fox Virtual Space)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool isVirt = scene.IsVirtualSpace();
				if (ImGui::Checkbox("スターフォックス風 仮想空間シェーダー有効##GameVirtualSpace", &isVirt))
				{
					scene.SetVirtualSpace(isVirt);
					scene.ApplyVirtualSpaceStyle();
				}
				ImGui::TextDisabled("※ONでエメラルドグリッド地面・ホログラム都市・オーロラ宇宙空間になります");
			}
			ImGui::Spacing();

			if (ImGui::CollapsingHeader("画面遷移 (Transition Settings)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (auto sm = scene.GetSceneManager())
				{
					sm->DrawTransitionSettingsUI();
				}
			}

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("ポストプロセス (Post Process)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (auto pp = EngineServices::GetInstance()->GetPostProcess())
				{
					pp->DrawImGuiContent();
				}
			}

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("パーティクルエフェクト (Particles)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Button("エフェクト専用画面 (Effect Studio) を開く", ImVec2(-1, 36)))
				{
					EffectStudio::GetInstance()->SetShowViewport(true);
					EffectStudio::GetInstance()->SetShowEditor(true);
				}
				ImGui::TextDisabled("※エフェクトの編集・保存は、中央上部の「エフェクト画面」および右側の「エフェクトエディター」で行えます。");
			}
		}
		// 5. シーン・照明
		else if (currentNavIndex_ == 4)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ カメラ・照明・配置モデル");
			ImGui::Separator();

			// カメラ設定
			if (ImGui::CollapsingHeader("カメラ詳細 (Active Camera)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (auto* cam = scene.GetMainCamera())
				{
					Vector3 camPosRef = cam->GetTranslate();
					float camPosArr[3] = { camPosRef.x, camPosRef.y, camPosRef.z };
					if (ImGui::DragFloat3("座標 (Translate)", camPosArr, 0.1f))
					{
						cam->SetTranslate(Vector3(camPosArr[0], camPosArr[1], camPosArr[2]));
					}

					Vector3 camRotRef = cam->GetRotation();
					float camRotArr[3] = { camRotRef.x, camRotRef.y, camRotRef.z };
					if (ImGui::DragFloat3("回転 (Rotation)", camRotArr, 0.1f))
					{
						cam->SetRotation(Vector3(camRotArr[0], camRotArr[1], camRotArr[2]));
					}

					float fov = cam->GetFovY();
					if (ImGui::DragFloat("画角 (FOV Y)", &fov, 0.01f, 0.01f, 3.14f))
					{
						cam->SetFovY(fov);
					}

					float aspect = cam->GetAspectRatio();
					if (ImGui::DragFloat("アスペクト比", &aspect, 0.01f, 0.1f, 10.0f))
					{
						cam->SetAspectRatio(aspect);
					}

					float nearC = cam->GetNearClip();
					if (ImGui::DragFloat("近クリップ", &nearC, 0.001f, 0.001f, 100.0f))
					{
						cam->SetNearClip(nearC);
					}

					float farC = cam->GetFarClip();
					if (ImGui::DragFloat("遠クリップ", &farC, 0.1f, 1.0f, 100000.0f))
					{
						cam->SetFarClip(farC);
					}
				}
			}

			// モデルとライトの準備
			std::vector<Object3d*> allModels;
			std::vector<std::string> modelNames;
			int index = 0;
			for (const auto& obj : scene.GetModelInstances())
			{
				if (obj)
				{
					allModels.push_back(obj.get());
					modelNames.push_back("Model " + std::to_string(index));
				}
				index++;
			}
			if (auto* player = scene.GetPlayer())
			{
				if (player->GetObject3d())
				{
					allModels.push_back(player->GetObject3d());
					modelNames.push_back("Player");
				}
				if (player->GetReticle())
				{
					allModels.push_back(player->GetReticle());
					modelNames.push_back("Player Reticle");
				}
			}

			// ライト設定
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("グローバルライト設定 (Global Lights)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (!allModels.empty())
				{
					Object3d* firstObj = allModels[0];
					Model* sampleModel = firstObj ? firstObj->GetModel() : nullptr;
					int lightingMode = sampleModel ? sampleModel->GetSelectLightings() : 0;

					auto usesDirectional = [](int mode) { return mode >= 1 && mode <= 4; };
					auto usesPoint = [](int mode) { return mode == 4; };
					auto usesSpot = [](int mode) { return mode == 5; };

					// 平行光源
					if (usesDirectional(lightingMode))
					{
						ImGui::Text("平行光源 (Directional Light)");
						static bool dirInit = false;
						static Vector4 dirColor = { 1.0f, 1.0f, 1.0f, 1.0f };
						static Vector3 dirDirection = { -1.0f, -0.5f, 0.5f };
						static float dirIntensity = 1.0f;
						static bool dirEnabledGlobal = true;
						static float dirPrevIntensityGlobal = 1.0f;

						if (!dirInit)
						{
							dirColor = firstObj->GetDirectionalLightColor();
							dirDirection = firstObj->GetDirectionalLightDirection();
							dirIntensity = firstObj->GetDirectionalLightIntensity();
							dirPrevIntensityGlobal = dirIntensity;
							dirInit = true;
						}

						float dcArr[4] = { dirColor.x, dirColor.y, dirColor.z, dirColor.w };
						if (ImGui::ColorEdit4("色##Dir", dcArr))
						{
							dirColor = Vector4(dcArr[0], dcArr[1], dcArr[2], dcArr[3]);
							for (auto& m : allModels) m->SetDirectionalLightColor(dirColor);
						}

						float ddArr[3] = { dirDirection.x, dirDirection.y, dirDirection.z };
						if (ImGui::DragFloat3("方向##Dir", ddArr, 0.01f, -10.0f, 10.0f))
						{
							dirDirection = Vector3(ddArr[0], ddArr[1], ddArr[2]);
							for (auto& m : allModels) m->SetDirectionalLightDirection(dirDirection);
						}

						if (ImGui::DragFloat("強度##Dir", &dirIntensity, 0.01f, 0.0f, 100.0f))
						{
							if (dirEnabledGlobal)
							{
								for (auto& m : allModels) m->SetDirectionalLightIntensity(dirIntensity);
								dirPrevIntensityGlobal = dirIntensity;
							}
							else
							{
								dirPrevIntensityGlobal = dirIntensity;
							}
						}

						if (ImGui::Checkbox("平行光源を有効化##Dir", &dirEnabledGlobal))
						{
							if (!dirEnabledGlobal)
							{
								for (auto& m : allModels) m->SetDirectionalLightIntensity(0.0f);
							}
							else
							{
								for (auto& m : allModels) m->SetDirectionalLightIntensity(dirPrevIntensityGlobal);
							}
						}
					}

					// 点光源
					if (usesPoint(lightingMode))
					{
						ImGui::Separator();
						ImGui::Text("点光源 (Point Light)");
						static bool pointInit = false;
						static Vector4 pointColor = { 1.0f, 1.0f, 1.0f, 1.0f };
						static Vector3 pointPosition = { 0.0f, 1.0f, -8.0f };
						static float pointIntensity = 1.0f;
						static float prevPointIntensity = 1.0f;
						static float pointRadius = 15.0f;
						static float pointRange = 1.0f;
						static bool pointLightEnabled = true;

						if (!pointInit && firstObj)
						{
							pointColor = firstObj->GetPointLightColor();
							pointPosition = firstObj->GetPointLightPosition();
							pointIntensity = firstObj->GetPointLightIntensity();
							prevPointIntensity = pointIntensity;
							pointInit = true;
						}

						if (ImGui::Checkbox("点光源を有効化##Point", &pointLightEnabled))
						{
							if (!pointLightEnabled)
								for (auto& m : allModels) m->SetPointLightIntensity(0.0f);
							else
								for (auto& m : allModels) m->SetPointLightIntensity(prevPointIntensity);
						}

						ImGui::BeginDisabled(!pointLightEnabled);
						float pcArr[4] = { pointColor.x, pointColor.y, pointColor.z, pointColor.w };
						if (ImGui::ColorEdit4("色##Point", pcArr))
						{
							pointColor = Vector4(pcArr[0], pcArr[1], pcArr[2], pcArr[3]);
							for (auto& m : allModels) m->SetPointLightColor(pointColor);
						}
						float ppArr[3] = { pointPosition.x, pointPosition.y, pointPosition.z };
						if (ImGui::DragFloat3("位置##Point", ppArr, 0.05f, -100.0f, 100.0f))
						{
							pointPosition = Vector3(ppArr[0], ppArr[1], ppArr[2]);
							for (auto& m : allModels) m->SetPointLightPosition(pointPosition);
						}
						if (ImGui::DragFloat("強度##Point", &pointIntensity, 0.01f, 0.0f, 100.0f))
						{
							if (pointLightEnabled)
							{
								for (auto& m : allModels) m->SetPointLightIntensity(pointIntensity);
								prevPointIntensity = pointIntensity;
							}
							else
							{
								prevPointIntensity = pointIntensity;
							}
						}
						if (ImGui::DragFloat("半径##Point", &pointRadius, 0.01f, 0.1f, 100.0f))
						{
							for (auto& m : allModels) m->SetPointLightRadius(pointRadius);
						}
						if (ImGui::DragFloat("減衰範囲##Point", &pointRange, 0.01f, 0.1f, 50.0f))
						{
							for (auto& m : allModels) m->SetPointLightDecry(pointRange);
						}
						ImGui::EndDisabled();
					}

					// スポットライト
					if (usesSpot(lightingMode))
					{
						ImGui::Separator();
						ImGui::Text("スポットライト (Spot Light)");
						static bool spotInit = false;
						static Vector4 spotColor = { 1.0f, 1.0f, 1.0f, 1.0f };
						static Vector3 spotPosition = { 0.0f, 5.0f, 0.0f };
						static Vector3 spotDirection = { 0.0f, -1.0f, 0.0f };
						static float spotIntensity = 1.0f;
						static float prevSpotIntensity = 1.0f;
						static float spotDistance = 10.0f;
						static float spotDecay = 1.0f;
						static float spotAngleDeg = 45.0f;
						static bool spotLightEnabled = true;

						if (!spotInit && firstObj)
						{
							spotColor = firstObj->GetSpotLightColor();
							spotPosition = firstObj->GetSpotLightPosition();
							spotDirection = firstObj->GetSpotLightDirection();
							spotIntensity = firstObj->GetSpotLightIntensity();
							prevSpotIntensity = spotIntensity;
							spotDistance = firstObj->GetSpotLightDistance();
							spotDecay = firstObj->GetSpotLightDecay();
							spotAngleDeg = firstObj->GetSpotLightAngleDeg();
							spotInit = true;
						}

						if (ImGui::Checkbox("スポットライトを有効化##Spot", &spotLightEnabled))
						{
							if (!spotLightEnabled)
								for (auto& m : allModels) m->SetSpotLightIntensity(0.0f);
							else
								for (auto& m : allModels) m->SetSpotLightIntensity(prevSpotIntensity);
						}

						ImGui::BeginDisabled(!spotLightEnabled);
						float scArr[4] = { spotColor.x, spotColor.y, spotColor.z, spotColor.w };
						if (ImGui::ColorEdit4("色##Spot", scArr))
						{
							spotColor = Vector4(scArr[0], scArr[1], scArr[2], spotColor.w);
							for (auto& m : allModels) m->SetSpotLightColor(spotColor);
						}
						float spArr[3] = { spotPosition.x, spotPosition.y, spotPosition.z };
						if (ImGui::DragFloat3("位置##Spot", spArr, 0.05f, -100.0f, 100.0f))
						{
							spotPosition = Vector3(spArr[0], spArr[1], spArr[2]);
							for (auto& m : allModels) m->SetSpotLightPosition(spotPosition);
						}
						float sdArr[3] = { spotDirection.x, spotDirection.y, spotDirection.z };
						if (ImGui::DragFloat3("方向##Spot", sdArr, 0.01f, -10.0f, 10.0f))
						{
							spotDirection = Vector3(sdArr[0], sdArr[1], sdArr[2]);
							for (auto& m : allModels) m->SetSpotLightDirection(spotDirection);
						}
						if (ImGui::DragFloat("強度##Spot", &spotIntensity, 0.01f, 0.0f, 100.0f))
						{
							if (spotLightEnabled)
							{
								for (auto& m : allModels) m->SetSpotLightIntensity(spotIntensity);
								prevSpotIntensity = spotIntensity;
							}
							else
							{
								prevSpotIntensity = spotIntensity;
							}
						}
						if (ImGui::DragFloat("距離##Spot", &spotDistance, 0.1f, 0.0f, 10000.0f))
						{
							for (auto& m : allModels) m->SetSpotLightDistance(spotDistance);
						}
						if (ImGui::DragFloat("減衰率##Spot", &spotDecay, 0.01f, 0.0f, 10.0f))
						{
							for (auto& m : allModels) m->SetSpotLightDecay(spotDecay);
						}
						if (ImGui::SliderFloat("照射角度##Spot", &spotAngleDeg, 1.0f, 90.0f))
						{
							for (auto& m : allModels) m->SetSpotLightAngleDeg(spotAngleDeg);
						}
						ImGui::EndDisabled();
					}

					if (!usesDirectional(lightingMode) && !usesPoint(lightingMode) && !usesSpot(lightingMode))
					{
						ImGui::TextWrapped("現在のライティングモードでは、編集可能なライトがありません。");
					}
				}
			}

			// モデル一覧・トランスフォーム調整
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("3Dモデル一覧・調整 (Model Inspector)"))
			{
				if (!allModels.empty())
				{
					static int currentModelIndex = 0;
					if (currentModelIndex >= (int)allModels.size()) currentModelIndex = 0;

					std::vector<const char*> namePtrs;
					for (const auto& name : modelNames)
					{
						namePtrs.push_back(name.c_str());
					}

					ImGui::Combo("対象モデル", &currentModelIndex, namePtrs.data(), (int)namePtrs.size());

					Object3d* obj = allModels[currentModelIndex];
					if (obj)
					{
						Vector3 t = obj->GetTranslate();
						float tArr[3] = { t.x, t.y, t.z };
						if (ImGui::DragFloat3("座標 (Translate)", tArr, 0.05f))
						{
							obj->SetTranslate(Vector3(tArr[0], tArr[1], tArr[2]));
						}

						Vector3 r = obj->GetRotation();
						float rArr[3] = { r.x, r.y, r.z };
						if (ImGui::DragFloat3("回転 (Rotation)", rArr, 0.5f))
						{
							obj->SetRotation(Vector3(rArr[0], rArr[1], rArr[2]));
						}

						Vector3 s = obj->GetScale();
						float sArr[3] = { s.x, s.y, s.z };
						if (ImGui::DragFloat3("スケール (Scale)", sArr, 0.01f, 0.001f, 100000.0f))
						{
							obj->SetScale(Vector3(sArr[0], sArr[1], sArr[2]));
						}

						if (obj->GetModel())
						{
							float envCoeff = obj->GetModel()->GetEnvironmentCoefficient();
							if (ImGui::DragFloat("環境反射係数", &envCoeff, 0.01f, 0.0f, 1.0f))
							{
								obj->SetEnvironmentCoefficient(envCoeff);
							}
						}
					}

					ImGui::Separator();
					Model* sampleModel = allModels[0]->GetModel();
					if (sampleModel)
					{
						int currentSelect = sampleModel->GetSelectLightings();
						const char* lightingNames[] = {
							"0: テクスチャのみ (TextureOnly)",
							"1: 平行光源・ディフューズ (Directional Diffuse)",
							"2: 平行光源・ソフト (Directional Soft)",
							"3: 平行光源・スペキュラ (Dir Diffuse+Specular)",
							"4: 平行光源 + 点光源 (Dir + Point)",
							"5: スポットライト (Spot)"
						};
						if (ImGui::Combo("ライティングモード (一括変更)", &currentSelect, lightingNames, IM_ARRAYSIZE(lightingNames)))
						{
							for (auto& mObj : allModels)
							{
								Model* m = mObj->GetModel();
								if (m) m->SetSelectLightings(currentSelect);
							}
						}
					}
				}
			}
		}
		// 6. UIテキスト配置
		else if (currentNavIndex_ == 5)
		{
			UITextManager::GetInstance()->DrawImGuiEditor();
		}
	}
	ImGui::End();

	// -------------------------------------------------------------
	// 3. [下ドック用] タイムライン (Timeline Editor)
	// -------------------------------------------------------------
	if (ImGui::Begin("タイムライン"))
	{
		float currentProgress = scene.GetRailCameraController() ? scene.GetRailCameraController()->GetProgress() : 0.0f;
		float totalRailLength = (!scene.GetMainRails().empty() && scene.GetMainRails()[0]->IsValid()) ? scene.GetMainRails()[0]->GetTotalLength() : 1000.0f;
		// 想定総時間（秒）: 基準速度での概算時間
		float estimatedTotalTime = (totalRailLength > 0.0f) ? (totalRailLength / 40.0f) : 60.0f;
		float currentEstimatedTime = currentProgress * estimatedTotalTime;

		// 制御バー: 再生コントロール
		ImGui::BeginGroup();
		{
			// 先頭へ
			if (ImGui::Button("|< 0%##TL", ImVec2(55, 26)))
			{
				if (auto* railCam = scene.GetRailCameraController()) railCam->SetProgress(0.0f);
			}
			ImGui::SameLine();
			// -5%
			if (ImGui::Button("<< -5%##TL", ImVec2(65, 26)))
			{
				float newP = (std::max)(0.0f, currentProgress - 0.05f);
				if (auto* railCam = scene.GetRailCameraController()) railCam->SetProgress(newP);
			}
			ImGui::SameLine();
			// 再生 / 一時停止
			if (scene.IsPlaying())
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
				if (ImGui::Button("一時停止 (Pause)##TL", ImVec2(125, 26)))
				{
					scene.SetPlaying(false);
					services->SetGamePlaying(false);
				}
				ImGui::PopStyleColor(2);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.65f, 0.35f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.78f, 0.42f, 1.0f));
				if (ImGui::Button("再生 (Play)##TL", ImVec2(125, 26)))
				{
					scene.SetPlaying(true);
					services->SetGamePlaying(true);
				}
				ImGui::PopStyleColor(2);
			}
			ImGui::SameLine();
			// +5%
			if (ImGui::Button("+5% >>##TL", ImVec2(65, 26)))
			{
				float newP = (std::min)(1.0f, currentProgress + 0.05f);
				if (auto* railCam = scene.GetRailCameraController()) railCam->SetProgress(newP);
			}
			ImGui::SameLine();
			// 末尾へ
			if (ImGui::Button("100% >|##TL", ImVec2(65, 26)))
			{
				if (auto* railCam = scene.GetRailCameraController()) railCam->SetProgress(1.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("リセット##TL", ImVec2(65, 26)))
			{
				scene.SetPlaying(false);
				services->SetGamePlaying(false);
				scene.SetGamePhase(GamePlayScene::GamePhase::PLAYING);
				if (auto* finisher = scene.GetBossFinisherSequence()) finisher->Reset(scene.GetPlayer());
				if (auto* railCam = scene.GetRailCameraController()) railCam->Reset();
				scene.ReloadEnemiesOnly();
			}
			ImGui::SameLine();
			if (ImGui::Button("敵リスポーン##TL", ImVec2(95, 26)))
			{
				scene.ReloadEnemiesOnly();
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.25f, 1.0f));
			if (ImGui::Button("クリア演出##TL", ImVec2(95, 26)))
			{
				scene.SetPlaying(true);
				services->SetGamePlaying(true);
				if (auto* player = scene.GetPlayer()) player->SetDead(false);
				scene.SetGamePhase(GamePlayScene::GamePhase::PLAYING);
				scene.StartClearSequence();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::TextDisabled("|");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(110.0f);
			float baseSpeedTL = scene.GetBaseGameSpeed();
			if (ImGui::SliderFloat("速度##TL", &baseSpeedTL, 0.1f, 3.0f, "%.2fx")) scene.SetBaseGameSpeed(baseSpeedTL);
			ImGui::SameLine();
			if (ImGui::SmallButton("1.0x##TL")) scene.SetBaseGameSpeed(1.0f);
			ImGui::SameLine();
			if (ImGui::SmallButton("2.0x##TL")) scene.SetBaseGameSpeed(2.0f);
		}
		ImGui::EndGroup();

		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "  進行度: %5.1f%% (%.1fs / %.1fs)", currentProgress * 100.0f, currentEstimatedTime, estimatedTotalTime);

		ImGui::Spacing();

		// タイムライン描画 (DrawList によるカスタムシーケンサーバー)
		ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 34.0f);
		if (canvasSize.x < 100.0f) canvasSize.x = 100.0f;
		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// 背景トラック
		drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(32, 34, 40, 255), 4.0f);
		drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(60, 65, 75, 255), 4.0f);

		// 狭小ゾーン (Narrow Zones) の帯を描画
		if (!scene.GetMainRails().empty() && scene.GetMainRails()[0]->IsValid())
		{
			const auto& zones = scene.GetMainRails()[0]->GetNarrowZones();
			for (const auto& zone : zones)
			{
				float x0 = canvasPos.x + zone.startT * canvasSize.x;
				float x1 = canvasPos.x + zone.endT * canvasSize.x;
				drawList->AddRectFilled(ImVec2(x0, canvasPos.y + 2.0f), ImVec2(x1, canvasPos.y + canvasSize.y - 2.0f), IM_COL32(230, 140, 40, 100), 2.0f);
				drawList->AddRect(ImVec2(x0, canvasPos.y + 2.0f), ImVec2(x1, canvasPos.y + canvasSize.y - 2.0f), IM_COL32(250, 170, 60, 200), 2.0f);
			}
		}

		// エネミーマーカーの収集と描画
		struct EnemyMarkerItem {
			float progress;
			std::string type;
			bool isDead;
			bool isActive;
		};
		std::vector<EnemyMarkerItem> enemyMarkers;

		const Rail* mainRail = (!scene.GetMainRails().empty() && scene.GetMainRails()[0]->IsValid()) ? scene.GetMainRails()[0].get() : nullptr;

		for (const auto& enemy : scene.GetEnemies())
		{
			if (!enemy) continue;
			float sp = enemy->GetRailProgress();
			// 未設定(0.0f以下)の場合は、敵の配置座標またはパス始点から最寄りレール進行度を自動逆算
			if ((sp <= 0.0f || sp > 1.0f) && mainRail)
			{
				Vector3 basePos = enemy->GetSpawnPos();
				if (enemy->GetMovePath() && enemy->GetMovePath()->IsValid())
				{
					basePos = enemy->GetMovePath()->GetPosition(0.0f);
				}
				sp = mainRail->GetClosestProgress(basePos);
				// タイムライン用キャッシュに反映（敵のスポーン設定は変更しない）
				const_cast<Enemy*>(enemy.get())->SetRailProgress(sp);
			}

			if (sp >= 0.0f && sp <= 1.0f)
			{
				enemyMarkers.push_back({ sp, enemy->GetTypeName(), enemy->IsDead(), enemy->IsActive() });

				float ex = canvasPos.x + sp * canvasSize.x;
				// 状態に応じた色分け（待機中: 紫, 出現中: 黄金, 撃破済: 暗いグレー）
				ImU32 col = IM_COL32(185, 95, 255, 230);
				if (enemy->IsDead()) col = IM_COL32(110, 110, 120, 140);
				else if (enemy->IsActive()) col = IM_COL32(255, 215, 0, 255);

				drawList->AddLine(ImVec2(ex, canvasPos.y + 2.0f), ImVec2(ex, canvasPos.y + canvasSize.y - 2.0f), col, 1.5f);
				drawList->AddTriangleFilled(ImVec2(ex - 3.5f, canvasPos.y + 2.0f), ImVec2(ex + 3.5f, canvasPos.y + 2.0f), ImVec2(ex, canvasPos.y + 8.0f), col);
			}
		}

		// 目盛り (10% 刻み)
		for (int i = 1; i < 10; ++i)
		{
			float tx = canvasPos.x + (i * 0.1f) * canvasSize.x;
			drawList->AddLine(ImVec2(tx, canvasPos.y + canvasSize.y - 6.0f), ImVec2(tx, canvasPos.y + canvasSize.y - 1.0f), IM_COL32(100, 105, 120, 180), 1.0f);
		}

		// 現在の再生ヘッド（Playhead）
		float playheadX = canvasPos.x + currentProgress * canvasSize.x;
		drawList->AddLine(ImVec2(playheadX, canvasPos.y), ImVec2(playheadX, canvasPos.y + canvasSize.y), IM_COL32(255, 60, 60, 255), 2.5f);
		drawList->AddTriangleFilled(ImVec2(playheadX - 5.0f, canvasPos.y), ImVec2(playheadX + 5.0f, canvasPos.y), ImVec2(playheadX, canvasPos.y + 8.0f), IM_COL32(255, 80, 80, 255));

		// 見えないボタンを被せてマウスクリック＆ドラッグによるスクラブ操作を実現
		ImGui::InvisibleButton("##TimelineTrack", canvasSize);
		if (ImGui::IsItemActive() || ImGui::IsItemClicked())
		{
			ImVec2 mousePos = ImGui::GetMousePos();
			float newT = (mousePos.x - canvasPos.x) / canvasSize.x;
			newT = (std::max)(0.0f, (std::min)(1.0f, newT));
			if (auto* railCam = scene.GetRailCameraController())
			{
				railCam->SetProgress(newT);
			}
		}

		// ホバー時にツールチップ表示
		if (ImGui::IsItemHovered())
		{
			ImVec2 mousePos = ImGui::GetMousePos();
			float hoverT = (mousePos.x - canvasPos.x) / canvasSize.x;
			hoverT = (std::max)(0.0f, (std::min)(1.0f, hoverT));

			std::string extraInfo = "";
			float minDiff = 0.035f; // 3.5%の範囲内の敵を検出
			int nearbyCount = 0;
			std::string nearestType = "";
			for (const auto& m : enemyMarkers)
			{
				if (std::abs(m.progress - hoverT) < minDiff)
				{
					nearbyCount++;
					if (nearestType.empty()) nearestType = m.type;
				}
			}
			if (nearbyCount > 0)
			{
				extraInfo = "\n[エネミー配置: " + nearestType + (nearbyCount > 1 ? (" ほか計" + std::to_string(nearbyCount) + "体") : "") + "]";
			}

			ImGui::SetTooltip("位置: %.1f%% (%.1fs)%s\nクリックでジャンプ", hoverT * 100.0f, hoverT * estimatedTotalTime, extraInfo.c_str());
		}

		// イベントマーカー凡例・クイックジャンプ
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "■ 狭小ゾーン (Orange)");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.75f, 0.45f, 1.0f, 1.0f), "  ■ エネミー配置 (Purple:待機 / Yellow:出現中 / Gray:撃破)");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "  | 再生位置 (Red)");
		ImGui::SameLine();
		ImGui::TextDisabled(" [敵総数: %zu体]", enemyMarkers.size());

		if (!scene.GetMainRails().empty() && scene.GetMainRails()[0]->IsValid())
		{
			const auto& zones = scene.GetMainRails()[0]->GetNarrowZones();
			ImGui::TextDisabled("ジャンプ: ");

			if (!zones.empty())
			{
				for (size_t zIdx = 0; zIdx < zones.size(); ++zIdx)
				{
					ImGui::SameLine();
					char btnLabel[32];
					snprintf(btnLabel, sizeof(btnLabel), "ゾーン%zu (%.0f%%)##Jump", zIdx + 1, zones[zIdx].startT * 100.0f);
					if (ImGui::SmallButton(btnLabel))
					{
						if (auto* railCam = scene.GetRailCameraController()) railCam->SetProgress(zones[zIdx].startT);
					}
				}
			}

			// 敵の出現ポイントへのジャンプ（代表的なポイントを重複排除して最大5箇所）
			if (!enemyMarkers.empty())
			{
				std::vector<float> enemyJumpPoints;
				for (const auto& em : enemyMarkers)
				{
					bool isTooClose = false;
					for (float jp : enemyJumpPoints)
					{
						if (std::abs(jp - em.progress) < 0.05f) { isTooClose = true; break; }
					}
					if (!isTooClose)
					{
						enemyJumpPoints.push_back(em.progress);
						if (enemyJumpPoints.size() >= 5) break;
					}
				}
				std::sort(enemyJumpPoints.begin(), enemyJumpPoints.end());

				for (size_t eIdx = 0; eIdx < enemyJumpPoints.size(); ++eIdx)
				{
					ImGui::SameLine();
					char btnLabel[32];
					snprintf(btnLabel, sizeof(btnLabel), "敵群%zu (%.0f%%)##EJump", eIdx + 1, enemyJumpPoints[eIdx] * 100.0f);
					if (ImGui::SmallButton(btnLabel))
					{
						if (auto* railCam = scene.GetRailCameraController()) railCam->SetProgress(enemyJumpPoints[eIdx]);
					}
				}
			}
		}
	}
	ImGui::End();

}
#else
void GamePlayEditor::Draw(GamePlayScene&)
{
}
#endif // ENABLE_EDITOR