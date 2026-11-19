#define NOMINMAX
#include "GamePlayScene.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Core/Utility/Log/Logger.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleManager.h"
#include "KHEngine/Graphics/Billboard/Billboard.h"
#include "KHEngine/Debug/Imgui/ImGuiManager.h"
#include "KHEngine/Graphics/3d/Particle/ParticleRenderer.h"
#include "KHEngine/Sound/Core/SoundManager.h"
#include <algorithm>
#include <random>
#include <memory>
#include <cmath>
#include <numbers>
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include "externals/imgui/imgui.h"
#ifdef ENABLE_EDITOR
#include "KHEngine/Debug/Editor/EffectStudio.h"
#include "KHEngine/Debug/Editor/EnemyStudio.h"
#include "KHEngine/Debug/Editor/EditorSystem.h"
#endif
#include "Game/Actor/Enemy/EnemyPresetManager.h"
#include <filesystem>
#include "KHEngine/Math/CollisionMath.h"
#include "KHEngine/Scene/SceneManager.h"
#include "KHEngine/UI/UITextManager.h"
#include <chrono>
#include <fstream>
#include "externals/nlohmann/json.hpp"
#include "Game/System/TerrainCollisionDebugger.h"

static void CreateObjectFromNode(const LevelObjectData& node, const Object3d* parentObj, std::vector<std::unique_ptr<Object3d>>& instances, std::vector<std::unique_ptr<Rail>>& outRails, std::unique_ptr<Rail>& outBossRail, float& outBossSpawnProgress, Object3dCommon* common, uint32_t skyboxTexIndex, std::list<std::unique_ptr<Enemy>>& enemies, std::list<std::unique_ptr<Obstacle>>& obstacles, std::list<std::unique_ptr<EnhanceRing>>& enhanceRings, std::vector<Enemy*> parentEnemies = {})
{
	const Object3d* currentObj = parentObj;

	if (node.type == "CURVE")
	{
		if (!parentEnemies.empty())
		{
			for (Enemy* e : parentEnemies)
			{
				auto rail = std::make_unique<Rail>();
				rail->Initialize(node.curvePoints);
				e->SetMovePath(std::move(rail));
			}
		}
		else if (node.name.find("BossRail") != std::string::npos || node.name.find("boss") != std::string::npos || node.name.find("Boss") != std::string::npos)
		{
			// ボス戦専用レール (BossRail)
			auto rail = std::make_unique<Rail>();
			rail->Initialize(node.curvePoints);
			outBossRail = std::move(rail);
			if (node.spawnProgress > 0.0f)
			{
				outBossSpawnProgress = node.spawnProgress;
			}
		}
		else
		{
			// showPlayerRangeが有効（メインレール）の場合のみプレイヤーのカメラレールに追加
			if (node.showPlayerRange)
			{
				auto rail = std::make_unique<Rail>();
				rail->Initialize(node.curvePoints);
				outRails.push_back(std::move(rail));
			}
		}
	}

	std::vector<Enemy*> currentEnemies = parentEnemies;

	bool isObstacle = (node.fileName.find("Obstacle") != std::string::npos) || (node.fileName.find("Invisible") != std::string::npos) || (node.fileName.find("ColliderOnly") != std::string::npos);
	bool isRing = (node.fileName.find("Ring") != std::string::npos) || (node.name.find("Ring") != std::string::npos) || (node.name.find("強化リング") != std::string::npos) || (node.fileName.find("Heal") != std::string::npos) || (node.name.find("Heal") != std::string::npos) || (node.name.find("回復") != std::string::npos);
	bool isEnemy = node.isEnemy ||
		(node.fileName.find("Fighter") != std::string::npos) ||
		(node.fileName.find("Asteroid") != std::string::npos) ||
		(node.fileName.find("Enemy") != std::string::npos) ||
		(node.fileName.find("enemy") != std::string::npos) ||
		(node.name.find("Enemy") != std::string::npos) ||
		(node.name.find("enemy") != std::string::npos);

	if (isRing)
	{
		auto ring = std::make_unique<EnhanceRing>();
        
        RingType type = RingType::POWER_UP;
        if (node.fileName.find("Heal") != std::string::npos || node.name.find("Heal") != std::string::npos || node.name.find("回復") != std::string::npos) {
            type = RingType::HEAL;
        }

		ring->Initialize(common, node.translation, node.scale, node.rotation, node.fileName, type, skyboxTexIndex, node.collider);
		enhanceRings.push_back(std::move(ring));
	}
	else if (isEnemy)
	{
		currentEnemies.clear();
		auto presetMgr = EnemyPresetManager::GetInstance();
		std::string enemyType = node.enemyType;
		if (enemyType.empty())
		{
			enemyType = "RUSHER";
		}
		const auto* preset = presetMgr->GetPreset(enemyType);

		std::string formationType = node.formationType;
		int count = node.spawnCount;
		float spacing = node.formationSpacing;

		// Blender側で陣形指定がない場合、ゲーム側プリセットの陣形設定を自動適用
		if (preset)
		{
			if (formationType == "NONE" || formationType.empty() || count <= 1)
			{
				formationType = preset->formationType;
				count = preset->formationCount;
				spacing = preset->formationSpacing;
			}
		}

		int spawnCount = (std::max)(1, count);
		auto offsets = EnemyPresetManager::CalculateFormationOffsets(formationType, spawnCount, spacing);

		for (int i = 0; i < spawnCount; ++i)
		{
			auto enemy = std::make_unique<Enemy>();

			LevelObjectData spawnNode = node;
			spawnNode.enemyType = enemyType;
			if (i < static_cast<int>(offsets.size()))
			{
				spawnNode.translation.x += offsets[i].x;
				spawnNode.translation.y += offsets[i].y;
				spawnNode.translation.z += offsets[i].z;
			}

			enemy->Initialize(common, spawnNode, skyboxTexIndex);
			enemy->SetSpawnProgress(node.spawnProgress);
			enemy->SetSpawnDelay(static_cast<float>(i * node.spawnInterval));

			if (!node.texturePath.empty())
			{
				enemy->SetTexturePath(node.texturePath);
			}

			currentEnemies.push_back(enemy.get());
			enemies.push_back(std::move(enemy));
		}
	}
	else if (isObstacle)
	{
		auto obstacle = std::make_unique<Obstacle>();

		Vector3 rotRad;
		rotRad.x = node.rotation.x * (std::numbers::pi_v<float> / 180.0f);
		rotRad.y = node.rotation.y * (std::numbers::pi_v<float> / 180.0f);
		rotRad.z = node.rotation.z * (std::numbers::pi_v<float> / 180.0f);
		obstacle->Initialize(common, node.translation, node.scale, rotRad, node.fileName, skyboxTexIndex, node.collider, node.isDestructible);
		// オブジェクトのスポーン進行度を設定
		obstacle->SetSpawnProgress(node.spawnProgress);
		if (!node.texturePath.empty())
		{
			obstacle->SetTexturePath(node.texturePath);
		}
		obstacles.push_back(std::move(obstacle));
	}
	else if (node.type == "MESH")
	{
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(common);

		std::string modelName = node.fileName;
		if (modelName.empty())
		{
			// Blenderの複製連番サフィックス（.001, .002等）がある場合はベース名を抽出
			std::string baseName = node.name;
			size_t dotPos = baseName.rfind('.');
			if (dotPos != std::string::npos && dotPos + 1 < baseName.size())
			{
				bool isNumber = true;
				for (size_t i = dotPos + 1; i < baseName.size(); ++i)
				{
					if (!std::isdigit(static_cast<unsigned char>(baseName[i])))
					{
						isNumber = false;
						break;
					}
				}
				if (isNumber)
				{
					baseName = baseName.substr(0, dotPos);
				}
			}
			std::string baseObjName = baseName;
			if (baseObjName.find(".obj") == std::string::npos) baseObjName += ".obj";

			// ベース名のモデル（例: AITerrain_Background_Mountains.obj, pillar.obj等）が存在すればそれを優先して共通化
			if (!ResourceLocator::Resolve(baseObjName, ResourceLocator::AssetType::Model3D).empty())
			{
				modelName = baseObjName;
			}
			else
			{
				std::string directName = node.name;
				if (directName.find(".obj") == std::string::npos) directName += ".obj";
				modelName = directName;
			}
		}
		else if (modelName.find(".obj") == std::string::npos)
		{
			modelName += ".obj";
		}

		ModelManager::GetInstance()->LoadModel(modelName);
		if (ModelManager::GetInstance()->FindModel(modelName) != nullptr)
		{
			obj->SetModel(modelName);
		}

		obj->SetTranslate(node.translation);


		Vector3 rotRad;
		rotRad.x = node.rotation.x * (std::numbers::pi_v<float> / 180.0f);
		rotRad.y = node.rotation.y * (std::numbers::pi_v<float> / 180.0f);
		rotRad.z = node.rotation.z * (std::numbers::pi_v<float> / 180.0f);
		obj->SetRotation(rotRad);

		obj->SetScale(node.scale);
		obj->SetEnvironmentTextureIndex(skyboxTexIndex);
		obj->SetName(node.name);

		if (node.name.find("Terrain_Background_Mountains") != std::string::npos ||
			modelName.find("Terrain_Background_Mountains") != std::string::npos ||
			node.name.find("Background_Mountains") != std::string::npos)
		{
			obj->SetBackgroundMountain(true);
		}

		// 背景専用オブジェクト（山岳遠景など）はコリジョン判定を無効化
		if (node.name.find("Background") != std::string::npos || modelName.find("Background") != std::string::npos)
		{
			obj->SetCollisionEnabled(false);
		}

		if (parentObj)
		{
			obj->SetParent(parentObj);
		}

		currentObj = obj.get();
		instances.push_back(std::move(obj));
	}


	for (const auto& child : node.children)
	{
		CreateObjectFromNode(child, currentObj, instances, outRails, outBossRail, outBossSpawnProgress, common, skyboxTexIndex, enemies, obstacles, enhanceRings, currentEnemies);
	}
}

static void LoadEnemiesOnlyFromNode(const LevelObjectData& node, Object3dCommon* common, uint32_t skyboxTexIndex, std::list<std::unique_ptr<Enemy>>& enemies, std::list<std::unique_ptr<Obstacle>>& obstacles, std::list<std::unique_ptr<EnhanceRing>>& enhanceRings, std::vector<Enemy*> parentEnemies = {})
{
	if (node.type == "CURVE" && !parentEnemies.empty())
	{
		for (Enemy* e : parentEnemies)
		{
			auto rail = std::make_unique<Rail>();
			rail->Initialize(node.curvePoints);
			e->SetMovePath(std::move(rail));
		}
	}

	std::vector<Enemy*> currentEnemies = parentEnemies;

	bool isObstacle = (node.fileName.find("Obstacle") != std::string::npos) || (node.fileName.find("Invisible") != std::string::npos) || (node.fileName.find("ColliderOnly") != std::string::npos);
	bool isRing = (node.fileName.find("Ring") != std::string::npos) || (node.name.find("Ring") != std::string::npos) || (node.name.find("強化リング") != std::string::npos) || (node.fileName.find("Heal") != std::string::npos) || (node.name.find("Heal") != std::string::npos) || (node.name.find("回復") != std::string::npos);
	bool isEnemy = node.isEnemy ||
		(node.fileName.find("Fighter") != std::string::npos) ||
		(node.fileName.find("Asteroid") != std::string::npos) ||
		(node.fileName.find("Enemy") != std::string::npos) ||
		(node.fileName.find("enemy") != std::string::npos) ||
		(node.name.find("Enemy") != std::string::npos) ||
		(node.name.find("enemy") != std::string::npos);

	if (isRing)
	{
		auto ring = std::make_unique<EnhanceRing>();
        
        RingType type = RingType::POWER_UP;
        if (node.fileName.find("Heal") != std::string::npos || node.name.find("Heal") != std::string::npos || node.name.find("回復") != std::string::npos) {
            type = RingType::HEAL;
        }

		ring->Initialize(common, node.translation, node.scale, node.rotation, node.fileName, type, skyboxTexIndex, node.collider);
		enhanceRings.push_back(std::move(ring));
	}
	else if (isEnemy)
	{
		currentEnemies.clear();
		auto presetMgr = EnemyPresetManager::GetInstance();
		std::string enemyType = node.enemyType;
		if (enemyType.empty())
		{
			enemyType = "RUSHER";
		}
		const auto* preset = presetMgr->GetPreset(enemyType);

		std::string formationType = node.formationType;
		int count = node.spawnCount;
		float spacing = node.formationSpacing;

		// Blender側で陣形指定がない場合、ゲーム側プリセットの陣形設定を自動適用
		if (preset)
		{
			if (formationType == "NONE" || formationType.empty() || count <= 1)
			{
				formationType = preset->formationType;
				count = preset->formationCount;
				spacing = preset->formationSpacing;
			}
		}

		int spawnCount = (std::max)(1, count);
		auto offsets = EnemyPresetManager::CalculateFormationOffsets(formationType, spawnCount, spacing);

		for (int i = 0; i < spawnCount; ++i)
		{
			auto enemy = std::make_unique<Enemy>();

			LevelObjectData spawnNode = node;
			spawnNode.enemyType = enemyType;
			if (i < static_cast<int>(offsets.size()))
			{
				spawnNode.translation.x += offsets[i].x;
				spawnNode.translation.y += offsets[i].y;
				spawnNode.translation.z += offsets[i].z;
			}

			enemy->Initialize(common, spawnNode, skyboxTexIndex);
			enemy->SetSpawnProgress(node.spawnProgress);
			enemy->SetSpawnDelay(static_cast<float>(i * node.spawnInterval));

			if (!node.texturePath.empty())
			{
				enemy->SetTexturePath(node.texturePath);
			}
			currentEnemies.push_back(enemy.get());
			enemies.push_back(std::move(enemy));
		}
	}
	else if (isObstacle)
	{
		auto obstacle = std::make_unique<Obstacle>();
		Vector3 rotRad;
		rotRad.x = node.rotation.x * (std::numbers::pi_v<float> / 180.0f);
		rotRad.y = node.rotation.y * (std::numbers::pi_v<float> / 180.0f);
		rotRad.z = node.rotation.z * (std::numbers::pi_v<float> / 180.0f);
		obstacle->Initialize(common, node.translation, node.scale, rotRad, node.fileName, skyboxTexIndex, node.collider, node.isDestructible);
		// オブジェクトのスポーン進行度を設定
		obstacle->SetSpawnProgress(node.spawnProgress);
		if (!node.texturePath.empty())
		{
			obstacle->SetTexturePath(node.texturePath);
		}
		obstacles.push_back(std::move(obstacle));
	}

	for (const auto& child : node.children)
	{
		LoadEnemiesOnlyFromNode(child, common, skyboxTexIndex, enemies, obstacles, enhanceRings, currentEnemies);
	}
}

void GamePlayScene::Initialize()
{
	auto initStartTime = std::chrono::high_resolution_clock::now();

	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();
	auto dxCommon = services->GetDirectXCommon();
	auto srvManager = services->GetSrvManager();
	auto spriteCommon = services->GetSpriteCommon();

	demoTimer_ = 0.0f;
	demoDeathLoopCount_ = 0;
	demoSurvivalTimer_ = 0.0f;
	demoBulletSpawned_ = false;
	demoHitTriggered_ = false;
	demoGameOverWaitTimer_ = 0.0f;
	demoMenuSelectStep_ = 0;

	collisionManager_ = std::make_unique<CollisionManager>();


	camera = std::make_unique<Camera>();
	camera->SetTranslate({ 0.0f, 6.0f, -20.0f });
	camera->SetRotation({ 0.0f, 0.0f, 0.0f });


	debugCamera_ = std::make_unique<Camera>();
	debugCamera_->SetTranslate({ 0.0f, 6.0f, -20.0f });
	debugCamera_->SetRotation({ 0.0f, 0.0f, 0.0f });

	activeCamera_ = camera.get();

	if (object3dCommon)
	{
		object3dCommon->SetDefaultCamera(activeCamera_);
	}


	ParticleManager::GetInstance()->RegisterQuad("quad", "circle2.png");
	ParticleManager::GetInstance()->RegisterRing("ring", "resources/sprites/effect/gradationLine.png", 32, 0.5f, 1.0f);
	ParticleManager::GetInstance()->RegisterCylinder("Cylinder", "resources/sprites/effect/gradationLine.png");

	uint32_t instancingSrvIndex = UINT32_MAX;

	auto texManager = TextureManager::GetInstance();
	dxCommon->BeginTextureUploadBatch();


	auto t0 = std::chrono::high_resolution_clock::now();
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(dxCommon, "resources/skybox.dds");
	auto t1 = std::chrono::high_resolution_clock::now();
	float skyboxMs = std::chrono::duration<float, std::milli>(t1 - t0).count();


	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("cube.obj");
	ModelManager::GetInstance()->LoadModel("player.obj");
	ModelManager::GetInstance()->LoadModel("memory.obj");
	ModelManager::GetInstance()->LoadModel("beam_enemy.obj");
	ModelManager::GetInstance()->LoadModel("collider_sphere_enemy.obj");


	texManager->LoadTexture("uvChecker.png");
	texManager->LoadTexture("monsterBall.png");
	texManager->LoadTexture("checkerBoard.png");
	texManager->LoadTexture("circle.png");
	texManager->LoadTexture("circle2.png");
	texManager->LoadTexture("gradationLine.png");
	texManager->LoadTexture("sprites/white.png");
	texManager->LoadTexture("prticle_kira.png");
	texManager->LoadTexture("hart.png");
	texManager->LoadTexture("light.png");
	texManager->LoadTexture("sprites/UI/ringGet_icon_outline.png");
	texManager->LoadTexture("sprites/UI/ringGet_icon.png");


	uint32_t uvCheckerTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("uvChecker.png");
	uint32_t monsterBallTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("monsterBall.png");
	uint32_t checkerBoardTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("checkerBoard.png");
	uint32_t skyboxTexIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath("resources/skybox.dds");




	uint32_t whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("sprites/white.png");
	if (whiteTex == 0)
	{

		whiteTex = TextureManager::GetInstance()->GetTextureIndexByFilePath("white.png");
	}
	whiteTexIndex_ = whiteTex;

	ringIconOutlineTex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("sprites/UI/ringGet_icon_outline.png");
	ringIconTex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath("sprites/UI/ringGet_icon.png");


	hud_.Initialize(spriteCommon, whiteTexIndex_, ringIconTex_, ringIconOutlineTex_);

	bossBattle_.Initialize(object3dCommon, spriteCommon, whiteTexIndex_);

	auto tLevel0 = std::chrono::high_resolution_clock::now();
	ReloadLevel();
	auto tLevel1 = std::chrono::high_resolution_clock::now();
	float levelMs = std::chrono::duration<float, std::milli>(tLevel1 - tLevel0).count();

	// カメラ
	cameraObject_ = std::make_unique<Object3d>();
	cameraObject_->Initialize(object3dCommon);

	player_ = std::make_unique<Player>();
	player_->Initialize(object3dCommon, skybox_->GetCubemapSrvIndex());
	player_->LoadSettings("resources/json/player/player_settings.json");


	player_->GetObject3d()->SetParent(cameraObject_.get());
	if (player_->GetColliderObject())
	{
		player_->GetColliderObject()->SetParent(cameraObject_.get());
	}
	player_->GetReticle()->SetParent(cameraObject_.get());
	if (player_->GetFrontReticle())
	{
		player_->GetFrontReticle()->SetParent(player_->GetObject3d());
	}
	if (player_->GetGroundMarkerObject())
	{
		player_->GetGroundMarkerObject()->SetParent(cameraObject_.get());
	}


	railCameraController_ = std::make_unique<RailCameraController>();
	std::vector<Rail*> railsRaw;
	for (auto& r : mainRails_)
	{
		railsRaw.push_back(r.get());
	}
	railCameraController_->Initialize(railsRaw, activeCamera_, cameraObject_.get());

	vfx_.Initialize(dxCommon, srvManager);

	// 装甲列車ボスの初期化
	armoredTrainBoss_ = std::make_unique<ArmoredTrainBoss>();
	Vector3 bossSpawnPos = { 0.0f, 0.0f, 150.0f };
	armoredTrainBoss_->Initialize(object3dCommon, bossSpawnPos, skybox_->GetCubemapSrvIndex());
	armoredTrainBoss_->SetTerrainObjects(&modelInstances);
	if (!mainRails_.empty())
	{
		armoredTrainBoss_->SetRail(mainRails_[0].get());
	}
	isBossSpawned_ = false;

	// 地形コリジョンデバッガーの初期化
	TerrainCollisionDebugger::GetInstance()->Initialize(dxCommon, object3dCommon);

	auto tUp0 = std::chrono::high_resolution_clock::now();
	texManager->ExecuteUploadCommands();
	texManager->ClearIntermediateResources();
	auto tUp1 = std::chrono::high_resolution_clock::now();
	float uploadMs = std::chrono::duration<float, std::milli>(tUp1 - tUp0).count();

	auto initEndTime = std::chrono::high_resolution_clock::now();
	lastLoadTimeMs_ = std::chrono::duration<float, std::milli>(initEndTime - initStartTime).count();

	char logBuf[512];
	snprintf(logBuf, sizeof(logBuf),
		"\n========================================\n"
 		"[Load Profiler Breakdown]\n"
		"  - Skybox Load:         %8.2f ms\n"
		"  - ReloadLevel (Map):   %8.2f ms\n"
		"  - GPU Texture Upload:  %8.2f ms\n"
		"  - TOTAL Initialize:    %8.2f ms (%.3f s)\n"
		"========================================\n\n",
		skyboxMs, levelMs, uploadMs, lastLoadTimeMs_, lastLoadTimeMs_ / 1000.0f);
	OutputDebugStringA(logBuf);

	// ゲームオーバー演出状態のリセット
	gameOverSequence_.Reset(false);
	demoSurvivalTimer_ = 0.0f;
	demoBulletSpawned_ = false;
	demoHitTriggered_ = false;
	demoGameOverWaitTimer_ = 0.0f;
	demoMenuSelectStep_ = 0;
	if (player_)
	{
		player_->SetVisible(true);
	}

	// ステージ開始時の降下演出を開始
	StartOpeningCutscene();

	// ゲームプレイBGMの再生（ループ再生、音量0.25）
	SoundManager::GetInstance()->PlayBGM("gameplayBGM.mp3", 0.25f, true);

	// ボス戦用UIテキスト項目の初期登録（JSONに未登録の場合のみ新規作成され、既存のJSON設定は完全に保護される）
	if (auto textMgr = UITextManager::GetInstance())
	{
		if (!textMgr->HasTextItem("BossTitle", "GAMEPLAY"))
		{
			UITextItem bossTitle;
			bossTitle.id = "BossTitle";
			bossTitle.text = "BOSS: ARMORED TRAIN \"BEHEMOTH\"";
			bossTitle.position = { 340.0f, 12.0f };
			bossTitle.fontSize = 20.0f;
			bossTitle.fontType = ImGuiManager::FontType::Japanese_MPLUS;
			bossTitle.color = { 1.0f, 0.35f, 0.31f, 1.0f };
			bossTitle.hasOutline = true;
			bossTitle.outlineThickness = 2.0f;
			bossTitle.hasShadow = true;
			bossTitle.shadowColor = { 0.0f, 0.0f, 0.0f, 0.8f };
			bossTitle.align = UITextAlign::Left;
			bossTitle.isVisible = false;
			textMgr->RegisterText(bossTitle, "GAMEPLAY");
		}
		if (!textMgr->HasTextItem("BossTimer", "GAMEPLAY"))
		{
			UITextItem bossTimer;
			bossTimer.id = "BossTimer";
			bossTimer.text = "TIME LIMIT";
			bossTimer.position = { 640.0f, 85.0f };
			bossTimer.fontSize = 28.0f;
			bossTimer.fontType = ImGuiManager::FontType::English_FiraMono;
			bossTimer.color = { 0.0f, 0.92f, 0.84f, 1.0f };
			bossTimer.hasOutline = true;
			bossTimer.outlineThickness = 2.2f;
			bossTimer.hasShadow = true;
			bossTimer.shadowColor = { 0.0f, 0.0f, 0.0f, 0.85f };
			bossTimer.align = UITextAlign::Center;
			bossTimer.isVisible = false;
			textMgr->RegisterText(bossTimer, "GAMEPLAY");
		}
		if (!textMgr->HasTextItem("BossWarningTitle", "GAMEPLAY"))
		{
			UITextItem bossWarnTitle;
			bossWarnTitle.id = "BossWarningTitle";
			bossWarnTitle.text = "WARNING";
			bossWarnTitle.position = { 640.0f, 280.0f };
			bossWarnTitle.fontSize = 58.0f;
			bossWarnTitle.fontType = ImGuiManager::FontType::Japanese_MPLUS;
			bossWarnTitle.color = { 1.0f, 0.15f, 0.15f, 1.0f };
			bossWarnTitle.hasOutline = true;
			bossWarnTitle.outlineThickness = 3.0f;
			bossWarnTitle.hasShadow = true;
			bossWarnTitle.shadowColor = { 0.0f, 0.0f, 0.0f, 0.85f };
			bossWarnTitle.align = UITextAlign::Center;
			bossWarnTitle.isVisible = false;
			textMgr->RegisterText(bossWarnTitle, "GAMEPLAY");
		}
		if (!textMgr->HasTextItem("BossWarningSub", "GAMEPLAY"))
		{
			UITextItem bossWarnSub;
			bossWarnSub.id = "BossWarningSub";
			bossWarnSub.text = "EMERGENCY: MASSIVE ARMORED TRAIN DETECTED";
			bossWarnSub.position = { 640.0f, 350.0f };
			bossWarnSub.fontSize = 22.0f;
			bossWarnSub.fontType = ImGuiManager::FontType::Japanese_MPLUS;
			bossWarnSub.color = { 1.0f, 0.94f, 0.94f, 1.0f };
			bossWarnSub.hasOutline = true;
			bossWarnSub.outlineThickness = 2.0f;
			bossWarnSub.align = UITextAlign::Center;
			bossWarnSub.isVisible = false;
			textMgr->RegisterText(bossWarnSub, "GAMEPLAY");
		}
		if (!textMgr->HasTextItem("BossTimeOverTitle", "GAMEPLAY"))
		{
			UITextItem bossTimeOverTitle;
			bossTimeOverTitle.id = "BossTimeOverTitle";
			bossTimeOverTitle.text = "TIME OVER";
			bossTimeOverTitle.position = { 640.0f, 270.0f };
			bossTimeOverTitle.fontSize = 68.0f;
			bossTimeOverTitle.fontType = ImGuiManager::FontType::Japanese_MPLUS;
			bossTimeOverTitle.color = { 1.0f, 0.12f, 0.12f, 1.0f };
			bossTimeOverTitle.hasOutline = true;
			bossTimeOverTitle.outlineThickness = 3.0f;
			bossTimeOverTitle.hasShadow = true;
			bossTimeOverTitle.shadowColor = { 0.0f, 0.0f, 0.0f, 0.9f };
			bossTimeOverTitle.align = UITextAlign::Center;
			bossTimeOverTitle.isVisible = false;
			textMgr->RegisterText(bossTimeOverTitle, "GAMEPLAY");
		}
		if (!textMgr->HasTextItem("BossTimeOverSub", "GAMEPLAY"))
		{
			UITextItem bossTimeOverSub;
			bossTimeOverSub.id = "BossTimeOverSub";
			bossTimeOverSub.text = "CRITICAL ANNIHILATION ATTACK INCOMING";
			bossTimeOverSub.position = { 640.0f, 350.0f };
			bossTimeOverSub.fontSize = 22.0f;
			bossTimeOverSub.fontType = ImGuiManager::FontType::Japanese_MPLUS;
			bossTimeOverSub.color = { 1.0f, 0.86f, 0.2f, 1.0f };
			bossTimeOverSub.hasOutline = true;
			bossTimeOverSub.outlineThickness = 2.0f;
			bossTimeOverSub.align = UITextAlign::Center;
			bossTimeOverSub.isVisible = false;
			textMgr->RegisterText(bossTimeOverSub, "GAMEPLAY");
		}
		if (!textMgr->HasTextItem("BossDefeatedTitle", "GAMEPLAY"))
		{
			UITextItem bossDefeated;
			bossDefeated.id = "BossDefeatedTitle";
			bossDefeated.text = "TARGET DESTROYED";
			bossDefeated.position = { 640.0f, 300.0f };
			bossDefeated.fontSize = 48.0f;
			bossDefeated.fontType = ImGuiManager::FontType::Japanese_MPLUS;
			bossDefeated.color = { 1.0f, 0.84f, 0.0f, 1.0f };
			bossDefeated.hasOutline = true;
			bossDefeated.outlineThickness = 3.0f;
			bossDefeated.hasShadow = true;
			bossDefeated.shadowColor = { 0.0f, 0.0f, 0.0f, 0.85f };
			bossDefeated.align = UITextAlign::Center;
			bossDefeated.isVisible = false;
			textMgr->RegisterText(bossDefeated, "GAMEPLAY");
		}
		if (!textMgr->HasLoadedFromFile())
		{
			if (!textMgr->HasTextItem("ExecuteCritical", "GAMEPLAY"))
			{
				UITextItem item;
				item.id = "ExecuteCritical";
				item.text = "[ CRITICAL // EXECUTE READY ]";
				item.position = { 640.0f, 160.0f };
				item.fontSize = 28.0f;
				item.fontType = ImGuiManager::FontType::Japanese_MPLUS;
				item.color = { 1.0f, 0.15f, 0.2f, 1.0f };
				item.hasOutline = true;
				item.outlineThickness = 2.0f;
				item.align = UITextAlign::Center;
				item.isVisible = false;
				textMgr->RegisterText(item, "GAMEPLAY");
			}
			if (!textMgr->HasTextItem("ExecuteGuide", "GAMEPLAY"))
			{
				UITextItem item;
				item.id = "ExecuteGuide";
				item.text = "[ HOLD SPACE TO LOCK ON ]";
				item.position = { 640.0f, 520.0f };
				item.fontSize = 22.0f;
				item.fontType = ImGuiManager::FontType::Japanese_MPLUS;
				item.color = { 0.0f, 0.95f, 1.0f, 1.0f };
				item.hasOutline = true;
				item.outlineThickness = 2.0f;
				item.align = UITextAlign::Center;
				item.isVisible = false;
				textMgr->RegisterText(item, "GAMEPLAY");
			}
			if (!textMgr->HasTextItem("MissionAccomplishedTitle", "GAMEPLAY"))
			{
				UITextItem item;
				item.id = "MissionAccomplishedTitle";
				item.text = "MISSION ACCOMPLISHED";
				item.position = { 640.0f, 290.0f };
				item.fontSize = 38.0f;
				item.fontType = ImGuiManager::FontType::Japanese_MPLUS;
				item.color = { 0.95f, 1.0f, 1.0f, 1.0f };
				item.hasOutline = true;
				item.outlineThickness = 3.0f;
				item.hasShadow = true;
				item.align = UITextAlign::Center;
				item.isVisible = false;
				textMgr->RegisterText(item, "GAMEPLAY");
			}
			if (!textMgr->HasTextItem("MissionAccomplishedSub", "GAMEPLAY"))
			{
				UITextItem item;
				item.id = "MissionAccomplishedSub";
				item.text = "- 任務完了 // TARGET PURGED -";
				item.position = { 640.0f, 335.0f };
				item.fontSize = 18.0f;
				item.fontType = ImGuiManager::FontType::Japanese_MPLUS;
				item.color = { 0.0f, 0.9f, 1.0f, 1.0f };
				item.hasOutline = true;
				item.outlineThickness = 2.0f;
				item.align = UITextAlign::Center;
				item.isVisible = false;
				textMgr->RegisterText(item, "GAMEPLAY");
			}
		}

		// シーン開始時に処刑・クリア関連テキストを確実に非表示にリセット
		textMgr->SetVisible("ExecuteCritical", false, "GAMEPLAY");
		textMgr->SetVisible("ExecuteGuide", false, "GAMEPLAY");
		textMgr->SetVisible("MissionAccomplishedTitle", false, "GAMEPLAY");
		textMgr->SetVisible("MissionAccomplishedSub", false, "GAMEPLAY");
	}

	// 電脳フィニッシャー（クリア演出シークエンス）の初期化
	bossFinisherSequence_ = std::make_unique<BossFinisherSequence>();
	if (bossFinisherSequence_)
	{
		uint32_t skyTex = skybox_ ? skybox_->GetCubemapSrvIndex() : 0;
		bossFinisherSequence_->Initialize(services->GetObject3dCommon(), camera.get(), railCameraController_.get(), skyTex);
		bossFinisherSequence_->SetTerrainObjects(&modelInstances);
		bossFinisherSequence_->SetOnVoxelHit([this](const Vector3& pos, int count, float minSz, float maxSz) {
			vfx_.SpawnVoxelHitParticles(pos, activeCamera_, count, minSz, maxSz);
		});
		bossFinisherSequence_->Reset(player_.get());
	}
}

void GamePlayScene::ReloadLevel()
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();


	while (modelInstances.size() > 1)
	{
		modelInstances.pop_back();
	}

	enemies_.clear();
	obstacles_.clear();
	bullets_.clear();
	missiles_.clear();
	railVisualizers_.clear();
	enemyRailVisualizers_.clear();
	if (railCameraController_)
	{
		// 解放される古いレールへのダングリングポインタアクセスを完全に防止
		railCameraController_->ClearRails();
	}
	mainRails_.clear();
	bossRail_.reset();
	hud_.AcquiredEnhanceRingCount() = 0;
	if (player_)
	{
		player_->SetPowerUpLevel(0);
	}

	// ボス戦状態のリセット
	bossBattle_.Reset();
	isBossSpawned_ = false;
	if (railCameraController_)
	{
		railCameraController_->SetBaseSpeed(50.0f);
	}
	if (armoredTrainBoss_)
	{
		armoredTrainBoss_->SetActive(false);
		armoredTrainBoss_->SetRail(nullptr);
		armoredTrainBoss_->SetTerrainObjects(nullptr);
	}


	auto levelData = LevelLoader::Load("resources/json/maps/template/template.json");
	if (levelData)
	{
		cachedLevelData_ = std::move(levelData);
		for (const auto& objData : cachedLevelData_->objects)
		{
			CreateObjectFromNode(objData, nullptr, modelInstances, mainRails_, bossRail_, bossBattle_.SpawnProgressThreshold(), object3dCommon, skybox_->GetCubemapSrvIndex(), enemies_, obstacles_, enhanceRings_);
		}
		ApplyVirtualSpaceStyle();
		OutputDebugStringA("LevelLoader: Successfully reloaded objects.\n");

		totalEnemiesCount_ = static_cast<int>(enemies_.size());
		killedEnemiesCount_ = 0;
		stageElapsedTime_ = 0.0f;
		bossElapsedTime_ = 0.0f;

		// BossRailが存在する場合、マップからの指定がなければ始点近傍のメインレール進行度を自動算出
		if (bossRail_ && bossRail_->IsValid() && !mainRails_.empty() && mainRails_[0]->IsValid())
		{
			if (bossBattle_.SpawnProgressThreshold() == 0.60f)
			{
				Vector3 bossStartPos = bossRail_->GetPosition(0.0f);
				bossBattle_.SpawnProgressThreshold() = mainRails_[0]->GetClosestProgress(bossStartPos);
			}
		}

		// 保存された設定ファイル（boss_settings.json）があれば読み込み（手動保存設定を優先）
		bossBattle_.LoadSettings(armoredTrainBoss_.get());
		if (armoredTrainBoss_)
		{
			armoredTrainBoss_->SetTerrainObjects(&modelInstances);
		}


		if (railCameraController_)
		{
			std::vector<Rail*> railsRaw;
			for (auto& r : mainRails_)
			{
				if (r && r->IsValid())
				{
					railsRaw.push_back(r.get());
				}
			}
			railCameraController_->Initialize(railsRaw, activeCamera_, cameraObject_.get());
		}
	}
	else
	{
		OutputDebugStringA("LevelLoader: Failed to reload level.\n");
	}


	railVisualizers_.clear();
	railModels_.clear();
	if (!mainRails_.empty())
	{
		std::vector<Vector4> colors = {
			{1.0f, 0.0f, 0.0f, 1.0f},
			{0.0f, 0.5f, 1.0f, 1.0f},
			{1.0f, 1.0f, 0.0f, 1.0f},
			{0.0f, 1.0f, 1.0f, 1.0f},
			{1.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, 0.5f, 0.0f, 1.0f}
		};

		std::string resolved = ResourceLocator::Resolve("rail.obj", ResourceLocator::AssetType::Model3D);
		std::string directory = "resources/models";
		std::string filename = "rail.obj";
		if (!resolved.empty())
		{
			std::filesystem::path rp(reinterpret_cast<const char8_t*>(resolved.c_str()));
			directory = rp.parent_path().string();
			filename = rp.filename().string();
		}

		int sampleCount = 200;
		int colorIndex = 0;
		for (auto& rail : mainRails_)
		{
			if (!rail || !rail->IsValid()) continue;

			auto railModel = std::make_unique<Model>();
			railModel->Initialize(ModelManager::GetInstance()->GetModelCommon(), directory, filename);
			railModel->SetColor(colors[colorIndex % colors.size()]);

			for (int i = 0; i < sampleCount; ++i)
			{
				float t1 = static_cast<float>(i) / sampleCount;
				float t2 = static_cast<float>(i + 1) / sampleCount;

				Vector3 p1 = rail->GetPosition(t1);
				Vector3 p2 = rail->GetPosition(t2);

				Vector3 dir = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
				float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
				if (length < 0.0001f) continue;

				dir.x /= length; dir.y /= length; dir.z /= length;

				Vector3 center = { (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f, (p1.z + p2.z) * 0.5f };

				auto obj = std::make_unique<Object3d>();
				obj->Initialize(object3dCommon);
				obj->SetModel(railModel.get());
				obj->SetTranslate(center);
				float yaw = std::atan2(dir.x, dir.z);
				float pitch = std::asin(-dir.y);
				obj->SetRotation(Vector3(pitch, yaw, 0.0f));

				obj->SetScale(Vector3(0.02f, 0.02f, length));

				obj->SetEnvironmentCoefficient(0.0f);
				obj->SetEnvironmentTextureIndex(skybox_->GetCubemapSrvIndex());

				railVisualizers_.push_back(std::move(obj));
			}

			railModels_.push_back(std::move(railModel));
			colorIndex++;
		}
	}


	enemyRailModel_ = std::make_unique<Model>();
	std::string resolved = ResourceLocator::Resolve("rail.obj", ResourceLocator::AssetType::Model3D);
	if (!resolved.empty())
	{
		std::filesystem::path rp(reinterpret_cast<const char8_t*>(resolved.c_str()));
		enemyRailModel_->Initialize(ModelManager::GetInstance()->GetModelCommon(), rp.parent_path().string(), rp.filename().string());
	}
	else
	{
		enemyRailModel_->Initialize(ModelManager::GetInstance()->GetModelCommon(), "resources/models", "rail.obj");
	}
	enemyRailModel_->SetColor({ 0.5f, 1.0f, 0.0f, 1.0f });

	int sampleCount = 100;
	for (auto& enemy : enemies_)
	{
		const Rail* rail = enemy->GetMovePath();
		if (!rail || !rail->IsValid()) continue;
		for (int i = 0; i < sampleCount; ++i)
		{
			float t1 = static_cast<float>(i) / sampleCount;
			float t2 = static_cast<float>(i + 1) / sampleCount;

			Vector3 p1 = rail->GetPosition(t1);
			Vector3 p2 = rail->GetPosition(t2);

			Vector3 dir = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
			float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
			if (length < 0.0001f) continue;

			dir.x /= length; dir.y /= length; dir.z /= length;

			Vector3 center = { (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f, (p1.z + p2.z) * 0.5f };

			auto obj = std::make_unique<Object3d>();
			obj->Initialize(object3dCommon);
			obj->SetModel(enemyRailModel_.get());
			obj->SetTranslate(center);
			float yaw = std::atan2(dir.x, dir.z);
			float pitch = std::asin(-dir.y);
			obj->SetRotation(Vector3(pitch, yaw, 0.0f));

			obj->SetScale(Vector3(0.015f, 0.015f, length));

			obj->SetEnvironmentCoefficient(0.0f);
			obj->SetEnvironmentTextureIndex(skybox_->GetCubemapSrvIndex());

			enemyRailVisualizers_.push_back(std::move(obj));
		}
	}

	// レベル再読込時も降下演出を開始
	StartOpeningCutscene();
}

void GamePlayScene::ApplyVirtualSpaceStyle()
{
	if (skybox_)
	{
		skybox_->SetVirtualSpace(isVirtualSpace_);
	}
	int32_t lightingMode = isVirtualSpace_ ? 7 : 1;
	for (auto& obj : modelInstances)
	{
		if (obj)
		{
			obj->SetSelectLightings(lightingMode);
		}
	}
	for (auto& obs : obstacles_)
	{
		if (obs && obs->GetObject3d())
		{
			obs->GetObject3d()->SetSelectLightings(lightingMode);
		}
	}
}

void GamePlayScene::ReloadEnemiesOnly()
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();

	enemies_.clear();
	obstacles_.clear();
	enemyRailVisualizers_.clear();

	auto levelData = LevelLoader::Load("resources/json/maps/template/template.json");
	if (levelData)
	{
		for (const auto& objData : levelData->objects)
		{
			LoadEnemiesOnlyFromNode(objData, object3dCommon, skybox_->GetCubemapSrvIndex(), enemies_, obstacles_, enhanceRings_);
		}
		ApplyVirtualSpaceStyle();
		OutputDebugStringA("LevelLoader: Successfully respawned enemies.\n");


		if (enemyRailModel_)
		{
			int sampleCount = 100;
			for (auto& enemy : enemies_)
			{
				const Rail* rail = enemy->GetMovePath();
				if (!rail || !rail->IsValid()) continue;
				for (int i = 0; i < sampleCount; ++i)
				{
					float t1 = static_cast<float>(i) / sampleCount;
					float t2 = static_cast<float>(i + 1) / sampleCount;
					Vector3 p1 = rail->GetPosition(t1);
					Vector3 p2 = rail->GetPosition(t2);
					Vector3 dir = { p2.x - p1.x, p2.y - p1.y, p2.z - p1.z };
					float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
					if (length < 0.0001f) continue;
					dir.x /= length; dir.y /= length; dir.z /= length;
					Vector3 center = { (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f, (p1.z + p2.z) * 0.5f };
					auto obj = std::make_unique<Object3d>();
					obj->Initialize(object3dCommon);
					obj->SetModel(enemyRailModel_.get());
					obj->SetTranslate(center);
					float yaw = std::atan2(dir.x, dir.z);
					float pitch = std::asin(-dir.y);
					obj->SetRotation(Vector3(pitch, yaw, 0.0f));
					obj->SetScale(Vector3(0.015f, 0.015f, length));
					obj->SetEnvironmentCoefficient(0.0f);
					obj->SetEnvironmentTextureIndex(skybox_->GetCubemapSrvIndex());
					enemyRailVisualizers_.push_back(std::move(obj));
				}
			}
		}
	}
	else
	{
		OutputDebugStringA("LevelLoader: Failed to respawn enemies.\n");
	}
}

void GamePlayScene::SeamlessResetForRetry()
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services ? services->GetObject3dCommon() : nullptr;

	// 1. 弾、ミサイル、破片エフェクト等の即時クリア
	bullets_.clear();
	missiles_.clear();
	hud_.ClearScorePopups();
	openingSequence_.ClearShards();
	vfx_.ClearAll();
	gameOverSequence_.Reset(true);

	// 2. 敵・障害物・リングの再生成（キャッシュデータから復元、地形やレールは一切破棄せずゼロフレームで復帰！）
	enemies_.clear();
	obstacles_.clear();
	enhanceRings_.clear();
	memoryItems_.clear();
	hud_.AcquiredEnhanceRingCount() = 0;

	if (cachedLevelData_ && object3dCommon)
	{
		for (const auto& objData : cachedLevelData_->objects)
		{
			LoadEnemiesOnlyFromNode(objData, object3dCommon, skybox_->GetCubemapSrvIndex(), enemies_, obstacles_, enhanceRings_);
		}
		ApplyVirtualSpaceStyle();
	}

	// 3. ボス状態のリセット（モデルや車両は破棄せず、パラメータとチェイス状態のみ初期化）
	bossBattle_.Reset();
	isBossSpawned_ = false;
	if (railCameraController_)
	{
		railCameraController_->SetBaseSpeed(50.0f);
	}
	if (armoredTrainBoss_)
	{
		armoredTrainBoss_->SetActive(false);
		armoredTrainBoss_->ResetChaseState();
		if (bossRail_ && bossRail_->IsValid())
		{
			armoredTrainBoss_->SetRail(bossRail_.get());
			armoredTrainBoss_->SetRailProgress(0.0f);
		}
		else if (railCameraController_ && !mainRails_.empty() && mainRails_[0]->IsValid())
		{
			armoredTrainBoss_->SetRail(mainRails_[0].get());
			armoredTrainBoss_->SetRailProgress(0.0f);
		}
	}

	// 4. レールカメラを進行度 0.0f に巻き戻し
	if (railCameraController_)
	{
		railCameraController_->Reset();
	}

	// 5. 自機の状態を完全初期化
	if (player_)
	{
		player_->SetDead(false);
		player_->Heal(player_->GetMaxHp());
		player_->ResetBoost();
		player_->ResetPosture();
		player_->SetPowerUpLevel(0);
		player_->SetVisible(true);
	}

	score_ = 0;
	gameOverSequence_.Reset(false);
	demoSurvivalTimer_ = 0.0f;
	demoBulletSpawned_ = false;
	demoHitTriggered_ = false;
	demoGameOverWaitTimer_ = 0.0f;
	demoMenuSelectStep_ = 0;

	// 6. シームレスにオープニングカットシーン（地形マテリアライズ・自機再構成）を開始！
	StartOpeningCutscene();

	// 7. ゲームプレイBGMを再生
	SoundManager::GetInstance()->PlayBGM("gameplayBGM.mp3", 0.25f, true);
}


void GamePlayScene::Finalize()
{
	// BGMおよび効果音の停止
	SoundManager::GetInstance()->StopBGM();
	SoundManager::GetInstance()->StopAllSE();

	if (bossFinisherSequence_)
	{
		bossFinisherSequence_->Reset(player_.get());
	}

	sprites.clear();
	modelInstances.clear();

	if (auto pp = EngineServices::GetInstance()->GetPostProcess())
	{
		pp->SetEffectActive("Grayscale", false);
		pp->SetEffectActive("Vignetting", false);
		pp->SetEffectActive("Random(Glitch)", false);
		if (auto* d = pp->GetData())
		{
			d->glitchStrength = 0.0f;
			d->noiseStrength = 0.0f;
		}
	}

	skybox_.reset();
	camera.reset();
	debugCamera_.reset();
	activeCamera_ = nullptr;
}

namespace
{
	float LerpAngle(float a, float b, float t)
	{
		float diff = b - a;
		while (diff < -3.14159265f) diff += 6.2831853f;
		while (diff > 3.14159265f) diff -= 6.2831853f;
		return a + diff * t;
	}
}

void GamePlayScene::Update()
{
	auto services = EngineServices::GetInstance();
	auto input = services->GetInput();
	float dt = services->GetDeltaTime();

	HandleGlobalHotkeys(services, input);
	UpdateDemoPlay(services, dt);

#ifdef ENABLE_EDITOR
	// EngineServicesのゲーム再生状態と同期
	isPlaying_ = services->IsGamePlaying();
#endif

	if (isPlaying_)
	{
		activeCamera_ = camera.get();
	}
	else
	{
		activeCamera_ = debugCamera_.get();
	}
	if (auto objCommon = services->GetObject3dCommon())
	{
		objCommon->SetDefaultCamera(activeCamera_);
	}

	float unscaledGameSpeed = baseGameSpeed_;





	if (input && activeCamera_)
	{

		const float kRotateSpeed = 0.005f;
		const float kMoveSpeed = 8.0f;
		const float kZoomSpeed = 0.0015f;

		LONG dx = input->GetMouseMoveX();
		LONG dy = input->GetMouseMoveY();
		LONG wheel = input->GetMouseWheel();


		if (!isPlaying_)
		{
			if (input->PushMouseButton(1))
			{
				Vector3 rot = activeCamera_->GetRotation();

				rot.y += static_cast<float>(dx) * kRotateSpeed;
				rot.x += static_cast<float>(dy) * kRotateSpeed;


				const float kMaxPitch = 1.5f;
				const float kMinPitch = -1.5f;
				rot.x = std::clamp(rot.x, kMinPitch, kMaxPitch);

				activeCamera_->SetRotation(rot);
			}


			float currentMoveSpeed = kMoveSpeed;
			if (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT))
			{
				currentMoveSpeed *= 25.0f;
			}
			float moveStep = currentMoveSpeed * dt;
			Vector3 pos = activeCamera_->GetTranslate();
			Vector3 rot = activeCamera_->GetRotation();
			float yaw = rot.y;


			Vector3 forward = { std::sinf(yaw), 0.0f, std::cosf(yaw) };
			Vector3 right = { std::cosf(yaw), 0.0f, -std::sinf(yaw) };

			auto normalize = [](Vector3 v)
				{
					float len = std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
					if (len > 1e-6f) { v.x /= len; v.y /= len; v.z /= len; }
					return v;
				};

			forward = normalize(forward);
			right = normalize(right);

			if (input->PushKey(DIK_W))
			{
				pos.x += forward.x * moveStep;
				pos.z += forward.z * moveStep;
			}
			if (input->PushKey(DIK_S))
			{
				pos.x -= forward.x * moveStep;
				pos.z -= forward.z * moveStep;
			}
			if (input->PushKey(DIK_D))
			{
				pos.x += right.x * moveStep;
				pos.z += right.z * moveStep;
			}
			if (input->PushKey(DIK_A))
			{
				pos.x -= right.x * moveStep;
				pos.z -= right.z * moveStep;
			}
			if (input->PushKey(DIK_E))
			{
				pos.y += moveStep;
			}
			if (input->PushKey(DIK_Q))
			{
				pos.y -= moveStep;
			}

			activeCamera_->SetTranslate(pos);



		}

		// ポーズメニューの開閉判定（プレイ中、かつゲームオーバーやクリア中でない場合）
		if (isPlaying_ && gamePhase_ != GamePhase::GAMEOVER && gamePhase_ != GamePhase::CLEAR)
		{
			if (input)
			{
				if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_P) || input->TriggerPadButton(XINPUT_GAMEPAD_START))
				{
					pauseMenu_.Toggle();
				}
			}
		}

	float unscaledGameSpeed = baseGameSpeed_;
		if (pauseMenu_.IsPaused())
		{
			pauseMenu_.Update(GetSceneManager());
		}
		else if (isPlaying_)
		{
			unscaledGameSpeed = baseGameSpeed_;
			UpdateRailAndCamera(services, dt, unscaledGameSpeed);
		}

	UpdateActorsAndCollisions(dt, unscaledGameSpeed);
	UpdateCameraShakeAndVfx(dt);
	}

	if (player_)
	{
		hud_.UpdateBars(dt, player_.get());
		if (isPlaying_)
		{
			CheckGameEndConditions();
		}
	}

	hud_.UpdateHudTexts(player_.get(), bossFinisherSequence_.get(), score_, gamePhase_ == GamePhase::START_CUTSCENE, gamePhase_ == GamePhase::GAMEOVER, gamePhase_ == GamePhase::CLEAR, openingSequence_.GetTimer());

#ifdef ENABLE_EDITOR
	editor_.Draw(*this);
#endif // ENABLE_EDITOR
}

void GamePlayScene::HandleGlobalHotkeys(EngineServices* services, Input* input)
{
	if (input && input->TriggerKey(DIK_0))
	{
		services->SetEditorMode(true);
	}


	if (input && input->TriggerKey(DIK_F2))
	{
		isDrawCollider_ = !isDrawCollider_;
	}

	if (input && input->TriggerKey(DIK_F3))
	{
		isDrawTerrainWireframe_ = !isDrawTerrainWireframe_;
	}

	if (input && input->TriggerKey(DIK_F5))
	{
		ReloadLevel();
	}

	if (input && input->TriggerKey(DIK_M))
	{
		SoundManager::GetInstance()->ToggleMasterMute();
	}

}

void GamePlayScene::UpdateDemoPlay(EngineServices* services, float dt)
{
	// デモプレイ中：ゲームクリア＆処刑フィニッシャー演出を自動進行する
	if (services && services->IsDemoMode())
	{
		if (player_)
		{
			player_->SetGodMode(true);
		}
		if (gamePhase_ == GamePhase::PLAYING)
		{
			// 生き生きとした自動飛行（左右・上下へのスウェー移動＋バレルロール回避）
			if (player_)
			{
				bool inFinisher = (bossFinisherSequence_ && bossFinisherSequence_->IsActive());
				float moveAmpX = inFinisher ? 0.45f : 0.85f;
				float moveAmpY = inFinisher ? 0.25f : 0.45f;
				float moveX = std::sin(demoTimer_ * 2.2f) * moveAmpX;
				float moveY = std::sin(demoTimer_ * 1.5f + 0.6f) * moveAmpY;
				bool rollNow = false;
				if (!inFinisher && bossBattle_.GetStep() == BossBattleController::Step::BATTLE && armoredTrainBoss_ && armoredTrainBoss_->IsCloseToPlayer() && !demoHitTriggered_ && demoSurvivalTimer_ >= 0.5f)
				{
					demoHitTriggered_ = true; // デモ用バレルロール1回トリガーとして使用
					rollNow = true;
				}
				player_->SetDemoInputOverride(true, moveX, moveY, rollNow, 1.0f);
			}
			demoTimer_ += dt;

			if (bossBattle_.GetStep() == BossBattleController::Step::NOT_ACTIVE)
			{
				// 39%位置から開始後、すぐにボス戦へ移行
				demoSurvivalTimer_ += dt;
				if (demoSurvivalTimer_ >= 0.45f)
				{
					demoSurvivalTimer_ = 0.0f;
					StartBossWarningSequence();
				}
			}
			else if (bossBattle_.GetStep() == BossBattleController::Step::BATTLE && armoredTrainBoss_ && armoredTrainBoss_->IsActive() &&
				(!bossFinisherSequence_ || (!bossFinisherSequence_->IsActive() && !bossFinisherSequence_->IsCompleted())))
			{
				// ボスがカメラ前方（並走位置）にしっかり映ってからタイマーを進める
				if (armoredTrainBoss_->IsCloseToPlayer())
				{
					demoSurvivalTimer_ += dt;
				}
				// ボスがカメラ前に現れてから1.2秒後（ロール回避後）に先頭車両へ向けて通常弾を発射（1発命中でHP1処刑条件へ移行）
				if (demoSurvivalTimer_ >= 1.2f && !demoBulletSpawned_)
				{
					demoBulletSpawned_ = true;
					demoSurvivalTimer_ = 1.0f; // 万一外れた場合は0.5秒後に再射撃
					if (player_ && services->GetObject3dCommon())
					{
						Vector3 pPos = player_->GetWorldPosition();
						Vector3 targetPos = armoredTrainBoss_->GetCarriagePosition(0) + Vector3{ 0.0f, 7.0f, 0.0f };
						Vector3 dir = targetPos - pPos;
						float len = dir.Length();
						Vector3 vel = (len > 0.001f) ? (dir / len) * 12.0f : Vector3{ 0.0f, 0.0f, 12.0f };
						auto b = std::make_unique<PlayerBullet>();
						b->Initialize(services->GetObject3dCommon(), pPos, vel, nullptr);
						bullets_.push_back(std::move(b));
						SoundManager::GetInstance()->PlaySE("shot.wav", 0.5f);
					}
				}
				if (demoBulletSpawned_ && demoSurvivalTimer_ >= 1.5f)
				{
					demoBulletSpawned_ = false;
				}
			}
		}
		else if (player_)
		{
			player_->SetDemoInputOverride(false);
		}
	}
	else
	{
		if (player_)
		{
			if (player_->IsGodMode()) player_->SetGodMode(false);
			player_->SetDemoInputOverride(false);
		}
		demoSurvivalTimer_ = 0.0f;
		demoBulletSpawned_ = false;
		demoHitTriggered_ = false;
	}
}

void GamePlayScene::UpdateRailAndCamera(EngineServices* services, float dt, float& unscaledGameSpeed)
{

			auto pp = EngineServices::GetInstance()->GetPostProcess();
			unscaledGameSpeed = baseGameSpeed_;
			if (player_ && player_->IsBoosting())
			{
				unscaledGameSpeed = baseGameSpeed_ * 1.5f;
				vfx_.Thruster().SetBaseColor({ 1.0f, 0.2f, 0.0f, 1.0f });
				if (pp)
				{
					pp->SetEffectActive("RadialBlur", true);
					pp->GetData()->radialBlurIntensity = 0.08f;
				}
				if (railCameraController_)
				{
					railCameraController_->SetSpeedMultiplier(1.5f);
				}
			}
			else
			{
				unscaledGameSpeed = baseGameSpeed_;
				vfx_.Thruster().SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				if (pp)
				{
					pp->SetEffectActive("RadialBlur", false);
				}
				if (railCameraController_)
				{
					railCameraController_->SetSpeedMultiplier(1.0f);
				}
			}

			// スタート降下演出（Opening Cutscene）の更新
			if (gamePhase_ == GamePhase::START_CUTSCENE)
			{
				UpdateOpeningCutscene(dt);
				// 前に進みながら降下演出を再生（止まらない！）
				unscaledGameSpeed = baseGameSpeed_;
				if (railCameraController_)
				{
					railCameraController_->SetSpeedMultiplier(1.0f);
				}
			}
			else if (gamePhase_ == GamePhase::GAMEOVER)
			{
				// ゲームオーバー演出（カメラ停止、自機落下・爆散・UI表示）の更新
				UpdateGameOverSequence(dt);
				unscaledGameSpeed = 0.0f;
				if (railCameraController_)
				{
					railCameraController_->SetSpeedMultiplier(0.0f);
				}
			}
			else if (gamePhase_ == GamePhase::CLEAR)
			{
				// ゲームクリア演出（カメラ停止、自機上昇・飛び去り、ミッション完了UI・スコア表示）の更新
				UpdateClearSequence(dt);
				unscaledGameSpeed = 0.0f;
				if (railCameraController_)
				{
					railCameraController_->SetSpeedMultiplier(0.0f);
				}
			}
			else if (gamePhase_ == GamePhase::PLAYING)
			{
				openingSequence_.TickMissionStartText(dt);
				// ステージ全体経過時間の計測（処刑ムービー突入前まで）
				if (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase())
				{
					// 電脳処刑フィニッシャーの黒帯ムービー中はレール進行を完全停止し、カメラを一切動かさない
					unscaledGameSpeed = 0.0f;
					if (railCameraController_)
					{
						railCameraController_->SetSpeedMultiplier(0.0f);
					}
				}
				else
				{
					stageElapsedTime_ += dt;
				}
			}

			// ブースト時の動的FOV拡大演出（通常66度 → ブースト時76度へ滑らかに補間）
			if (activeCamera_)
			{
				float targetFov = (player_ && player_->IsBoosting()) ? 1.32f : 1.15f;
				float currentFov = activeCamera_->GetFovY();
				currentFov += (targetFov - currentFov) * 0.15f;
				activeCamera_->SetFovY(currentFov);
			}


			if (isJustDodgeActive_)
			{
				justDodgeTimer_ -= 1.0f;


				if (pp)
				{
					float t = justDodgeTimer_ / justDodgeMaxTime_;

					pp->GetData()->vignetteIntensity = std::sinf(t * 3.141592f) * 0.6f;
				}

				if (justDodgeTimer_ <= 0.0f)
				{
					isJustDodgeActive_ = false;


					if (pp)
					{
						pp->SetEffectActive("Grayscale", false);
						pp->SetEffectActive("Vignetting", false);
					}
				}
			}

			gameSpeed_ = unscaledGameSpeed;
			if (isJustDodgeActive_)
			{
				gameSpeed_ *= justDodgeSlowSpeed_;
			}

			if (railCameraController_)
			{
				Vector3 cameraTrackPos = player_->GetTranslate();
				if (gamePhase_ == GamePhase::CLEAR || (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase()))
				{
					// クリア演出・処刑ムービー中は自機が移動してもレールカメラを動かさない
					cameraTrackPos = clearSequence_.GetStartPlayerPos();
				}
				railCameraController_->Update(gameSpeed_, cameraTrackPos);
			}

			// レール進行度監視によるボス自動出現トリガー
			if (gamePhase_ == GamePhase::PLAYING && bossBattle_.GetStep() == BossBattleController::Step::NOT_ACTIVE && railCameraController_)
			{
				if (railCameraController_->GetProgress() >= bossBattle_.SpawnProgressThreshold())
				{
					StartBossWarningSequence();
				}
			}

			// ボス戦全体の更新（WARNING警告演出・HPバー追従・撃破シークエンス）
			UpdateBossBattle(dt);
}

void GamePlayScene::UpdateActorsAndCollisions(float dt, float unscaledGameSpeed)
{
	if (player_)
	{
		if (isPlaying_ && !pauseMenu_.IsPaused() && gamePhase_ != GamePhase::GAMEOVER && gamePhase_ != GamePhase::CLEAR)
		{

			bool wasBanking = player_->IsBanking();
			Vector3 prevLeftWing = player_->GetLeftWingPosition();
			Vector3 prevRightWing = player_->GetRightWingPosition();


			// エイムアシストは Player::UpdateReticleAndAimAssist で精密に一元管理
			player_->SetAssistTarget(nullptr);

			// レール進行度に応じた動的移動制限の適用
			if (!mainRails_.empty() && mainRails_[0]->IsValid() && railCameraController_)
			{
				float p = railCameraController_->GetProgress();
				float limitX, limitYMin, limitYMax;
				mainRails_[0]->GetMoveLimits(p, limitX, limitYMin, limitYMax);
				player_->SetTargetMoveLimits(limitX, limitYMin, limitYMax);
			}

			player_->Update(bullets_, missiles_, enemies_, cameraObject_.get(), unscaledGameSpeed, armoredTrainBoss_.get());


			if (player_->ConsumeDodgeTrigger())
			{
				const Matrix4x4& wMat = player_->GetObject3d()->GetmatWorld();

				Vector3 pPos = { wMat.m[3][0], wMat.m[3][1], wMat.m[3][2] };


				Vector3 forward = { wMat.m[2][0], wMat.m[2][1], wMat.m[2][2] };
				float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
				if (len > 0.0001f)
				{
					forward.x /= len; forward.y /= len; forward.z /= len;
				}


				Vector3 effectPos = { pPos.x - forward.x * 2.0f, pPos.y - forward.y * 2.0f, pPos.z - forward.z * 2.0f };

				vfx_.Dodge().SetPosition(effectPos);
				vfx_.Dodge().Play();
			}


			if (player_->IsBanking())
			{
				Vector3 leftWing = player_->GetLeftWingPosition();
				Vector3 rightWing = player_->GetRightWingPosition();

				if (wasBanking)
				{

					int emitCount = 8;
					for (int i = 1; i <= emitCount; ++i)
					{
						float t = (float)i / emitCount;
						Vector3 lPos = {
							prevLeftWing.x + (leftWing.x - prevLeftWing.x) * t,
							prevLeftWing.y + (leftWing.y - prevLeftWing.y) * t,
							prevLeftWing.z + (leftWing.z - prevLeftWing.z) * t
						};
						Vector3 rPos = {
							prevRightWing.x + (rightWing.x - prevRightWing.x) * t,
							prevRightWing.y + (rightWing.y - prevRightWing.y) * t,
							prevRightWing.z + (rightWing.z - prevRightWing.z) * t
						};

						vfx_.Trail().SetPosition(lPos);
						vfx_.Trail().Play();

						vfx_.Trail().SetPosition(rPos);
						vfx_.Trail().Play();
					}
				}
				else
				{
					vfx_.Trail().SetPosition(leftWing);
					vfx_.Trail().Play();

					vfx_.Trail().SetPosition(rightWing);
					vfx_.Trail().Play();
				}
			}
		}
		else
		{

			player_->Update3DObjectOnly();
		}
	}


	if (isPlaying_ && !pauseMenu_.IsPaused())
	{
		for (auto it = bullets_.begin(); it != bullets_.end(); )
		{
			(*it)->Update(gameSpeed_);
			if ((*it)->IsDead())
			{
				it = bullets_.erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto it = missiles_.begin(); it != missiles_.end(); )
		{
			(*it)->Update(gameSpeed_);

			if ((*it)->GetCurrentPhase() == PlayerMissile::Phase::FLIGHT)
			{
				vfx_.MissileSmoke().SetPosition((*it)->GetPosition());
				vfx_.MissileSmoke().Play();
			}

			if ((*it)->IsDead())
			{
				it = missiles_.erase(it);
			}
			else
			{
				++it;
			}
		}

		Vector3 cameraPos = activeCamera_->GetTranslate();
		Vector3 rot = activeCamera_->GetRotation();
		Vector3 cameraForward = { std::sinf(rot.y), 0.0f, std::cosf(rot.y) };


		// 各種アクターの更新
		for (auto& enemy : enemies_)
		{
			enemy->Update(cameraPos, cameraForward, player_.get(), enemyBullets_, gameSpeed_);
		}

		if (armoredTrainBoss_ && (isBossSpawned_ || (bossFinisherSequence_ && bossFinisherSequence_->IsActive())))
		{
			Vector3 camPos = activeCamera_ ? activeCamera_->GetTranslate() : Vector3{ 0,0,0 };
			float playerSpeed = 50.0f;
			if (railCameraController_)
			{
				playerSpeed = railCameraController_->GetBaseSpeed() * railCameraController_->GetSpeedMultiplier();
			}
			// 処刑ムービー中はレール進行停止のため gameSpeed_ が 0 になるが、
			// ボスのグリッチ演出と瀕死パーティクル（criticalParticles_）は動かし続ける
			float bossGameSpeed = (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase()) ? (dt * 60.0f) : gameSpeed_;
			armoredTrainBoss_->Update(camPos, cameraForward, playerSpeed, player_.get(), enemyBullets_, bossGameSpeed);
		}

		#if 1
		Vector3 bombPos = { 0.0f, 0.0f, 0.0f };
		float bombRadius = 0.0f;
		bool isBombClearing = player_ && player_->HasActiveBombExplosion(&bombPos, &bombRadius);

		for (auto it = enemyBullets_.begin(); it != enemyBullets_.end(); )
		{
			if (isBombClearing)
			{
				Vector3 ebPos = (*it)->GetPosition();
				float bdx = ebPos.x - bombPos.x;
				float bdy = ebPos.y - bombPos.y;
				float bdz = ebPos.z - bombPos.z;
				if (bdx * bdx + bdy * bdy + bdz * bdz <= bombRadius * bombRadius)
				{
					it = enemyBullets_.erase(it);
					continue;
				}
			}
			(*it)->Update(gameSpeed_);
			if ((*it)->IsDead())
			{
				it = enemyBullets_.erase(it);
			}
			else
			{
				++it;
			}
		}
#endif
		for (auto& bullet : enemyBullets_)
		{
			// bullet->Update(gameSpeed_);
		}

		for (auto& ring : enhanceRings_)
		{
			ring->Update();
		}

		// メモリドロップアイテムの更新・マグネット吸引・回収判定
		for (auto it = memoryItems_.begin(); it != memoryItems_.end(); )
		{
			(*it)->Update(gameSpeed_, player_.get());
			if ((*it)->IsDead())
			{
				it = memoryItems_.erase(it);
			}
			else
			{
				++it;
			}
		}

		for (auto& obstacle : obstacles_)
		{
			obstacle->Update();
		}

		// 地形コリジョンデバッガーのフレーム開始
		TerrainCollisionDebugger::GetInstance()->BeginFrame();

		// 衝突判定管理クラス (CollisionManager) による一括判定
		CollisionContext colCtx;
		colCtx.player = player_.get();
		colCtx.playerBullets = &bullets_;
		colCtx.playerMissiles = &missiles_;
		colCtx.enemies = &enemies_;
		colCtx.boss = armoredTrainBoss_.get();
		colCtx.isBossSpawned = isBossSpawned_;
		colCtx.enemyBullets = &enemyBullets_;
		colCtx.obstacles = &obstacles_;
		colCtx.enhanceRings = &enhanceRings_;
		colCtx.terrainModels = &modelInstances;
		colCtx.cameraObject = cameraObject_.get();

		colCtx.hitEffect = &vfx_.Hit();
		colCtx.explosionEffect = &vfx_.Explosion();
		colCtx.onSpawnVoxelHit = [this](const Vector3& pos, int count, float minSz, float maxSz) {
			vfx_.SpawnVoxelHitParticles(pos, activeCamera_, count, minSz, maxSz);
		};
		colCtx.dodgeEffect = &vfx_.Dodge();
		colCtx.ringEffect = &vfx_.Ring();
		colCtx.healRingEffect = &vfx_.HealRing();

		colCtx.isDemoMode = EngineServices::GetInstance()->IsDemoMode();
		colCtx.score = &score_;
		colCtx.cameraShakeTimer = &cameraShakeTimer_;
		colCtx.isJustDodgeActive = &isJustDodgeActive_;
		colCtx.justDodgeTimer = &justDodgeTimer_;
		colCtx.justDodgeMaxTime = justDodgeMaxTime_;
		colCtx.acquiredEnhanceRingCount = &hud_.AcquiredEnhanceRingCount();
		colCtx.maxEnhanceRingIcons = GamePlayHUD::kMaxEnhanceRingIcons;

		colCtx.onBossDefeated = [this]() {
			if (bossBattle_.GetStep() == BossBattleController::Step::BATTLE)
			{
				bossBattle_.SetDefeated();
				score_ += 5000; // ボス撃破ボーナス
				SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
				cameraShakeTimer_ = 1.0f;
			}
		};

		colCtx.onEnemyKilled = [this](const Vector3& pos, int addScore) {
			++killedEnemiesCount_;
			// 1. 敵撃破時のごくわずかな微小カメラシェイク（1〜2フレームだけ数ピクセル振動）
			enemyKillShakeFrames_ = 2;
			// 2. 敵撃破座標にスコア加算ポップアップ演出を登録
			hud_.AddScorePopup(pos, addScore, 0.65f);
			// 3. 敵撃破時のメモリドロップ（40%の確率でビットメモリをドロップ）
			static std::mt19937 memRng(1337);
			std::uniform_real_distribution<float> memDist(0.0f, 1.0f);
			if (memDist(memRng) < 0.40f) {
				const MemoryType kDropTypes[] = {
					MemoryType::CYBER_BIT,

					MemoryType::BOMB,
					MemoryType::SPREAD,

				};
				std::uniform_int_distribution<size_t> typeDist(0, 2);
				SpawnMemoryItem(pos, kDropTypes[typeDist(memRng)]);
			}
		};

		if (collisionManager_)
		{
			collisionManager_->ProcessAllCollisions(colCtx);
		}

		TerrainCollisionDebugger::GetInstance()->EndFrame();

		bool isLockOn = false;
		if (player_ && player_->GetObject3d() && player_->GetReticle() && activeCamera_)
		{
			Vector3 cameraPos = activeCamera_->GetTranslate();
			Vector3 reticlePos = player_->GetReticleWorldPosition();


			Vector3 rayDir = {
				reticlePos.x - cameraPos.x,
				reticlePos.y - cameraPos.y,
				reticlePos.z - cameraPos.z
			};
			float length = std::sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y + rayDir.z * rayDir.z);

			Vector3 lockOnPos = { 0, 0, 0 };
			Enemy* lockOnEnemy = nullptr;

			if (length > 0.0001f)
			{
				rayDir.x /= length;
				rayDir.y /= length;
				rayDir.z /= length;

				Ray ray = { cameraPos, rayDir };

				float bestScore = 1000000.0f;


				for (auto& enemy : enemies_)
				{
					if (enemy->IsDead()) continue;

					float dist = 0.0f;
					bool hit = enemy->CheckRaycast(ray, &dist);

					float maxLockDist = player_ ? player_->GetLockOnMaxDistance() : lockOnMaxDistance_;
					// 最大射程距離以内の敵のみロックオン対象とする
					if (hit && dist <= maxLockDist)
					{
						Vector3 enemyPos = enemy->GetPosition();
						Vector3 toEnemy = { enemyPos.x - cameraPos.x, enemyPos.y - cameraPos.y, enemyPos.z - cameraPos.z };
						float toEnemyLen = std::sqrt(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y + toEnemy.z * toEnemy.z);
						if (toEnemyLen > 0.0f)
						{
							toEnemy.x /= toEnemyLen;
							toEnemy.y /= toEnemyLen;
							toEnemy.z /= toEnemyLen;
						}

						float dot = ray.direction.x * toEnemy.x + ray.direction.y * toEnemy.y + ray.direction.z * toEnemy.z;
						float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

						float score = angle * 100.0f + dist * 0.1f;

						if (score < bestScore)
						{
							bestScore = score;
							lockOnPos = {
								ray.origin.x + ray.direction.x * dist,
								ray.origin.y + ray.direction.y * dist,
								ray.origin.z + ray.direction.z * dist
							};
							isLockOn = true;
							lockOnEnemy = enemy.get();
						}
					}
				}
			}

			float minEnemyDist = 1000000.0f;
			for (auto& enemy : enemies_)
			{
				if (enemy->IsDead()) continue;
				Vector3 ePos = enemy->GetPosition();
				float d = std::sqrt((ePos.x - cameraPos.x) * (ePos.x - cameraPos.x) +
					(ePos.y - cameraPos.y) * (ePos.y - cameraPos.y) +
					(ePos.z - cameraPos.z) * (ePos.z - cameraPos.z));
				if (d < minEnemyDist)
				{
					minEnemyDist = d;
				}
			}

			float maxLockDist = player_ ? player_->GetLockOnMaxDistance() : lockOnMaxDistance_;
			if (isLockOn)
			{
				player_->SetReticleColor({ 1.0f, 0.0f, 0.0f, 1.0f }); // ロックオン時: 赤
				player_->SetLockOn(true, lockOnPos, lockOnEnemy);
			}
			else
			{
				if (minEnemyDist < maxLockDist)
				{
					// 射程内に敵が存在する（接近: オレンジ）
					player_->SetReticleColor({ 1.0f, 0.6f, 0.0f, 1.0f });
				}
				else
				{
					// 射程外（通常: 白）
					player_->SetReticleColor({ 1.0f, 1.0f, 1.0f, 1.0f });
				}
				player_->SetLockOn(false);
			}
		}


		for (auto it = bullets_.begin(); it != bullets_.end(); )
		{
			if ((*it)->IsDead())
			{
				it = bullets_.erase(it);
			}
			else
			{
				++it;
			}
		}


		for (auto it = missiles_.begin(); it != missiles_.end(); )
		{
			if ((*it)->IsDead())
			{
				it = missiles_.erase(it);
			}
			else
			{
				++it;
			}
		}

	}
	else
	{
		for (auto& bullet : bullets_)
		{
			bullet->Update3DObjectOnly();
		}
		for (auto& missile : missiles_)
		{
			missile->Update3DObjectOnly();
		}
		for (auto& enemy : enemies_)
		{
			enemy->Update3DObjectOnly();
		}
	}




}

void GamePlayScene::UpdateCameraShakeAndVfx(float dt)
{
	// 敵撃破時のスコア加算ポップアップ演出の更新
	if (!pauseMenu_.IsPaused())
	{
		hud_.UpdateScorePopups((1.0f / 60.0f) * gameSpeed_);
	}


	if (activeCamera_)
	{
		// 1. 通常の大きなカメラシェイク（被弾・ボス爆発）
		if (cameraShakeTimer_ > 0.0f)
		{
			cameraShakeTimer_ -= 1.0f;
			float p = cameraShakeTimer_ / 20.0f;
			float shakePower = 2.0f * p;
			Vector3 pos = activeCamera_->GetTranslate();
			pos.x += ((rand() % 100) / 50.0f - 1.0f) * shakePower;
			pos.y += ((rand() % 100) / 50.0f - 1.0f) * shakePower;
			activeCamera_->SetTranslate(pos);
		}
		// 2. 敵撃破時のごくわずかなマイクロシェイク（1〜2フレームだけ微小に振動・数ピクセル程度）
		if (enemyKillShakeFrames_ > 0)
		{
			--enemyKillShakeFrames_;
			float killShakePower = 0.22f; // ごく微小（酔わない数ピクセル振動）
			Vector3 pos = activeCamera_->GetTranslate();
			pos.x += ((rand() % 100) / 50.0f - 1.0f) * killShakePower;
			pos.y += ((rand() % 100) / 50.0f - 1.0f) * killShakePower;
			activeCamera_->SetTranslate(pos);
		}
		activeCamera_->Update();
	}
	for (auto& model : modelInstances) if (model) model->Update();
	if (isDrawRail_)
	{
		for (auto& vis : railVisualizers_) if (vis) vis->Update();
		for (auto& vis : enemyRailVisualizers_) if (vis) vis->Update();
	}


	if (!isPlaying_ || pauseMenu_.IsPaused())
	{
		for (auto& enemy : enemies_) if (enemy) enemy->Update3DObjectOnly();
		for (auto& obstacle : obstacles_) if (obstacle) obstacle->Update3DObjectOnly();
		for (auto& ring : enhanceRings_) if (ring) ring->Update();
	}


	Matrix4x4 cameraMatrix = Matrix4x4::Identity();
	Matrix4x4 viewMatrix = Matrix4x4::Identity();
	Matrix4x4 projectionMatrix = Matrix4x4::Identity();
	Matrix4x4 billboardMatrix = Matrix4x4::Identity();

	if (activeCamera_)
	{
		cameraMatrix = activeCamera_->GetWorldMatrix();
		viewMatrix = activeCamera_->GetViewMatrix();
		projectionMatrix = activeCamera_->GetProjectionMatrix();
		billboardMatrix = Billboard::CreateFromCamera(activeCamera_, true);
	}

	if (skybox_)
	{
		skybox_->SetCamera(activeCamera_);
		skybox_->Update();
	}

	if (isPlaying_)
	{

	}


	if (player_ && player_->GetObject3d())
	{
		const Matrix4x4& wMat = player_->GetObject3d()->GetmatWorld();
		Vector3 worldPos = { wMat.m[3][0], wMat.m[3][1], wMat.m[3][2] };

		// タイトルシーンと完全に同一：自機マトリクスに親子付けし、機体姿勢・ロール・傾きに完全追従
		vfx_.Thruster().SetParentMatrix(&player_->GetObject3d()->GetmatWorld());
		vfx_.Thruster().SetPosition({ 0.0f, -0.22f, -3.00f });

		bool shouldEmitThruster = false;

		if (!player_->IsDead() && gamePhase_ != GamePhase::GAMEOVER)
		{
			if (gamePhase_ == GamePhase::START_CUTSCENE)
			{
				// スタート演出中は、機体が復元完了して点火するタイミング（2.50s以降）のみ放出
				if (openingSequence_.GetTimer() >= 2.50f || player_->IsCutsceneBoosting())
				{
					shouldEmitThruster = true;
					// 点火時は力強いブレイジングオレンジ
					vfx_.Thruster().SetBaseColor({ 1.0f, 0.55f, 0.1f, 1.0f });
				}
			}
			else if (gamePhase_ == GamePhase::PLAYING || (gamePhase_ == GamePhase::CLEAR && player_->IsVisible()))
			{
				// 通常プレイ時およびクリア演出・リザルト背景飛行時
				shouldEmitThruster = true;
				// 巡航時はクールな高エネルギーシアンブルー、ブースト時やロール回避時はブレイジングオレンジ
				if (player_->IsBoosting() || player_->IsRolling())
				{
					vfx_.Thruster().SetBaseColor({ 1.0f, 0.55f, 0.1f, 1.0f });
				}
				else
				{
					vfx_.Thruster().SetBaseColor({ 0.5f, 0.85f, 1.0f, 1.0f });
				}
			}

			if (shouldEmitThruster)
			{
				vfx_.Thruster().Play();
				vfx_.Thruster().Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
			}
			else
			{
				// 放出すべきでない期間（カットシーン復元前など）はパーティクルを常にクリアし、frequency自動生成も阻止
				vfx_.Thruster().ClearParticles();
			}
		}
		else
		{
			// 死亡時・ゲームオーバー時はスラスターパーティクルを即座に消去
			vfx_.Thruster().ClearParticles();
		}
		vfx_.UpdateEffects(dt, viewMatrix, projectionMatrix, billboardMatrix);

		// 気流線（スピードライン・風の筋）エフェクトの発生
		// プレイヤーが前進していない時（停止時・カットシーン待機・ゲームオーバー・被弾・リトライ時等）は完全消去
		bool isPlayerMoving = isPlaying_ && !pauseMenu_.IsPaused() &&
		                      (gamePhase_ == GamePhase::PLAYING || 
		                       (gamePhase_ == GamePhase::START_CUTSCENE && openingSequence_.IsThrusterReady())) &&
		                      gameSpeed_ > 0.01f &&
		                      (player_ && !player_->IsDead());

		if (isPlayerMoving)
		{
			bool isBoost = player_ && player_->IsBoosting();
			int spawnCount = isBoost ? 4 : 2;

			// 機体の飛行進路周辺に分散させて発生（照準を遮らないよう自然な範囲に分散）
			std::uniform_real_distribution<float> distOffsetX(-8.0f, 8.0f);
			std::uniform_real_distribution<float> distOffsetY(-3.0f, 4.5f);
			std::uniform_real_distribution<float> distOffsetZ(20.0f, 45.0f);

			for (int i = 0; i < spawnCount; ++i)
			{
				Vector3 windSpawnPos = {
					worldPos.x + distOffsetX(randomEngine),
					worldPos.y + distOffsetY(randomEngine),
					worldPos.z + distOffsetZ(randomEngine)
				};
				vfx_.Wind().SetPosition(windSpawnPos);
				vfx_.Wind().Play();
			}
		}
		else
		{
			// プレイヤーが進んでいない時はスピードラインを即座に完全消去
			vfx_.Wind().ClearParticles();
		}
		vfx_.Wind().Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
	}

}

void GamePlayScene::CheckGameEndConditions()
{
	if (!enemies_.empty())
	{
		hasEnemySpawned_ = true;
	}

	if (player_->GetHp() <= 0)
	{
		if (gamePhase_ == GamePhase::PLAYING)
		{
			StartGameOverSequence();
		}
	}

	// 全通常敵撃破後のクリア判定（ボス戦が控えている・戦闘中の場合はボス撃破までクリアにしない）
	if (hasEnemySpawned_ && enemies_.empty() && gamePhase_ == GamePhase::PLAYING)
	{
		if (bossBattle_.GetStep() == BossBattleController::Step::FINISHED || (bossBattle_.GetStep() == BossBattleController::Step::NOT_ACTIVE && !armoredTrainBoss_))
		{
			StartClearSequence();
		}
	}
}

void GamePlayScene::Draw()
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();
	auto spriteCommon = services->GetSpriteCommon();

	if (skybox_)
	{
		skybox_->Draw();
	}

	// 視錐台（Frustum）カリング用の情報取得
	Frustum frustum{};
	bool enableCulling = false;
	if (activeCamera_)
	{
		Matrix4x4 vp = activeCamera_->GetViewMatrix() * activeCamera_->GetProjectionMatrix();
		frustum = Frustum::CreateFromViewProjection(vp);
		enableCulling = true;
	}

	if (object3dCommon) object3dCommon->SetCommonDrawSetting();

	for (auto& model : modelInstances)
	{
		if (!model) continue;

		if (enableCulling)
		{
			Vector3 center;
			float radius = 0.0f;
			if (model->GetBoundingSphere(center, radius))
			{
				// 半径が極端に巨大なオブジェクト（広域背景など）以外は視錐台カリング
				if (radius < 300.0f && !frustum.ContainsSphere(center, radius))
				{
					continue; // 画面外のため描画スキップ
				}
			}
		}

		model->Draw();
	}

	// スタート演出中のオブジェクト実体化判定（地形が実体化した領域の敵やリングのみ出現・描画）
	auto IsMaterializedAt = [&](const Vector3& pos) -> bool {
		if (gamePhase_ != GamePhase::START_CUTSCENE)
		{
			return true;
		}
		const float kPhase1Duration = 2.20f;
		if (openingSequence_.GetTimer() >= kPhase1Duration)
		{
			return true; // Phase 1 完了後はすべて出現
		}

		// シェーダー側の実体化波（solidT）と完全に同一の進行度計算
		float solidT = std::clamp((terrainMaterializeProgress_ - 0.54f) / 0.46f, 0.0f, 1.0f);
		if (solidT <= 0.0001f)
		{
			return false; // まだ実体化波がスタートしていない（ワイヤーフレーム段階では一切非表示）
		}

		float scanDistSolid = -60.0f + (850.0f - (-60.0f)) * solidT;

		Vector3 camPos = activeCamera_ ? activeCamera_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
		float zDist = pos.z - camPos.z;
		float heightDelay = std::max(0.0f, pos.y) * 1.5f;
		float scanCoord = zDist + heightDelay;

		return (scanCoord <= scanDistSolid);
	};

	if (object3dCommon) object3dCommon->SetCommonDrawSetting();
	for (auto& enemy : enemies_)
	{
		if (gameOverSequence_.IsRetryDematerializing()) continue; // 地形消滅演出中は敵を完全非表示
		if (gamePhase_ == GamePhase::START_CUTSCENE && !IsMaterializedAt(enemy->GetPosition()))
		{
			continue; // 地形実体化前の敵は非表示
		}
		if (enableCulling)
		{
			Vector3 ePos = enemy->GetPosition();
			if (!frustum.ContainsSphere(ePos, 20.0f))
			{
				continue; // 画面外スキップ
			}
		}
		enemy->Draw();
	}
	if (armoredTrainBoss_ && (isBossSpawned_ || (bossFinisherSequence_ && bossFinisherSequence_->IsActive())) && !gameOverSequence_.IsRetryDematerializing())
	{
		if (gamePhase_ != GamePhase::START_CUTSCENE || IsMaterializedAt(armoredTrainBoss_->GetPosition()))
		{
			armoredTrainBoss_->Draw();
			bossBattle_.Draw3D();
		}
	}
	for (auto& obstacle : obstacles_)
	{
		if (enableCulling)
		{
			Vector3 oPos = obstacle->GetPosition();
			if (!frustum.ContainsSphere(oPos, 25.0f))
			{
				continue; // 画面外スキップ
			}
		}
		obstacle->Draw();
	}
	for (auto& ring : enhanceRings_)
	{
		if (gameOverSequence_.IsRetryDematerializing()) continue; // 地形消滅演出中はリングを完全非表示
		if (gamePhase_ == GamePhase::START_CUTSCENE && !IsMaterializedAt(ring->GetPosition()))
		{
			continue; // 地形実体化前の領域は非表示
		}
		if (enableCulling)
		{
			Vector3 rPos = ring->GetPosition();
			if (!frustum.ContainsSphere(rPos, 20.0f))
			{
				continue; // 画面外スキップ
			}
		}
		ring->Draw();
	}

	// メモリドロップアイテムの描画
	for (auto& item : memoryItems_)
	{
		if (item) item->Draw();
	}


	if (isDrawCollider_ || isDrawTerrainWireframe_)
	{
		if (object3dCommon) object3dCommon->SetWireframeDrawSetting();

		if (isDrawCollider_)
		{
			// 障害物のコライダー描画
			for (auto& obstacle : obstacles_)
			{
				obstacle->DrawCollider();
			}
			for (auto& enemy : enemies_)
			{
				if (gamePhase_ == GamePhase::START_CUTSCENE && !IsMaterializedAt(enemy->GetPosition())) continue;
				enemy->DrawCollider();
			}
			if (armoredTrainBoss_ && isBossSpawned_)
			{
				if (gamePhase_ != GamePhase::START_CUTSCENE || IsMaterializedAt(armoredTrainBoss_->GetPosition()))
				{
					armoredTrainBoss_->DrawCollider();
				}
			}
			for (auto& ring : enhanceRings_)
			{
				if (gamePhase_ == GamePhase::START_CUTSCENE && !IsMaterializedAt(ring->GetPosition())) continue;
				ring->DrawCollider();
			}
			for (auto& bullet : bullets_)
			{
				bullet->DrawCollider();
			}
			for (auto& missile : missiles_)
			{
				missile->DrawCollider();
			}

			#if 0
		Vector3 bombPos = { 0.0f, 0.0f, 0.0f };
		float bombRadius = 0.0f;
		bool isBombClearing = player_ && player_->HasActiveBombExplosion(&bombPos, &bombRadius);

		for (auto it = enemyBullets_.begin(); it != enemyBullets_.end(); )
		{
			if (isBombClearing)
			{
				Vector3 ebPos = (*it)->GetPosition();
				float bdx = ebPos.x - bombPos.x;
				float bdy = ebPos.y - bombPos.y;
				float bdz = ebPos.z - bombPos.z;
				if (bdx * bdx + bdy * bdy + bdz * bdz <= bombRadius * bombRadius)
				{
					it = enemyBullets_.erase(it);
					continue;
				}
			}
			(*it)->Update(gameSpeed_);
			if ((*it)->IsDead())
			{
				it = enemyBullets_.erase(it);
			}
			else
			{
				++it;
			}
		}
#endif
		for (auto& bullet : enemyBullets_)
			{
				bullet->DrawCollider();
			}
			if (player_)
			{
				player_->DrawCollider();
			}
		}

		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
	}


	for (auto& bullet : bullets_)
	{
		bullet->Draw();
	}
	for (auto& missile : missiles_)
	{
		missile->Draw();
	}
	for (auto& bullet : enemyBullets_)
	{
		bullet->Draw();
	}


	if (player_)
	{
		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
		player_->Draw();

		// ゲームオーバー破片プリズム描画
		if (gamePhase_ == GamePhase::GAMEOVER)
		{
			gameOverSequence_.Draw3D(activeCamera_, object3dCommon);
		}

		// スタート演出中の自機復元ポリゴン破片描画（Phase 2: 自機集束フェーズ 2.20s〜3.05s のみ描画）
		if (gamePhase_ == GamePhase::START_CUTSCENE)
		{
			openingSequence_.Draw3D(activeCamera_, object3dCommon);
		}
	}


	if (isDrawRail_)
	{
		if (object3dCommon) object3dCommon->SetCommonDrawSetting();
		for (auto& vis : railVisualizers_)
		{
			if (vis) vis->Draw();
		}
		for (auto& vis : enemyRailVisualizers_)
		{
			if (vis) vis->Draw();
		}
	}

	bool drawThruster = (gamePhase_ != GamePhase::START_CUTSCENE || openingSequence_.IsThrusterReady() || (player_ && player_->IsCutsceneBoosting())) && gamePhase_ != GamePhase::GAMEOVER;
	bool drawWind = (gamePhase_ == GamePhase::PLAYING || (gamePhase_ == GamePhase::START_CUTSCENE && openingSequence_.IsThrusterReady()));
	vfx_.Draw(drawThruster, drawWind, object3dCommon);

	// 地形コリジョンデバッグ描画（判定の計算が行われている部分のみ描画）
	if (isDrawTerrainWireframe_ || isDrawCollider_)
	{
		TerrainCollisionDebugger::GetInstance()->SetDebugEnabled(true);
		TerrainCollisionDebugger::GetInstance()->Draw(activeCamera_);
	}
	else
	{
		TerrainCollisionDebugger::GetInstance()->SetDebugEnabled(false);
	}

	// 電脳フィニッシャー演出（弱点コア、ミサイル、崩壊シャード等）の3D描画
	if (bossFinisherSequence_ && bossFinisherSequence_->IsActive())
	{
		auto services = EngineServices::GetInstance();
		auto obj3dCommon = services ? services->GetObject3dCommon() : nullptr;
		if (obj3dCommon) obj3dCommon->SetCommonDrawSetting();
		auto srvMgr = services ? services->GetSrvManager() : nullptr;
		if (srvMgr) srvMgr->PreDraw();
		bossFinisherSequence_->Draw3D();
	}
}

void GamePlayScene::DrawUI()
{
	auto services = EngineServices::GetInstance();
	auto spriteCommon = services->GetSpriteCommon();
	if (spriteCommon) spriteCommon->SetCommonDrawSetting();
	auto srvManager = services->GetSrvManager();
	if (srvManager) srvManager->PreDraw();

	// 通常プレイ時のみHPバー、ブーストバー、強化リングアイコンを描画（スタート演出中・ゲームオーバー・クリア時・処刑ムービー中は非表示）
	bool isFinisherMovie = (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase());
	if (gamePhase_ == GamePhase::PLAYING && !isFinisherMovie)
	{
		hud_.DrawSprites();
		if (isDisplaySprite) { for (auto& sprite : sprites) if (sprite) { sprite->Update(); sprite->Draw(); } }

		// ボス専用HPバースプライト（戦闘中・撃破演出中）
		bossBattle_.DrawHpSprites(armoredTrainBoss_ ? armoredTrainBoss_->GetTotalHpRate() : 0.0f);
	}

	// 通常プレイ中のレティクル・ロックオンマーカー・スコアポップアップ描画
	hud_.DrawOverlayUI(gamePhase_ == GamePhase::PLAYING, (bossFinisherSequence_ && bossFinisherSequence_->IsMoviePhase()), player_.get(), activeCamera_);

	// スタート演出用UIテキストの描画
	openingSequence_.DrawUI(gamePhase_ == GamePhase::START_CUTSCENE, gamePhase_ == GamePhase::PLAYING);

	// ボス戦用UI（WARNING警告・詳細ボスHPバー・各車両インジケーター）の描画
	bossBattle_.DrawUI(gamePhase_ == GamePhase::START_CUTSCENE, gamePhase_ == GamePhase::GAMEOVER, gamePhase_ == GamePhase::CLEAR, BuildBossBattleContext());

	// ゲームオーバー演出用UIテキストの描画
	gameOverSequence_.DrawUI(gamePhase_ == GamePhase::GAMEOVER, GetSceneManager());

	// ゲームクリア演出用UIテキストの描画
	clearSequence_.DrawUI(gamePhase_ == GamePhase::CLEAR, BuildClearContext());

	// ポーズメニューUIの描画
	pauseMenu_.DrawUI();

	// 電脳フィニッシャー演出用UIの描画
	if (bossFinisherSequence_)
	{
		bossFinisherSequence_->DrawUI();
	}
}

OpeningCutsceneContext GamePlayScene::BuildOpeningContext()
{
	OpeningCutsceneContext ctx;
	ctx.player = player_.get();
	ctx.activeCamera = activeCamera_;
	ctx.railCameraController = railCameraController_.get();
	ctx.modelInstances = &modelInstances;
	ctx.obstacles = &obstacles_;
	ctx.thrusterEffect = &vfx_.Thruster();
	ctx.windEffect = &vfx_.Wind();
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.terrainMaterializeProgress = &terrainMaterializeProgress_;
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	return ctx;
}

void GamePlayScene::StartOpeningCutscene()
{
	gamePhase_ = GamePhase::START_CUTSCENE;
	openingSequence_.Start(BuildOpeningContext());
}

void GamePlayScene::UpdateOpeningCutscene(float dt)
{
	if (openingSequence_.Update(dt, BuildOpeningContext()))
	{
		gamePhase_ = GamePhase::PLAYING;
	}
}

GameOverContext GamePlayScene::BuildGameOverContext()
{
	GameOverContext ctx;
	ctx.player = player_.get();
	ctx.activeCamera = activeCamera_;
	ctx.railCameraController = railCameraController_.get();
	ctx.sceneManager = GetSceneManager();
	ctx.modelInstances = &modelInstances;
	ctx.obstacles = &obstacles_;
	ctx.enemies = &enemies_;
	ctx.armoredTrainBoss = armoredTrainBoss_.get();
	ctx.bullets = &bullets_;
	ctx.missiles = &missiles_;
	ctx.enemyBullets = &enemyBullets_;
	ctx.enhanceRings = &enhanceRings_;
	ctx.thrusterEffect = &vfx_.Thruster();
	ctx.windEffect = &vfx_.Wind();
	ctx.terrainMaterializeProgress = &terrainMaterializeProgress_;
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.demoDeathLoopCount = &demoDeathLoopCount_;
	ctx.demoSurvivalTimer = &demoSurvivalTimer_;
	ctx.demoBulletSpawned = &demoBulletSpawned_;
	ctx.demoHitTriggered = &demoHitTriggered_;
	ctx.demoGameOverWaitTimer = &demoGameOverWaitTimer_;
	ctx.demoMenuSelectStep = &demoMenuSelectStep_;
	ctx.onRetry = [this]() { SeamlessResetForRetry(); };
	return ctx;
}

void GamePlayScene::StartGameOverSequence()
{
	gamePhase_ = GamePhase::GAMEOVER;
	gameOverSequence_.Start(BuildGameOverContext());
}

void GamePlayScene::UpdateGameOverSequence(float dt)
{
	gameOverSequence_.Update(dt, BuildGameOverContext());
}

GameClearContext GamePlayScene::BuildClearContext()
{
	GameClearContext ctx;
	ctx.player = player_.get();
	ctx.railCameraController = railCameraController_.get();
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.gameOverSequence = &gameOverSequence_;
	ctx.enemyBullets = &enemyBullets_;
	ctx.sceneManager = GetSceneManager();
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.totalEnemiesCount = totalEnemiesCount_;
	ctx.killedEnemiesCount = killedEnemiesCount_;
	ctx.stageElapsedTime = stageElapsedTime_;
	ctx.bossElapsedTime = bossElapsedTime_;
	return ctx;
}

void GamePlayScene::StartClearSequence()
{
	if (gamePhase_ != GamePhase::PLAYING) return;
	gamePhase_ = GamePhase::CLEAR;
	clearSequence_.Start(BuildClearContext());
}

void GamePlayScene::UpdateClearSequence(float dt)
{
	clearSequence_.Update(dt, BuildClearContext());
}

BossBattleContext GamePlayScene::BuildBossBattleContext()
{
	BossBattleContext ctx;
	ctx.mainRails = &mainRails_;
	ctx.bossRail = bossRail_.get();
	ctx.railCameraController = railCameraController_.get();
	ctx.gameSpeed = &gameSpeed_;
	ctx.explosionEffect = &vfx_.Explosion();
	ctx.player = player_.get();
	ctx.bullets = &bullets_;
	ctx.missiles = &missiles_;
	ctx.armoredTrainBoss = armoredTrainBoss_.get();
	ctx.isBossSpawned = &isBossSpawned_;
	ctx.bossFinisherSequence = bossFinisherSequence_.get();
	ctx.cameraShakeTimer = &cameraShakeTimer_;
	ctx.bossElapsedTime = &bossElapsedTime_;
	ctx.onGameOver = [this]() { if (gamePhase_ == GamePhase::PLAYING) StartGameOverSequence(); };
	ctx.onClear = [this]() { StartClearSequence(); };
	return ctx;
}

void GamePlayScene::TriggerTimeOverCannonSequence(bool nonLethal)
{
	gamePhase_ = GamePhase::PLAYING;
	bossBattle_.TriggerTimeOverCannonSequence(nonLethal, BuildBossBattleContext());
}

void GamePlayScene::StartBossWarningSequence()
{
	bossBattle_.StartWarningSequence(BuildBossBattleContext());
}

void GamePlayScene::UpdateBossBattle(float dt)
{
	bossBattle_.Update(dt, BuildBossBattleContext());
}

void GamePlayScene::SpawnMemoryItem(const Vector3& pos, MemoryType type)
{
	auto object3dCommon = EngineServices::GetInstance()->GetObject3dCommon();
	auto item = std::make_unique<MemoryItem>();
	item->Initialize(object3dCommon, pos, type);
	memoryItems_.push_back(std::move(item));
}
