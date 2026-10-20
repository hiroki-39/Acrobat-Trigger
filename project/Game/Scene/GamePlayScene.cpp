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
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
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


	// HPバー
	hpBarBgSprite_ = std::make_unique<Sprite>();
	if (hpBarBgSprite_)
	{
		hpBarBgSprite_->Initialize(spriteCommon, whiteTexIndex_);
		hpBarBgSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		hpBarBgSprite_->SetPosition(Vector2(20.0f, 720.0f - 32.0f - 20.0f));
		hpBarBgSprite_->SetSize(Vector2(400.0f, 32.0f));
		hpBarBgSprite_->SetColor(Vector4(0.8f, 0.8f, 0.8f, 0.8f));
		hpBarBgSprite_->Update();
	}

	hpBarSprite_ = std::make_unique<Sprite>();
	if (hpBarSprite_)
	{
		hpBarSprite_->Initialize(spriteCommon, whiteTexIndex_);
		hpBarSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		hpBarSprite_->SetPosition(Vector2(20.0f, 720.0f - 32.0f - 20.0f));
		hpBarSprite_->SetSize(Vector2(400.0f, 32.0f));
		hpBarSprite_->SetColor(Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		hpBarSprite_->Update();
	}

	// ブーストゲージバー（HPバーの上側に配置: Y = 668 - 14 - 6 = 648）
	boostBarBgSprite_ = std::make_unique<Sprite>();
	if (boostBarBgSprite_)
	{
		boostBarBgSprite_->Initialize(spriteCommon, whiteTexIndex_);
		boostBarBgSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		boostBarBgSprite_->SetPosition(Vector2(20.0f, 646.0f));
		boostBarBgSprite_->SetSize(Vector2(400.0f, 14.0f));
		boostBarBgSprite_->SetColor(Vector4(0.2f, 0.25f, 0.35f, 0.75f));
		boostBarBgSprite_->Update();
	}

	boostBarSprite_ = std::make_unique<Sprite>();
	if (boostBarSprite_)
	{
		boostBarSprite_->Initialize(spriteCommon, whiteTexIndex_);
		boostBarSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		boostBarSprite_->SetPosition(Vector2(20.0f, 646.0f));
		boostBarSprite_->SetSize(Vector2(400.0f, 14.0f));
		boostBarSprite_->SetColor(Vector4(0.0f, 0.75f, 1.0f, 1.0f));
		boostBarSprite_->Update();
	}

	// 強化リング獲得アイコン（ブーストゲージの上部に横2つ配置）
	// ブーストバー: Y = 646.0f -> アイコンサイズ 36x36、Y = 646 - 36 - 6 = 604.0f
	float ringIconSize = 36.0f;
	float ringIconY = 646.0f - ringIconSize - 6.0f; // 604.0f
	float ringIconSpacing = 8.0f;
	float ringIconStartX = 20.0f;

	for (int i = 0; i < kMaxEnhanceRingIcons; ++i)
	{
		float iconX = ringIconStartX + i * (ringIconSize + ringIconSpacing);

		// 中身アイコン（獲得時に表示）
		ringGetIconSprites_[i] = std::make_unique<Sprite>();
		if (ringGetIconSprites_[i])
		{
			ringGetIconSprites_[i]->Initialize(spriteCommon, ringIconTex_);
			ringGetIconSprites_[i]->SetAnchorPoint(Vector2(0.0f, 0.0f));
			ringGetIconSprites_[i]->SetPosition(Vector2(iconX, ringIconY));
			ringGetIconSprites_[i]->SetSize(Vector2(ringIconSize, ringIconSize));
			ringGetIconSprites_[i]->SetColor(ringIconColor_);
			ringGetIconSprites_[i]->Update();
		}

		// 枠アイコン（常時表示、描画順は中身の上）
		ringGetOutlineSprites_[i] = std::make_unique<Sprite>();
		if (ringGetOutlineSprites_[i])
		{
			ringGetOutlineSprites_[i]->Initialize(spriteCommon, ringIconOutlineTex_);
			ringGetOutlineSprites_[i]->SetAnchorPoint(Vector2(0.0f, 0.0f));
			ringGetOutlineSprites_[i]->SetPosition(Vector2(iconX, ringIconY));
			ringGetOutlineSprites_[i]->SetSize(Vector2(ringIconSize, ringIconSize));
			ringGetOutlineSprites_[i]->SetColor(ringOutlineColor_);
			ringGetOutlineSprites_[i]->Update();
		}
	}
	acquiredEnhanceRingCount_ = 0;

	// ボス専用UIスプライトの初期化（幅600px、画面上部中央 Y=35）
	float bossBarW = 600.0f;
	float bossBarH = 18.0f;
	float bossBarX = (1280.0f - bossBarW) * 0.5f;
	float bossBarY = 35.0f;

	bossHpBarBgSprite_ = std::make_unique<Sprite>();
	if (bossHpBarBgSprite_)
	{
		bossHpBarBgSprite_->Initialize(spriteCommon, whiteTexIndex_);
		bossHpBarBgSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossHpBarBgSprite_->SetPosition(Vector2(bossBarX, bossBarY));
		bossHpBarBgSprite_->SetSize(Vector2(bossBarW, bossBarH));
		bossHpBarBgSprite_->SetColor(Vector4(0.12f, 0.14f, 0.18f, 0.85f));
		bossHpBarBgSprite_->Update();
	}

	bossHpBarDelaySprite_ = std::make_unique<Sprite>();
	if (bossHpBarDelaySprite_)
	{
		bossHpBarDelaySprite_->Initialize(spriteCommon, whiteTexIndex_);
		bossHpBarDelaySprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossHpBarDelaySprite_->SetPosition(Vector2(bossBarX, bossBarY));
		bossHpBarDelaySprite_->SetSize(Vector2(bossBarW, bossBarH));
		bossHpBarDelaySprite_->SetColor(Vector4(1.0f, 0.85f, 0.2f, 0.9f));
		bossHpBarDelaySprite_->Update();
	}

	bossHpBarSprite_ = std::make_unique<Sprite>();
	if (bossHpBarSprite_)
	{
		bossHpBarSprite_->Initialize(spriteCommon, whiteTexIndex_);
		bossHpBarSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossHpBarSprite_->SetPosition(Vector2(bossBarX, bossBarY));
		bossHpBarSprite_->SetSize(Vector2(bossBarW, bossBarH));
		bossHpBarSprite_->SetColor(Vector4(0.95f, 0.2f, 0.15f, 1.0f));
		bossHpBarSprite_->Update();
	}

	// WARNING演出用スプライト（上下の赤色帯、画面全体フラッシュ）
	bossWarningBandTopSprite_ = std::make_unique<Sprite>();
	if (bossWarningBandTopSprite_)
	{
		bossWarningBandTopSprite_->Initialize(spriteCommon, whiteTexIndex_);
		bossWarningBandTopSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossWarningBandTopSprite_->SetPosition(Vector2(0.0f, 0.0f));
		bossWarningBandTopSprite_->SetSize(Vector2(1280.0f, 44.0f));
		bossWarningBandTopSprite_->SetColor(Vector4(0.85f, 0.1f, 0.1f, 0.85f));
		bossWarningBandTopSprite_->Update();
	}

	bossWarningBandBottomSprite_ = std::make_unique<Sprite>();
	if (bossWarningBandBottomSprite_)
	{
		bossWarningBandBottomSprite_->Initialize(spriteCommon, whiteTexIndex_);
		bossWarningBandBottomSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossWarningBandBottomSprite_->SetPosition(Vector2(0.0f, 720.0f - 44.0f));
		bossWarningBandBottomSprite_->SetSize(Vector2(1280.0f, 44.0f));
		bossWarningBandBottomSprite_->SetColor(Vector4(0.85f, 0.1f, 0.1f, 0.85f));
		bossWarningBandBottomSprite_->Update();
	}

	bossWarningFlashSprite_ = std::make_unique<Sprite>();
	if (bossWarningFlashSprite_)
	{
		bossWarningFlashSprite_->Initialize(spriteCommon, whiteTexIndex_);
		bossWarningFlashSprite_->SetAnchorPoint(Vector2(0.0f, 0.0f));
		bossWarningFlashSprite_->SetPosition(Vector2(0.0f, 0.0f));
		bossWarningFlashSprite_->SetSize(Vector2(1280.0f, 720.0f));
		bossWarningFlashSprite_->SetColor(Vector4(1.0f, 0.1f, 0.1f, 0.0f));
		bossWarningFlashSprite_->Update();
	}

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

	thrusterEffect_.Initialize(dxCommon, srvManager);
	thrusterEffect_.LoadFromJson("thruster.json");

	explosionEffect_.Initialize(dxCommon, srvManager);
	explosionEffect_.LoadFromJson("explosion.json");

	hitEffect_.Initialize(dxCommon, srvManager);
	hitEffect_.LoadFromJson("hit.json");


	dodgeEffect_.Initialize(dxCommon, srvManager);
	dodgeEffect_.LoadFromJson("dodge.json");

	trailEffect_.Initialize(dxCommon, srvManager);
	trailEffect_.LoadFromJson("trail.json");

	missileSmokeEffect_.Initialize(dxCommon, srvManager);
	missileSmokeEffect_.LoadFromJson("missile_smoke.json");

	ringEffect_.Initialize(dxCommon, srvManager);
	ringEffect_.LoadFromJson("ring.json");

	healRingEffect_.Initialize(dxCommon, srvManager);
	healRingEffect_.LoadFromJson("heal_ring.json");

	windEffect_.Initialize(dxCommon, srvManager);
	windEffect_.LoadFromJson("wind.json");

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
	gameOverStep_ = GameOverStep::FALLING;
	gameOverTimer_ = 0.0f;
	if (player_)
	{
		player_->SetVisible(true);
	}

	// ステージ開始時の降下演出を開始
	StartOpeningCutscene();

	// ゲームプレイBGMの再生（ループ再生、音量0.25）
	SoundManager::GetInstance()->PlayBGM("gameplayBGM.mp3", 0.25f, true);
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
	mainRails_.clear();
	bossRail_.reset();
	if (railCameraController_)
	{
		railCameraController_->Reset();
	}
	acquiredEnhanceRingCount_ = 0;
	if (player_)
	{
		player_->SetPowerUpLevel(0);
	}

	// ボス戦状態のリセット
	bossBattleStep_ = BossBattleStep::NOT_ACTIVE;
	isBossSpawned_ = false;
	bossWarningTimer_ = 0.0f;
	bossDefeatTimer_ = 0.0f;
	bossDisplayedHpRate_ = 1.0f;
	bossWarningSirenTimer_ = 0.0f;
	if (armoredTrainBoss_)
	{
		armoredTrainBoss_->SetActive(false);
	}


	auto levelData = LevelLoader::Load("resources/json/maps/template/template.json");
	if (levelData)
	{
		for (const auto& objData : levelData->objects)
		{
			CreateObjectFromNode(objData, nullptr, modelInstances, mainRails_, bossRail_, bossSpawnProgressThreshold_, object3dCommon, skybox_->GetCubemapSrvIndex(), enemies_, obstacles_, enhanceRings_);
		}
		ApplyVirtualSpaceStyle();
		OutputDebugStringA("LevelLoader: Successfully reloaded objects.\n");

		// BossRailが存在する場合、マップからの指定がなければ始点近傍のメインレール進行度を自動算出
		if (bossRail_ && bossRail_->IsValid() && !mainRails_.empty() && mainRails_[0]->IsValid())
		{
			if (bossSpawnProgressThreshold_ == 0.60f)
			{
				Vector3 bossStartPos = bossRail_->GetPosition(0.0f);
				bossSpawnProgressThreshold_ = mainRails_[0]->GetClosestProgress(bossStartPos);
			}
		}

		// 保存された設定ファイル（boss_settings.json）があれば読み込み（手動保存設定を優先）
		LoadBossSettings();
		if (armoredTrainBoss_)
		{
			armoredTrainBoss_->SetTerrainObjects(&modelInstances);
		}


		if (railCameraController_)
		{
			std::vector<Rail*> railsRaw;
			for (auto& r : mainRails_)
			{
				railsRaw.push_back(r.get());
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


void GamePlayScene::Finalize()
{
	// BGMおよび効果音の停止
	SoundManager::GetInstance()->StopBGM();
	SoundManager::GetInstance()->StopAllSE();

	sprites.clear();
	modelInstances.clear();

	if (auto pp = EngineServices::GetInstance()->GetPostProcess())
	{
		pp->SetEffectActive("Grayscale", false);
		pp->SetEffectActive("Vignetting", false);
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

	// デモプレイ中：ゲームプレイ部分は見せず、演出・画面遷移を見せた後タイトルシーンへ戻る
	if (services && services->IsDemoMode())
	{
		demoTimer_ += dt;
		// 画面遷移（明転約0.9秒）完了後、ゲーム画面を約1.6秒見せてからタイトルへ戻る（合計2.5秒）
		if (demoTimer_ >= 2.5f)
		{
			auto sceneManager = GetSceneManager();
			if (sceneManager && !sceneManager->IsTransitioning())
			{
				float fadeDur = sceneManager->GetFadeDuration();
				Vector4 fadeCol = sceneManager->GetFadeColor();
				sceneManager->ChangeScene("TITLE", fadeDur, fadeCol);
			}
		}
	}
	else
	{
		demoTimer_ = 0.0f;
	}

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


		if (isPlaying_)
		{

			auto pp = EngineServices::GetInstance()->GetPostProcess();
			float unscaledGameSpeed = baseGameSpeed_;
			if (player_ && player_->IsBoosting())
			{
				unscaledGameSpeed = baseGameSpeed_ * 1.5f;
				thrusterEffect_.SetBaseColor({ 1.0f, 0.2f, 0.0f, 1.0f });
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
				thrusterEffect_.SetBaseColor({ 1.0f, 1.0f, 1.0f, 1.0f });
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
			else if (missionStartTextTimer_ > 0.0f)
			{
				missionStartTextTimer_ -= dt;
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
				if (gamePhase_ == GamePhase::CLEAR)
				{
					// クリア演出中は自機が上空奥へ離脱するため、レールカメラ更新用の自機位置を固定して過度な追従を防止
					cameraTrackPos = clearStartPlayerPos_;
				}
				railCameraController_->Update(gameSpeed_, cameraTrackPos);
			}

			// レール進行度監視によるボス自動出現トリガー
			if (gamePhase_ == GamePhase::PLAYING && bossBattleStep_ == BossBattleStep::NOT_ACTIVE && railCameraController_)
			{
				if (railCameraController_->GetProgress() >= bossSpawnProgressThreshold_)
				{
					StartBossWarningSequence();
				}
			}

			// ボス戦全体の更新（WARNING警告演出・HPバー追従・撃破シークエンス）
			UpdateBossBattle(dt);
		}
	}


	if (player_)
	{
		if (isPlaying_ && gamePhase_ != GamePhase::GAMEOVER && gamePhase_ != GamePhase::CLEAR)
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

				dodgeEffect_.SetPosition(effectPos);
				dodgeEffect_.Play();
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

						trailEffect_.SetPosition(lPos);
						trailEffect_.Play();

						trailEffect_.SetPosition(rPos);
						trailEffect_.Play();
					}
				}
				else
				{
					trailEffect_.SetPosition(leftWing);
					trailEffect_.Play();

					trailEffect_.SetPosition(rightWing);
					trailEffect_.Play();
				}
			}
		}
		else
		{

			player_->Update3DObjectOnly();
		}
	}


	if (isPlaying_)
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
				missileSmokeEffect_.SetPosition((*it)->GetPosition());
				missileSmokeEffect_.Play();
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

		if (armoredTrainBoss_ && isBossSpawned_)
		{
			Vector3 camPos = activeCamera_ ? activeCamera_->GetTranslate() : Vector3{ 0,0,0 };
			float playerSpeed = 50.0f;
			if (railCameraController_)
			{
				playerSpeed = railCameraController_->GetBaseSpeed() * railCameraController_->GetSpeedMultiplier();
			}
			armoredTrainBoss_->Update(camPos, cameraForward, playerSpeed, player_.get(), enemyBullets_, gameSpeed_);
		}

		for (auto& bullet : enemyBullets_)
		{
			bullet->Update(gameSpeed_);
		}

		for (auto& ring : enhanceRings_)
		{
			ring->Update();
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

		colCtx.hitEffect = &hitEffect_;
		colCtx.explosionEffect = &explosionEffect_;
		colCtx.dodgeEffect = &dodgeEffect_;
		colCtx.ringEffect = &ringEffect_;
		colCtx.healRingEffect = &healRingEffect_;

		colCtx.score = &score_;
		colCtx.cameraShakeTimer = &cameraShakeTimer_;
		colCtx.isJustDodgeActive = &isJustDodgeActive_;
		colCtx.justDodgeTimer = &justDodgeTimer_;
		colCtx.justDodgeMaxTime = justDodgeMaxTime_;
		colCtx.acquiredEnhanceRingCount = &acquiredEnhanceRingCount_;
		colCtx.maxEnhanceRingIcons = kMaxEnhanceRingIcons;

		colCtx.onBossDefeated = [this]() {
			if (bossBattleStep_ == BossBattleStep::BATTLE)
			{
				bossBattleStep_ = BossBattleStep::DEFEATED_SEQUENCE;
				bossDefeatTimer_ = 0.0f;
				score_ += 5000; // ボス撃破ボーナス
				SoundManager::GetInstance()->PlaySE("explosion.mp3", 1.0f);
				cameraShakeTimer_ = 1.0f;
			}
		};

		colCtx.onEnemyKilled = [this](const Vector3& pos, int addScore) {
			// 1. 敵撃破時のごくわずかな微小カメラシェイク（1〜2フレームだけ数ピクセル振動）
			enemyKillShakeFrames_ = 2;
			// 2. 敵撃破座標にスコア加算ポップアップ演出を登録
			scorePopups_.push_back({ pos, addScore, 0.0f, 0.65f });
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


	if (input && input->TriggerKey(DIK_ESCAPE))
	{
		auto dxCommon = services->GetDirectXCommon();
		if (dxCommon)
		{
			WinApp* winApp = dxCommon->GetWinApp();
			if (winApp)
			{
				::PostMessage(winApp->GetHwnd(), WM_CLOSE, 0, 0);
			}
		}
	}


	// 敵撃破時のスコア加算ポップアップ演出の更新
	for (auto it = scorePopups_.begin(); it != scorePopups_.end();)
	{
		it->timer += (1.0f / 60.0f) * gameSpeed_;
		if (it->timer >= it->duration) {
			it = scorePopups_.erase(it);
		} else {
			++it;
		}
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


	if (!isPlaying_)
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
		thrusterEffect_.SetParentMatrix(&player_->GetObject3d()->GetmatWorld());
		thrusterEffect_.SetPosition({ 0.0f, -0.22f, -3.00f });

		if (!player_->IsDead())
		{
			// 巡航時はクールな高エネルギーシアンブルー、ブースト時やロール回避時はブレイジングオレンジ
			if (player_->IsBoosting() || player_->IsRolling())
			{
				thrusterEffect_.SetBaseColor({ 1.0f, 0.55f, 0.1f, 1.0f });
			}
			else
			{
				thrusterEffect_.SetBaseColor({ 0.5f, 0.85f, 1.0f, 1.0f });
			}
			thrusterEffect_.Play();
		}

		thrusterEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		explosionEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		hitEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		dodgeEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		trailEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		missileSmokeEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		ringEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
		healRingEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);

		// 気流線（風の筋）エフェクトの発生（自機前方から奥へと通り過ぎる白い空気抵抗パーティクル）
		if (isPlaying_)
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
				windEffect_.SetPosition(windSpawnPos);
				windEffect_.Play();
			}
		}
		windEffect_.Update(dt, viewMatrix, projectionMatrix, billboardMatrix);
	}

	// --- ゲームループ・UI更新 (HPバー・シーン終了判定) ---
	if (player_)
	{
		if (hpBarSprite_)
		{
			float hpRate = (float)player_->GetHp() / player_->GetMaxHp();
			if (hpRate < 0.0f) hpRate = 0.0f;
			hpBarSprite_->SetSize(Vector2(400.0f * hpRate, 32.0f));

			// HPに応じて色を変更
			if (hpRate > 0.5f)
			{
				hpBarSprite_->SetColor(Vector4(0.0f, 1.0f, 0.0f, 1.0f)); // 緑
			}
			else if (hpRate > 0.2f)
			{
				hpBarSprite_->SetColor(Vector4(1.0f, 1.0f, 0.0f, 1.0f)); // 黄色
			}
			else
			{
				hpBarSprite_->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤
			}
			hpBarSprite_->Update();
		}

		if (boostBarSprite_)
		{
			float boostRatio = player_->GetBoostRatio();
			boostBarSprite_->SetSize(Vector2(400.0f * boostRatio, 14.0f));

			if (player_->IsBoostOverheated())
			{
				// オーバーヒート中は警告赤点滅
				static float overheatBlinkTimer = 0.0f;
				overheatBlinkTimer += dt;
				float blink = 0.5f + 0.5f * std::sin(overheatBlinkTimer * 14.0f);
				boostBarSprite_->SetColor(Vector4(1.0f, 0.2f * blink, 0.2f * blink, 0.95f));
			}
			else if (player_->IsBoosting())
			{
				// ブースト発動中は高出力発光シアン
				boostBarSprite_->SetColor(Vector4(0.3f, 0.95f, 1.0f, 1.0f));
			}
			else if (boostRatio < 0.25f)
			{
				// 残量低下時はオレンジ
				boostBarSprite_->SetColor(Vector4(1.0f, 0.6f, 0.1f, 1.0f));
			}
			else
			{
				// 通常・回復中は綺麗なブーストシアン
				boostBarSprite_->SetColor(Vector4(0.0f, 0.75f, 1.0f, 1.0f));
			}
			boostBarSprite_->Update();
		}
		if (boostBarBgSprite_)
		{
			boostBarBgSprite_->Update();
		}

		if (isPlaying_)
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
				if (bossBattleStep_ == BossBattleStep::FINISHED || (bossBattleStep_ == BossBattleStep::NOT_ACTIVE && !armoredTrainBoss_))
				{
					StartClearSequence();
				}
			}
		}
	}

	// ゲーム内通常HUD（スコア、ロックオン、HP、操作説明等）の表示・フェード制御
	if (gamePhase_ == GamePhase::START_CUTSCENE)
	{
		// 黒帯が出ている間（0.0s〜2.6s）はゲーム中HUDを完全非表示。黒帯が引くタイミング（2.6s〜3.0s）で滑らかにフェードイン
		float hudAlpha = 0.0f;
		if (cutsceneTimer_ >= 2.6f)
		{
			hudAlpha = std::clamp((cutsceneTimer_ - 2.6f) / 0.4f, 0.0f, 1.0f);
		}
		UITextManager::GetInstance()->SetHudAlpha(hudAlpha);
	}
	else if (gamePhase_ == GamePhase::GAMEOVER || gamePhase_ == GamePhase::CLEAR)
	{
		// ゲームオーバー・ゲームクリア中はゲーム中HUDを完全非表示
		UITextManager::GetInstance()->SetHudAlpha(0.0f);
	}
	else
	{
		UITextManager::GetInstance()->SetHudAlpha(1.0f);
	}

	// UIテキストの動的更新 (スコア、ロックオン数、HP)
	if (player_)
	{
		if (gamePhase_ == GamePhase::GAMEOVER || gamePhase_ == GamePhase::CLEAR || gamePhase_ == GamePhase::START_CUTSCENE)
		{
			// ゲームオーバー・ゲームクリア・スタート演出中はHUDテキストをクリア（非表示）
			UITextManager::GetInstance()->SetText("Score", "");
			UITextManager::GetInstance()->SetText("LockOn", "");
			UITextManager::GetInstance()->SetText("PlayerHP", "");
		}
		else
		{
			char scoreStr[64];
			snprintf(scoreStr, sizeof(scoreStr), "SCORE: %06d", score_);
			UITextManager::GetInstance()->SetText("Score", scoreStr);

			char lockOnStr[64];
			snprintf(lockOnStr, sizeof(lockOnStr), "LOCK ON: %zu / %d", player_->GetLockedEnemyCount(), player_->GetMaxMissiles());
			UITextManager::GetInstance()->SetText("LockOn", lockOnStr);

			char hpStr[64];
			snprintf(hpStr, sizeof(hpStr), "HP: %d / %d", player_->GetHp(), player_->GetMaxHp());
			UITextManager::GetInstance()->SetText("PlayerHP", hpStr);
		}
	}

#ifdef ENABLE_EDITOR
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
	static int currentNavIndex = 0;
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
			bool isSelected = (currentNavIndex == i);
			if (ImGui::Selectable(navItems[i], isSelected, 0, ImVec2(0, 30)))
			{
				currentNavIndex = i;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("F1: UI表示切替");
		if (ImGui::Button("全画面プレイ (F1)", ImVec2(-1, 28)))
		{
			EngineServices::GetInstance()->SetEditorMode(false);
		}

		// オーディオ・ミュートコントロール
		SoundManager::GetInstance()->DrawImGuiAudioControls();

		// 最下部にパフォーマンス情報を常時表示
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.5f, 1.0f), "PERFORMANCE");

		float fps = ImGui::GetIO().Framerate;
		float frameTime = 1000.0f / (fps > 0.0f ? fps : 1.0f);

		ImGui::Text("FPS: %.1f (%.2f ms)", fps, frameTime);
		if (lastLoadTimeMs_ > 0.0f)
		{
			ImGui::TextDisabled("Load: %.1f ms", lastLoadTimeMs_);
		}

		static float fpsHistory[60] = {};
		static int historyOffset = 0;
		fpsHistory[historyOffset] = fps;
		historyOffset = (historyOffset + 1) % IM_ARRAYSIZE(fpsHistory);

		ImGui::PlotLines("##FPSMiniGraph", fpsHistory, IM_ARRAYSIZE(fpsHistory), historyOffset, nullptr, 0.0f, 120.0f, ImVec2(-1, 42));
	}
	ImGui::End();

	// -------------------------------------------------------------
	// 2. [右ドック用] インスペクター (Inspector Content)
	// -------------------------------------------------------------
	if (ImGui::Begin("インスペクター"))
	{
		// 1. ゲーム・進行
		if (currentNavIndex == 0)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ ゲーム・進行・レール制御");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("シーン切り替え (Scene Selector)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (auto sm = GetSceneManager())
				{
					sm->DrawSceneSelectorUI();
				}
			}

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("ゲームプレイ制御 (Playback Control)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool doReset = false;

				if (isPlaying_)
				{
					if (ImGui::Button("一時停止 (Pause)", ImVec2(130, 36)))
					{
						isPlaying_ = false;
						services->SetGamePlaying(false);
					}
					ImGui::SameLine();
					if (ImGui::Button("リセット (Reset)", ImVec2(110, 36)))
					{
						doReset = true;
						isPlaying_ = false;
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
						isPlaying_ = true;
						services->SetGamePlaying(true);
					}
					ImGui::SameLine();
					if (ImGui::Button("全画面プレイ", ImVec2(140, 36)))
					{
						isPlaying_ = true;
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
					isPlaying_ = true;
					services->SetGamePlaying(true);
					StartOpeningCutscene();
				}
				if (gamePhase_ == GamePhase::START_CUTSCENE)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "進行中: %.2f / %.2f 秒", cutsceneTimer_, kCutsceneDuration);
				}

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.35f, 1.0f));
				if (ImGui::Button("クリア演出を再生 (Test Clear)", ImVec2(240, 34)))
				{
					isPlaying_ = true;
					services->SetGamePlaying(true);
					if (player_) player_->SetDead(false);
					gamePhase_ = GamePhase::PLAYING;
					StartClearSequence();
				}
				ImGui::PopStyleColor(2);
				if (gamePhase_ == GamePhase::CLEAR)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "クリア進行中 (Timer: %.2fs)", clearTotalTimer_);
				}

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.20f, 0.20f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.25f, 0.25f, 1.0f));
				if (ImGui::Button("ゲームオーバー演出を再生 (Test GameOver)", ImVec2(240, 34)))
				{
					isPlaying_ = true;
					services->SetGamePlaying(true);
					gamePhase_ = GamePhase::PLAYING;
					StartGameOverSequence();
				}
				ImGui::PopStyleColor(2);
				if (gamePhase_ == GamePhase::GAMEOVER)
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "ゲームオーバー進行中 (Timer: %.2fs)", gameOverTimer_);
				}

				ImGui::Separator();
				if (ImGui::Button("レベル再読込 (F5)", ImVec2(160, 32)))
				{
					ReloadLevel();
				}
				ImGui::SameLine();
				if (ImGui::Button("敵を全リスポーン", ImVec2(160, 32)))
				{
					ReloadEnemiesOnly();
				}

				ImGui::Separator();
				if (railCameraController_)
				{
					float p = railCameraController_->GetProgress();
					if (ImGui::SliderFloat("レール進行度 (Progress)", &p, 0.0f, 1.0f, "%.3f"))
					{
						railCameraController_->SetProgress(p);
					}
				}
				ImGui::SliderFloat("ゲーム速度 (Game Speed)", &baseGameSpeed_, 0.0f, 5.0f, "%.2fx");
				float bgmVol = SoundManager::GetInstance()->GetBGMVolume();
				if (ImGui::SliderFloat("BGM音量", &bgmVol, 0.0f, 1.0f, "%.2f"))
				{
					SoundManager::GetInstance()->SetBGMVolume(bgmVol);
				}

				// リセット処理
				if (doReset)
				{
					isPlaying_ = false;
					if (railCameraController_)
					{
						railCameraController_->Reset();
					}
					if (!mainRails_.empty() && mainRails_[0]->IsValid())
					{
						Vector3 railForward = mainRails_[0]->GetForward(0.0f);
						float yaw = std::atan2(railForward.x, railForward.z);
						float pitch = std::asin(-railForward.y);
						float railTilt = mainRails_[0]->GetTilt(0.0f);
						currentCameraRot_ = { pitch, yaw, 0.0f };
						lastCameraYaw_ = yaw;
						currentCameraBank_ = railTilt;
					}
				}

				ImGui::Separator();
				ImGui::Checkbox("レール軌跡を表示 (Draw Rail)", &isDrawRail_);
				ImGui::SameLine();
				ImGui::Checkbox("コライダーを表示 [F2]", &isDrawCollider_);
				ImGui::SameLine();
				ImGui::Checkbox("判定計算ポリゴンのみ表示 [F3]", &isDrawTerrainWireframe_);
			}

			if (railCameraController_)
			{
				ImGui::Spacing();
				if (ImGui::CollapsingHeader("レールカメラ設定 (Rail Camera)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					railCameraController_->DrawImGuiContent();
				}
			}
		}
		// 2. プレイヤー
		else if (currentNavIndex == 1)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ プレイヤー設定・チート");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("強化リング獲得UIデバッグ (Enhance Ring Icons)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::SliderInt("獲得リング数", &acquiredEnhanceRingCount_, 0, kMaxEnhanceRingIcons))
				{
					if (player_) player_->SetPowerUpLevel(acquiredEnhanceRingCount_);
				}
				ImGui::SameLine();
				if (ImGui::Button("0個")) { acquiredEnhanceRingCount_ = 0; if (player_) player_->SetPowerUpLevel(0); }
				ImGui::SameLine();
				if (ImGui::Button("1個")) { acquiredEnhanceRingCount_ = 1; if (player_) player_->SetPowerUpLevel(1); }
				ImGui::SameLine();
				if (ImGui::Button("2個")) { acquiredEnhanceRingCount_ = 2; if (player_) player_->SetPowerUpLevel(2); }

				float iconCol[4] = { ringIconColor_.x, ringIconColor_.y, ringIconColor_.z, ringIconColor_.w };
				if (ImGui::ColorEdit4("中身アイコン色", iconCol))
				{
					ringIconColor_ = { iconCol[0], iconCol[1], iconCol[2], iconCol[3] };
				}

				float outlineCol[4] = { ringOutlineColor_.x, ringOutlineColor_.y, ringOutlineColor_.z, ringOutlineColor_.w };
				if (ImGui::ColorEdit4("外枠アウトライン色", outlineCol))
				{
					ringOutlineColor_ = { outlineCol[0], outlineCol[1], outlineCol[2], outlineCol[3] };
				}
			}

			if (player_)
			{
				player_->DrawImGuiContent();
			}
			else
			{
				ImGui::TextDisabled("プレイヤーが存在しません。");
			}
		}
		// 3. エネミー・装甲列車ボス
		else if (currentNavIndex == 2)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "■ エネミー・装甲列車ボス (Enemy & Boss)");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("装甲列車（中ボス）制御 (Armored Train Boss)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				float curProg = railCameraController_ ? railCameraController_->GetProgress() : 0.0f;
				ImGui::Text("現在カメラ進行度: %.3f (%.1f%%)", curProg, curProg * 100.0f);

				// ボス出現進行度のスライダー
				ImGui::SliderFloat("ボス出現進行度", &bossSpawnProgressThreshold_, 0.0f, 1.0f, "%.3f");

				// ワンクリックで現在の自機カメラ位置を出撃地点にするボタン
				if (ImGui::Button("現在のカメラ進行度を出撃地点にセット", ImVec2(-1, 28)))
				{
					bossSpawnProgressThreshold_ = curProg;
				}

				// 設定保存ボタン
				if (ImGui::Button("ボス出現設定を保存 (Save Settings)", ImVec2(-1, 26)))
				{
					SaveBossSettings();
				}

				ImGui::Spacing();
				if (armoredTrainBoss_)
				{
					float bScale = armoredTrainBoss_->GetScaleMultiplier();
					if (ImGui::SliderFloat("ボス全体スケール倍率 (Scale)", &bScale, 0.5f, 3.0f, "%.2fx"))
					{
						armoredTrainBoss_->SetScaleMultiplier(bScale);
					}

					bool isFollow = armoredTrainBoss_->IsFollowPlayer();
					if (ImGui::Checkbox("プレイヤー連動モード (Follow Player)", &isFollow))
					{
						armoredTrainBoss_->SetFollowPlayer(isFollow);
					}
					if (isFollow)
					{
						float leadDist = armoredTrainBoss_->GetDesiredLeadDistance();
						if (ImGui::SliderFloat("並走基準距離(自機前方m)", &leadDist, 50.0f, 200.0f, "%.1f m"))
						{
							armoredTrainBoss_->SetDesiredLeadDistance(leadDist);
						}
						bool isRandomLead = armoredTrainBoss_->IsRandomLeadDistanceEnabled();
						if (ImGui::Checkbox("ランダム接近チャンス (短縮して攻撃部位を露出)", &isRandomLead))
						{
							armoredTrainBoss_->SetRandomLeadDistanceEnabled(isRandomLead);
						}
						float rushSpeed = armoredTrainBoss_->GetRushSpeedBonus();
						if (ImGui::SliderFloat("接近/追い抜き追加速度", &rushSpeed, 10.0f, 80.0f, "%.1f m/s"))
						{
							armoredTrainBoss_->SetRushSpeedBonus(rushSpeed);
						}

						bool isGroundSnap = armoredTrainBoss_->IsGroundSnapEnabled();
						if (ImGui::Checkbox("地面自動吸着 (レイキャスト接地)", &isGroundSnap))
						{
							armoredTrainBoss_->SetGroundSnapEnabled(isGroundSnap);
						}
						if (isGroundSnap)
						{
							float snapOffset = armoredTrainBoss_->GetGroundSnapOffset();
							if (ImGui::SliderFloat("接地高さ微調整オフセット(m)", &snapOffset, -5.0f, 5.0f, "%.2f m"))
							{
								armoredTrainBoss_->SetGroundSnapOffset(snapOffset);
							}
						}

						// リアルタイムチェイス監視情報
						ImGui::Text("チェイス状態: ");
						ImGui::SameLine();
						if (armoredTrainBoss_->IsOvertaking())
						{
							ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "【追い抜き急加速中】");
						}
						else if (armoredTrainBoss_->IsShortDistanceOpportunity())
						{
							ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "【至近距離攻撃チャンス中！】");
						}
						else if (armoredTrainBoss_->IsCloseToPlayer())
						{
							ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "【基準距離で並走巡航中】");
						}
						else
						{
							ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "【遠方から急接近中】");
						}

						ImGui::Text("目標距離: %.1f m (基準: %.1f m)", armoredTrainBoss_->GetDynamicLeadDistance(), armoredTrainBoss_->GetDesiredLeadDistance());
						ImGui::Text("自機との前後距離: %.1f m", armoredTrainBoss_->GetRelativeForwardDistance());
						ImGui::Text("ボス実速度: %.1f m/s (目標: %.1f m/s)", armoredTrainBoss_->GetCurrentSpeed(), armoredTrainBoss_->GetTargetSpeed());
						ImGui::Text("レール進行度: %.1f%%", armoredTrainBoss_->GetRailProgress() * 100.0f);
					}
				}

				ImGui::Spacing();
				if (bossRail_ && bossRail_->IsValid())
				{
					ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "● ボス専用レール (BossRail): 検出済み");
				}
				else
				{
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "○ ボス走行レール: メインレール追走 (BossRail未設定)");
				}
				ImGui::Separator();

				const char* stepStr = "未遭遇 (NOT_ACTIVE)";
				if (bossBattleStep_ == BossBattleStep::WARNING_ALERT) stepStr = "WARNING警告中 (ALERT)";
				else if (bossBattleStep_ == BossBattleStep::BATTLE) stepStr = "戦闘中 (BATTLE)";
				else if (bossBattleStep_ == BossBattleStep::DEFEATED_SEQUENCE) stepStr = "撃破演出中 (DEFEATED)";
				else if (bossBattleStep_ == BossBattleStep::FINISHED) stepStr = "撃破完了 (FINISHED)";
				ImGui::Text("ボス戦フェーズ: %s", stepStr);

				if (bossBattleStep_ == BossBattleStep::NOT_ACTIVE)
				{
					if (ImGui::Button("WARNING演出付きで出現 (Start Boss Encounter)", ImVec2(-1, 38)))
					{
						StartBossWarningSequence();
					}
					if (ImGui::Button("即座に出現させる (Instant Spawn)", ImVec2(-1, 28)))
					{
						isBossSpawned_ = true;
						bossBattleStep_ = BossBattleStep::BATTLE;
						if (armoredTrainBoss_)
						{
							if (bossRail_ && bossRail_->IsValid())
							{
								armoredTrainBoss_->SetRail(bossRail_.get());
								armoredTrainBoss_->SetRailProgress(0.0f);
							}
							else if (railCameraController_ && !mainRails_.empty())
							{
								float bCurProg = railCameraController_->GetProgress();
								float bossProg = std::min(1.0f, bCurProg + 0.15f);
								armoredTrainBoss_->SetRailProgress(bossProg);
								armoredTrainBoss_->SetRail(mainRails_[0].get());
							}
							armoredTrainBoss_->SetActive(true);
						}
					}
				}
				else
				{
					if (ImGui::Button("装甲列車ボスを退場・リセット (Reset Boss)", ImVec2(-1, 38)))
					{
						isBossSpawned_ = false;
						bossBattleStep_ = BossBattleStep::NOT_ACTIVE;
						bossWarningTimer_ = 0.0f;
						bossDefeatTimer_ = 0.0f;
						bossDisplayedHpRate_ = 1.0f;
						if (armoredTrainBoss_) armoredTrainBoss_->SetActive(false);
					}

					if (armoredTrainBoss_)
					{
						float hpRate = armoredTrainBoss_->GetTotalHpRate();
						ImGui::ProgressBar(hpRate, ImVec2(-1, 24), "BOSS TOTAL HP");

						ImGui::Separator();
						ImGui::Text("各車両ステータス:");
						const auto& cars = armoredTrainBoss_->GetCarriages();
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
					EnemyStudio::GetInstance()->GetShowViewport() = true;
					EnemyStudio::GetInstance()->GetShowEditor() = true;
				}
				ImGui::TextDisabled("※敵モデル・陣形・当たり判定（コライダー）の編集は、中央上の「エネミー画面」および右側の「エネミーエディター」で行えます。");
			}
		}
		// 4. 演出・シェーダー
		else if (currentNavIndex == 3)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ 演出・トランジション・シェーダー");
			ImGui::Separator();

			if (ImGui::CollapsingHeader("サイバー仮想空間ビジュアル (Star Fox Virtual Space)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Checkbox("スターフォックス風 仮想空間シェーダー有効##GameVirtualSpace", &isVirtualSpace_))
				{
					ApplyVirtualSpaceStyle();
				}
				ImGui::TextDisabled("※ONでエメラルドグリッド地面・ホログラム都市・オーロラ宇宙空間になります");
			}
			ImGui::Spacing();

			if (ImGui::CollapsingHeader("画面遷移 (Transition Settings)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (auto sm = GetSceneManager())
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
					EffectStudio::GetInstance()->GetShowViewport() = true;
					EffectStudio::GetInstance()->GetShowEditor() = true;
				}
				ImGui::TextDisabled("※エフェクトの編集・保存は、中央上部の「エフェクト画面」および右側の「エフェクトエディター」で行えます。");
			}
		}
		// 5. シーン・照明
		else if (currentNavIndex == 4)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "■ カメラ・照明・配置モデル");
			ImGui::Separator();

			// カメラ設定
			if (ImGui::CollapsingHeader("カメラ詳細 (Active Camera)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (camera)
				{
					Vector3 camPosRef = camera->GetTranslate();
					float camPosArr[3] = { camPosRef.x, camPosRef.y, camPosRef.z };
					if (ImGui::DragFloat3("座標 (Translate)", camPosArr, 0.1f))
					{
						camera->SetTranslate(Vector3(camPosArr[0], camPosArr[1], camPosArr[2]));
					}

					Vector3 camRotRef = camera->GetRotation();
					float camRotArr[3] = { camRotRef.x, camRotRef.y, camRotRef.z };
					if (ImGui::DragFloat3("回転 (Rotation)", camRotArr, 0.1f))
					{
						camera->SetRotation(Vector3(camRotArr[0], camRotArr[1], camRotArr[2]));
					}

					float fov = camera->GetFovY();
					if (ImGui::DragFloat("画角 (FOV Y)", &fov, 0.01f, 0.01f, 3.14f))
					{
						camera->SetFovY(fov);
					}

					float aspect = camera->GetAspectRatio();
					if (ImGui::DragFloat("アスペクト比", &aspect, 0.01f, 0.1f, 10.0f))
					{
						camera->SetAspectRatio(aspect);
					}

					float nearC = camera->GetNearClip();
					if (ImGui::DragFloat("近クリップ", &nearC, 0.001f, 0.001f, 100.0f))
					{
						camera->SetNearClip(nearC);
					}

					float farC = camera->GetFarClip();
					if (ImGui::DragFloat("遠クリップ", &farC, 0.1f, 1.0f, 100000.0f))
					{
						camera->SetFarClip(farC);
					}
				}
			}

			// モデルとライトの準備
			std::vector<Object3d*> allModels;
			std::vector<std::string> modelNames;
			int index = 0;
			for (auto& obj : modelInstances)
			{
				if (obj)
				{
					allModels.push_back(obj.get());
					modelNames.push_back("Model " + std::to_string(index));
				}
				index++;
			}
			if (player_)
			{
				if (player_->GetObject3d())
				{
					allModels.push_back(player_->GetObject3d());
					modelNames.push_back("Player");
				}
				if (player_->GetReticle())
				{
					allModels.push_back(player_->GetReticle());
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
		else if (currentNavIndex == 5)
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
		float currentProgress = railCameraController_ ? railCameraController_->GetProgress() : 0.0f;
		float totalRailLength = (!mainRails_.empty() && mainRails_[0]->IsValid()) ? mainRails_[0]->GetTotalLength() : 1000.0f;
		// 想定総時間（秒）: 基準速度での概算時間
		float estimatedTotalTime = (totalRailLength > 0.0f) ? (totalRailLength / 40.0f) : 60.0f;
		float currentEstimatedTime = currentProgress * estimatedTotalTime;

		// 制御バー: 再生コントロール
		ImGui::BeginGroup();
		{
			// 先頭へ
			if (ImGui::Button("|< 0%##TL", ImVec2(55, 26)))
			{
				if (railCameraController_) railCameraController_->SetProgress(0.0f);
			}
			ImGui::SameLine();
			// -5%
			if (ImGui::Button("<< -5%##TL", ImVec2(65, 26)))
			{
				float newP = (std::max)(0.0f, currentProgress - 0.05f);
				if (railCameraController_) railCameraController_->SetProgress(newP);
			}
			ImGui::SameLine();
			// 再生 / 一時停止
			if (isPlaying_)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
				if (ImGui::Button("一時停止 (Pause)##TL", ImVec2(125, 26)))
				{
					isPlaying_ = false;
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
					isPlaying_ = true;
					services->SetGamePlaying(true);
				}
				ImGui::PopStyleColor(2);
			}
			ImGui::SameLine();
			// +5%
			if (ImGui::Button("+5% >>##TL", ImVec2(65, 26)))
			{
				float newP = (std::min)(1.0f, currentProgress + 0.05f);
				if (railCameraController_) railCameraController_->SetProgress(newP);
			}
			ImGui::SameLine();
			// 末尾へ
			if (ImGui::Button("100% >|##TL", ImVec2(65, 26)))
			{
				if (railCameraController_) railCameraController_->SetProgress(1.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("リセット##TL", ImVec2(65, 26)))
			{
				isPlaying_ = false;
				services->SetGamePlaying(false);
				if (railCameraController_) railCameraController_->Reset();
				ReloadEnemiesOnly();
			}
			ImGui::SameLine();
			if (ImGui::Button("敵リスポーン##TL", ImVec2(95, 26)))
			{
				ReloadEnemiesOnly();
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.25f, 1.0f));
			if (ImGui::Button("クリア演出##TL", ImVec2(95, 26)))
			{
				isPlaying_ = true;
				services->SetGamePlaying(true);
				if (player_) player_->SetDead(false);
				gamePhase_ = GamePhase::PLAYING;
				StartClearSequence();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::TextDisabled("|");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(110.0f);
			ImGui::SliderFloat("速度##TL", &baseGameSpeed_, 0.1f, 3.0f, "%.2fx");
			ImGui::SameLine();
			if (ImGui::SmallButton("1.0x##TL")) baseGameSpeed_ = 1.0f;
			ImGui::SameLine();
			if (ImGui::SmallButton("2.0x##TL")) baseGameSpeed_ = 2.0f;
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
		if (!mainRails_.empty() && mainRails_[0]->IsValid())
		{
			const auto& zones = mainRails_[0]->GetNarrowZones();
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

		const Rail* mainRail = (!mainRails_.empty() && mainRails_[0]->IsValid()) ? mainRails_[0].get() : nullptr;

		for (const auto& enemy : enemies_)
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
			if (railCameraController_)
			{
				railCameraController_->SetProgress(newT);
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

		if (!mainRails_.empty() && mainRails_[0]->IsValid())
		{
			const auto& zones = mainRails_[0]->GetNarrowZones();
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
						if (railCameraController_) railCameraController_->SetProgress(zones[zIdx].startT);
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
						if (railCameraController_) railCameraController_->SetProgress(enemyJumpPoints[eIdx]);
					}
				}
			}
		}
	}
	ImGui::End();

#endif // ENABLE_EDITOR

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

	if (object3dCommon) object3dCommon->SetCommonDrawSetting();
	for (auto& enemy : enemies_)
	{
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
	if (armoredTrainBoss_ && isBossSpawned_)
	{
		armoredTrainBoss_->Draw();
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
				enemy->DrawCollider();
			}
			if (armoredTrainBoss_ && isBossSpawned_)
			{
				armoredTrainBoss_->DrawCollider();
			}
			for (auto& ring : enhanceRings_)
			{
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

	thrusterEffect_.Draw();
	explosionEffect_.Draw();
	hitEffect_.Draw();
	dodgeEffect_.Draw();
	trailEffect_.Draw();
	missileSmokeEffect_.Draw();
	ringEffect_.Draw();
	healRingEffect_.Draw();
	windEffect_.Draw();

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
}

void GamePlayScene::DrawUI()
{
	auto services = EngineServices::GetInstance();
	auto spriteCommon = services->GetSpriteCommon();
	if (spriteCommon) spriteCommon->SetCommonDrawSetting();
	auto srvManager = services->GetSrvManager();
	if (srvManager) srvManager->PreDraw();

	// 通常プレイ時のみHPバー、ブーストバー、強化リングアイコンを描画（スタート演出中・ゲームオーバー・クリア時は非表示）
	if (gamePhase_ == GamePhase::PLAYING)
	{
		// 強化リング獲得アイコン（ブーストゲージの上部に横2つ配置）
		// 描画順: 中身 (ringGet_icon.png) を先に描き、その上から枠 (ringGet_icon_outline.png) を重ねて描画
		for (int i = 0; i < kMaxEnhanceRingIcons; ++i)
		{
			// 1. 中身（強化リング獲得時のみ表示）
			if (i < acquiredEnhanceRingCount_ && ringGetIconSprites_[i])
			{
				ringGetIconSprites_[i]->SetColor(ringIconColor_);
				ringGetIconSprites_[i]->Update();
				ringGetIconSprites_[i]->Draw();
			}

			// 2. 枠（常時表示。描画順は枠が上）
			if (ringGetOutlineSprites_[i])
			{
				ringGetOutlineSprites_[i]->SetColor(ringOutlineColor_);
				ringGetOutlineSprites_[i]->Update();
				ringGetOutlineSprites_[i]->Draw();
			}
		}

		// ブーストバー（HPバーの上）
		if (boostBarBgSprite_) { boostBarBgSprite_->Update(); boostBarBgSprite_->Draw(); }
		if (boostBarSprite_) { boostBarSprite_->Update(); boostBarSprite_->Draw(); }

		// HPバー
		if (hpBarBgSprite_) { hpBarBgSprite_->Update(); hpBarBgSprite_->Draw(); }
		if (hpBarSprite_) { hpBarSprite_->Update(); hpBarSprite_->Draw(); }
		if (isDisplaySprite) { for (auto& sprite : sprites) if (sprite) { sprite->Update(); sprite->Draw(); } }

		// ボス専用HPバースプライト（戦闘中・撃破演出中）
		if (bossBattleStep_ == BossBattleStep::BATTLE || bossBattleStep_ == BossBattleStep::DEFEATED_SEQUENCE)
		{
			float hpRate = armoredTrainBoss_ ? armoredTrainBoss_->GetTotalHpRate() : 0.0f;
			float barW = 600.0f;
			if (bossHpBarBgSprite_) { bossHpBarBgSprite_->Update(); bossHpBarBgSprite_->Draw(); }
			if (bossHpBarDelaySprite_)
			{
				bossHpBarDelaySprite_->SetSize(Vector2(barW * std::clamp(bossDisplayedHpRate_, 0.0f, 1.0f), 18.0f));
				bossHpBarDelaySprite_->Update();
				bossHpBarDelaySprite_->Draw();
			}
			if (bossHpBarSprite_)
			{
				bossHpBarSprite_->SetSize(Vector2(barW * std::clamp(hpRate, 0.0f, 1.0f), 18.0f));
				bossHpBarSprite_->Update();
				bossHpBarSprite_->Draw();
			}
		}
	}

	// スタート演出用UIテキストの描画
	DrawCutsceneUI();

	// ボス戦用UI（WARNING警告・詳細ボスHPバー・各車両インジケーター）の描画
	DrawBossUI();

	// ゲームオーバー演出用UIテキストの描画
	DrawGameOverUI();

	// ゲームクリア演出用UIテキストの描画
	DrawClearUI();
}

void GamePlayScene::StartOpeningCutscene()
{
	gamePhase_ = GamePhase::START_CUTSCENE;
	cutsceneTimer_ = 0.0f;
	missionStartTextTimer_ = 0.0f;

	// 自機を上空・前方の初期位置にセット（高度+45m、前方+28m、ピッチ約-22度で急降下突入）
	if (player_)
	{
		Vector3 initialPos = { 0.0f, 45.0f, 28.0f };
		Vector3 initialRot = { -0.38f, 0.0f, 0.0f };
		player_->SetCutsceneOverride(true, initialPos, initialRot);
	}

	// カメラを自機の右横・やや斜め後ろにセット（機体と共に上空から下降）
	if (railCameraController_)
	{
		Vector3 camLocalPos = { 8.5f, 46.2f, 24.5f };
		Vector3 lookTarget = { 0.0f, 45.0f, 31.0f };
		railCameraController_->SetCinematicCamera(true, camLocalPos, lookTarget, 1.0f);
	}
}

void GamePlayScene::UpdateOpeningCutscene(float dt)
{
	auto services = EngineServices::GetInstance();
	auto input = services->GetInput();

	// スキップ判定（SPACEキーまたはゲームパッドA/START）
	if (input && (input->TriggerKey(DIK_SPACE) || input->TriggerPadButton(XINPUT_GAMEPAD_A) || input->TriggerPadButton(XINPUT_GAMEPAD_START)))
	{
		cutsceneTimer_ = kCutsceneDuration; // 即時終了へ
	}
	else
	{
		cutsceneTimer_ += dt;
	}

	if (cutsceneTimer_ >= kCutsceneDuration)
	{
		// 演出終了：通常ゲームプレイへ遷移（操作解放）
		gamePhase_ = GamePhase::PLAYING;
		missionStartTextTimer_ = 1.2f; // MISSION START表示をプレイ開始後もしばらく維持
		if (player_)
		{
			player_->SetCutsceneOverride(false);
			player_->SetTranslate({ 0.0f, 0.0f, 0.0f });
			player_->SetRotation({ 0.0f, 0.0f, 0.0f });
			player_->ResetPosture();
		}
		if (railCameraController_)
		{
			railCameraController_->SetCinematicCamera(false, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f);
		}
		return;
	}

	// --- タイムライン進行 ---
	const float kDiveDuration = 1.8f;       // 自機＆カメラの横並走急降下フェーズ（0.0s〜1.8s）
	const float kDockDuration = 0.9f;       // 水平復帰＆カメラ回り込みフェーズ（1.8s〜2.7s）
	// 残り0.3sは背後視点にドッキング完了・前進したままMISSION START突入（2.7s〜3.0s）

	if (cutsceneTimer_ < kDiveDuration)
	{
		// === フェーズ1: 自機とカメラが一緒に上空前方から急降下（止まらず前進！） ===
		float t = cutsceneTimer_ / kDiveDuration; // 0.0 -> 1.0
		// S字イージング (SmoothStep)
		float u = t * t * (3.0f - 2.0f * t);

		// 自機の位置: 高度45m -> 0m、前方28m -> 0m（前空からグイーンと滑空進入）
		float pY = 45.0f * (1.0f - u);
		float pZ = 28.0f * (1.0f - u);
		float pX = std::sinf(t * 3.14159265f) * 1.5f; // 左右にわずかにバンク
		Vector3 playerPos = { pX, pY, pZ };

		// 自機の回転: ピッチ（機首下げ -0.38rad -> 水平 0.0rad）、ロール
		float pitch = -0.38f * (1.0f - u);
		float roll = std::sinf(t * 3.14159265f) * 0.18f;
		Vector3 playerRot = { pitch, 0.0f, roll };

		if (player_)
		{
			player_->SetCutsceneOverride(true, playerPos, playerRot);
		}

		// カメラ位置: 自機の右横・やや斜め後ろに並走し、機体と共に一緒に下降する！
		Vector3 camPos = {
			playerPos.x + 8.5f,
			playerPos.y + 1.2f,
			playerPos.z - 3.5f
		};
		// 注視点は自機と前方の飛行空間
		Vector3 lookTarget = { playerPos.x, playerPos.y, playerPos.z + 3.0f };

		if (railCameraController_)
		{
			railCameraController_->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
		}
	}
	else if (cutsceneTimer_ < kDiveDuration + kDockDuration)
	{
		// === フェーズ2: 水平復帰＆カメラが横から背後へ滑らかに回り込み ===
		float t2 = (cutsceneTimer_ - kDiveDuration) / kDockDuration; // 0.0 -> 1.0
		float s = t2 * t2 * (3.0f - 2.0f * t2); // SmoothStep

		// 自機はレール基準位置（0, 0, 0）で水平巡航
		Vector3 playerPos = { 0.0f, 0.0f, 0.0f };
		Vector3 playerRot = { 0.0f, 0.0f, 0.0f };
		if (player_)
		{
			player_->SetCutsceneOverride(true, playerPos, playerRot);
		}

		// カメラの回り込み軌道:
		// 始点 P0: 自機横・やや後方 { 8.5f, 1.2f, -3.5f }
		// 経由点 P1: 自機斜め後ろ { 5.0f, 2.0f, -6.0f }
		// 終点 P2: 通常カメラ位置 { 0.0f, 2.5f, -8.0f }
		// 2次ベジェ曲線: P(s) = (1-s)^2 * P0 + 2*(1-s)*s * P1 + s^2 * P2
		Vector3 p0 = { 8.5f, 1.2f, -3.5f };
		Vector3 p1 = { 5.0f, 2.0f, -6.0f };
		Vector3 p2 = { 0.0f, 2.5f, -8.0f };

		float invS = 1.0f - s;
		Vector3 camPos = {
			invS * invS * p0.x + 2.0f * invS * s * p1.x + s * s * p2.x,
			invS * invS * p0.y + 2.0f * invS * s * p1.y + s * s * p2.y,
			invS * invS * p0.z + 2.0f * invS * s * p1.z + s * s * p2.z
		};

		// ブレンド率: 1.0 -> 0.0 （背後に回り込みながら通常カメラの姿勢へ完全融合）
		float blend = 1.0f - s;
		if (railCameraController_)
		{
			railCameraController_->SetCinematicCamera(true, camPos, playerPos, blend);
		}
	}
	else
	{
		// === フェーズ3: 背後通常視点に合流完了（直後に操作解禁） ===
		if (player_)
		{
			player_->SetCutsceneOverride(true, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
		}
		if (railCameraController_)
		{
			railCameraController_->SetCinematicCamera(true, { 0.0f, 2.5f, -8.0f }, { 0.0f, 0.0f, 0.0f }, 0.0f);
		}
	}
}

void GamePlayScene::DrawCutsceneUI()
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

	// ===== 弾道予告ガイドライン（手前照準 / 銃口 -> 奥照準） =====
	if (gamePhase_ == GamePhase::PLAYING && player_ && !player_->IsDead() && activeCamera_)
	{
		Vector3 pStart = player_->GetFrontReticleWorldPosition();
		Vector3 pEnd = player_->GetReticleWorldPosition();

		const Matrix4x4& vMat = activeCamera_->GetViewMatrix();
		const Matrix4x4& pMat = activeCamera_->GetProjectionMatrix();
		Matrix4x4 vpMat = vMat * pMat;

		auto WorldToScreen = [&](const Vector3& world) -> std::pair<bool, ImVec2> {
			float w = world.x * vpMat.m[0][3] + world.y * vpMat.m[1][3] + world.z * vpMat.m[2][3] + vpMat.m[3][3];
			if (w < 0.1f) return { false, ImVec2(0, 0) };
			float x = (world.x * vpMat.m[0][0] + world.y * vpMat.m[1][0] + world.z * vpMat.m[2][0] + vpMat.m[3][0]) / w;
			float y = (world.x * vpMat.m[0][1] + world.y * vpMat.m[1][1] + world.z * vpMat.m[2][1] + vpMat.m[3][1]) / w;
			float sx = viewPos.x + (x * 0.5f + 0.5f) * screenW;
			float sy = viewPos.y + (-y * 0.5f + 0.5f) * screenH;
			return { true, ImVec2(sx, sy) };
		};

		auto [okStart, ptStart] = WorldToScreen(pStart);
		auto [okEnd, ptEnd] = WorldToScreen(pEnd);

		Vector3 pLeftWing = player_->GetLeftWingPosition();
		Vector3 pRightWing = player_->GetRightWingPosition();
		auto [okLeft, ptLeft] = WorldToScreen(pLeftWing);
		auto [okRight, ptRight] = WorldToScreen(pRightWing);

		// 1. 弾道予告線（ユーザー指示により非表示）
		/*
		if (okEnd)
		{
			bool isLocked = player_->IsTargetInReticle();
			ImU32 lineCol = isLocked ? IM_COL32(255, 235, 50, 76) : IM_COL32(220, 245, 255, 56);
			ImU32 shadowCol = IM_COL32(0, 0, 0, 36);

			auto DrawMuzzleRay = [&](const ImVec2& ptMuzzle) {
				float dist = std::hypot(ptEnd.x - ptMuzzle.x, ptEnd.y - ptMuzzle.y);
				if (dist > 8.0f)
				{
					int segs = (std::max)(2, (int)(dist / 16.0f));
					for (int i = 0; i < segs; ++i)
					{
						if (i % 2 == 0)
						{
							float t0 = (float)i / segs;
							float t1 = (float)(i + 1) / segs;
							ImVec2 pA(ptMuzzle.x + (ptEnd.x - ptMuzzle.x) * t0, ptMuzzle.y + (ptEnd.y - ptMuzzle.y) * t0);
							ImVec2 pB(ptMuzzle.x + (ptEnd.x - ptMuzzle.x) * t1, ptMuzzle.y + (ptEnd.y - ptMuzzle.y) * t1);
							drawList->AddLine(pA, pB, shadowCol, 2.2f * scale);
							drawList->AddLine(pA, pB, lineCol, 1.2f * scale);
						}
					}
				}
			};

			if (okLeft) DrawMuzzleRay(ptLeft);
			if (okRight) DrawMuzzleRay(ptRight);
		}
		*/

		// 2. ダブルレティクル【スターフォックス風デュアル・スクエア・レティクル】
		// ① 手前照準（インナー四角フレーム / 自機銃口HUD）
		// 自機機首の正面方向に完全同期し、自機の主翼ロール角に合わせてフレームがリアルタイムに傾く！
		if (okStart)
		{
			// 自機のZ軸回転（ロール角・主翼傾き）を画面上の角度として直接算出！
			// 自機が右に30度傾いたら（右バンク）、右翼が下がり左翼が上がるため dy > 0 となり、手前レティクルも右に30度傾く！
			float rollAngle = 0.0f;
			if (okLeft && okRight)
			{
				float dx = ptRight.x - ptLeft.x;
				float dy = ptRight.y - ptLeft.y;
				rollAngle = std::atan2(dy, dx);
			}
			else
			{
				rollAngle = player_->GetFinalBank();
			}

			float cosR = std::cos(rollAngle);
			float sinR = std::sin(rollAngle);
			// 画面上での自機ローカル基底ベクトル: u=(cos, sin), v=(-sin, cos)
			ImVec2 axisU(cosR, sinR);
			ImVec2 axisV(-sinR, cosR);

			float innerSize = 21.0f * scale;      // インナーフレーム半幅（奥照準28.0fに対して程よいサイズ感）
			float cornerLen = 8.5f * scale;       // L字の角の長さ（四角枠としてしっかり視認できる長さ）
			float stroke = 2.4f * scale;          // 線の太さ
			float shadowStroke = stroke + 2.6f;   // 黒縁アウトライン太さ（背景遮断用）

			// 2つの照準の色を統一（通常時: 純白ホワイト + 黒縁アウトライン、ロック時: 高輝度ゴールド）
			bool isLocked = player_->IsTargetInReticle();
			ImU32 innerColor = isLocked ? IM_COL32(255, 230, 45, 255) : IM_COL32(255, 255, 255, 255);
			ImU32 shadowColor = IM_COL32(0, 0, 0, 230);

			ImVec2 c = ptStart;

			// コーナーブラケット描画ヘルパーラムダ（回転対応）
			auto DrawRotatedCorner = [&](float uDist, float vDist, float du, float dv, ImU32 col, float th) {
				ImVec2 cornerPos(c.x + axisU.x * uDist + axisV.x * vDist, c.y + axisU.y * uDist + axisV.y * vDist);
				ImVec2 armU(cornerPos.x + axisU.x * du, cornerPos.y + axisU.y * du);
				ImVec2 armV(cornerPos.x + axisV.x * dv, cornerPos.y + axisV.y * dv);
				drawList->AddLine(cornerPos, armU, col, th);
				drawList->AddLine(cornerPos, armV, col, th);
			};

			// 四隅のL字ブラケット [ ] （極太黒アウトライン付きでシアン背景・オーロラでも完全視認）
			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 col = (pass == 0) ? shadowColor : innerColor;
				float th = (pass == 0) ? shadowStroke : stroke;

				// 左上
				DrawRotatedCorner(-innerSize, -innerSize, cornerLen, cornerLen, col, th);
				// 右上
				DrawRotatedCorner(innerSize, -innerSize, -cornerLen, cornerLen, col, th);
				// 左下
				DrawRotatedCorner(-innerSize, innerSize, cornerLen, -cornerLen, col, th);
				// 右下
				DrawRotatedCorner(innerSize, innerSize, -cornerLen, -cornerLen, col, th);
			}

			// 中心精密クロスヘア & 銃口ドット（自機正面の射線中心、自機ロール連動）
			float crossLen = 4.0f * scale;
			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 col = (pass == 0) ? shadowColor : innerColor;
				float th = (pass == 0) ? stroke + 2.0f : stroke * 0.85f;

				ImVec2 uA(c.x - axisU.x * crossLen, c.y - axisU.y * crossLen);
				ImVec2 uB(c.x + axisU.x * crossLen, c.y + axisU.y * crossLen);
				ImVec2 vA(c.x - axisV.x * crossLen, c.y - axisV.y * crossLen);
				ImVec2 vB(c.x + axisV.x * crossLen, c.y + axisV.y * crossLen);
				drawList->AddLine(uA, uB, col, th);
				drawList->AddLine(vA, vB, col, th);
			}
			drawList->AddCircleFilled(c, 2.5f * scale, shadowColor);
			drawList->AddCircleFilled(c, 1.4f * scale, innerColor);
		}

		// ② 奥照準（アウター四角フレーム / ターゲット枠） 2D UIスクリーン描画
		// 仕様書: PosTarget3D = PosShip + (DirAim * DistFar)
		//         PosScreen2D = WorldToScreen(PosTarget3D)
		//         UI描画のため地形による遮蔽（Z埋もれ）が発生せず、常にシャープにターゲットを捕捉
		if (okEnd)
		{
			bool isLocked = player_->IsTargetInReticle();
			float reticleBaseSize = 28.0f * scale; // 基準サイズ（UI解像度スケール対応）
			float cornerLen = 10.0f * scale;       // L字の角の長さ
			float stroke = 2.4f * scale;          // 線の太さ
			float shadowStroke = stroke + 2.4f;   // 黒縁シャドウの太さ

			// 手前照準と同一の白色（ロック時は揃ってゴールド）で統一
			ImU32 frameColor = isLocked ? IM_COL32(255, 230, 45, 255) : IM_COL32(255, 255, 255, 255);
			ImU32 shadowColor = IM_COL32(0, 0, 0, 230);

			if (isLocked)
			{
				reticleBaseSize *= 0.88f; // スナップ収縮
			}

			float halfW = reticleBaseSize;
			float halfH = reticleBaseSize;
			ImVec2 c = ptEnd; // 奥照準の2D画面座標

			// コーナーブラケット描画ヘルパーラムダ
			auto DrawCorner = [&](float x, float y, float dx, float dy, ImU32 col, float th) {
				drawList->AddLine(ImVec2(x, y), ImVec2(x + dx, y), col, th);
				drawList->AddLine(ImVec2(x, y), ImVec2(x, y + dy), col, th);
			};

			// 四隅のL字ブラケット [ ] （黒アウトライン付きで雪原や空でも背景同化を完全防止）
			for (int pass = 0; pass < 2; ++pass)
			{
				ImU32 col = (pass == 0) ? shadowColor : frameColor;
				float th = (pass == 0) ? shadowStroke : stroke;

				// 左上
				DrawCorner(c.x - halfW, c.y - halfH, cornerLen, cornerLen, col, th);
				// 右上
				DrawCorner(c.x + halfW, c.y - halfH, -cornerLen, cornerLen, col, th);
				// 左下
				DrawCorner(c.x - halfW, c.y + halfH, cornerLen, -cornerLen, col, th);
				// 右下
				DrawCorner(c.x + halfW, c.y + halfH, -cornerLen, -cornerLen, col, th);
			}

			// 中央精密クロスヘア（十字マーク）
			float crossSize = 5.0f * scale;
			drawList->AddLine(ImVec2(c.x - crossSize, c.y), ImVec2(c.x + crossSize, c.y), shadowColor, stroke + 2.0f);
			drawList->AddLine(ImVec2(c.x, c.y - crossSize), ImVec2(c.x, c.y + crossSize), shadowColor, stroke + 2.0f);
			drawList->AddLine(ImVec2(c.x - crossSize, c.y), ImVec2(c.x + crossSize, c.y), frameColor, stroke);
			drawList->AddLine(ImVec2(c.x, c.y - crossSize), ImVec2(c.x, c.y + crossSize), frameColor, stroke);

			// 敵ロックオン時のサークルインジケーター演出
			if (isLocked)
			{
				float ringRadius = halfW * 1.35f;
				drawList->AddCircle(c, ringRadius, IM_COL32(255, 210, 35, 120), 24, 1.8f * scale);
			}
		}

		// 3. 敵撃破時のスコア加算ポップアップ演出（3D座標に「+100」などが一瞬ポップして消える）
		for (const auto& popup : scorePopups_)
		{
			float t = popup.timer / popup.duration;
			if (t >= 1.0f) continue;

			// 時間経過でふわっと少し上へ上昇
			Vector3 popWorld = popup.worldPos + Vector3(0.0f, popup.timer * 3.5f, 0.0f);
			auto [okPop, ptPop] = WorldToScreen(popWorld);
			if (okPop)
			{
				// 透明度（後半25%で滑らかにフェードアウト）
				float alpha = (t < 0.75f) ? 1.0f : (1.0f - (t - 0.75f) / 0.25f);
				int a = std::clamp((int)(alpha * 255), 0, 255);
				int shadowA = std::clamp((int)(alpha * 230), 0, 230);

				// 出現時の一瞬のポップ拡大弾性演出
				float popScale = (t < 0.12f) ? (1.0f + (0.12f - t) * 2.2f) : 1.0f;
				float fontSize = 22.0f * scale * popScale;

				char buf[32];
				snprintf(buf, sizeof(buf), "+%d", popup.score);

				ImFont* font = ImGui::GetFont();
				ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
				ImVec2 textPos(ptPop.x - textSize.x * 0.5f, ptPop.y - textSize.y * 0.5f);

				// デジタルネオンイエロー / ゴールド
				ImU32 textCol = IM_COL32(255, 235, 45, a);
				ImU32 shadowCol = IM_COL32(0, 0, 0, shadowA);

				// 8方向の濃い黒縁シャドウ（雪原やオーロラでも完璧に視認できるデジタル輪郭）
				float o = 1.8f * scale;
				drawList->AddText(font, fontSize, ImVec2(textPos.x - o, textPos.y), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x + o, textPos.y), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x, textPos.y - o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x, textPos.y + o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x - o, textPos.y - o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x + o, textPos.y - o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x - o, textPos.y + o), shadowCol, buf);
				drawList->AddText(font, fontSize, ImVec2(textPos.x + o, textPos.y + o), shadowCol, buf);

				// メインテキスト
				drawList->AddText(font, fontSize, textPos, textCol, buf);
			}
		}
	}

	// === 上下の黒帯（シネマスコープ・映画風レターボックス演出） ===
	float barHeightMax = screenH * 0.11f;
	float barHeight = 0.0f;

	if (gamePhase_ == GamePhase::START_CUTSCENE)
	{
		if (cutsceneTimer_ < 0.3f)
		{
			barHeight = barHeightMax * (cutsceneTimer_ / 0.3f);
		}
		else if (cutsceneTimer_ >= 2.6f)
		{
			float t = (cutsceneTimer_ - 2.6f) / 0.4f;
			barHeight = barHeightMax * (1.0f - std::clamp(t, 0.0f, 1.0f));
		}
		else
		{
			barHeight = barHeightMax;
		}
	}
	else if (gamePhase_ == GamePhase::PLAYING && missionStartTextTimer_ > 0.0f)
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

	// カットシーン中のみスキップ案内（ゲーム画面右下）＋タイトル空間破壊からの開幕白フラッシュ減衰
	if (gamePhase_ == GamePhase::START_CUTSCENE)
	{

		const char* skipText = "[SPACE] SKIP";
		drawList->AddText(ImVec2(viewPos.x + screenW - 140.0f * scale, viewPos.y + screenH - barHeight - 30.0f * scale), IM_COL32(200, 200, 200, 180), skipText);
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
	if (gamePhase_ == GamePhase::START_CUTSCENE && cutsceneTimer_ >= 1.8f && cutsceneTimer_ < 2.5f)
	{
		float t = (cutsceneTimer_ - 1.8f) / 0.7f;
		float alpha = std::clamp(std::sinf(t * 3.14159265f), 0.0f, 1.0f);

		const UITextItem* readyItem = textMgr ? textMgr->GetTextItem("CutsceneReady", "GAMEPLAY") : nullptr;
		if (readyItem)
		{
			renderTextItem(readyItem, alpha);
		}
	}
	else if ((gamePhase_ == GamePhase::START_CUTSCENE && cutsceneTimer_ >= 2.5f) || (gamePhase_ == GamePhase::PLAYING && missionStartTextTimer_ > 0.0f))
	{
		float alpha = 1.0f;
		if (gamePhase_ == GamePhase::PLAYING)
		{
			alpha = std::clamp(missionStartTextTimer_ / 1.2f, 0.0f, 1.0f);
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

void GamePlayScene::StartGameOverSequence()
{
	gamePhase_ = GamePhase::GAMEOVER;
	gameOverStep_ = GameOverStep::FALLING;
	gameOverSelectedOption_ = GameOverMenuOption::Retry;
	gameOverTimer_ = 0.0f;
	gameOverFallVelocity_ = 2.0f; // 落下初速

	if (player_)
	{
		gameOverPlayerFallPos_ = player_->GetTranslate();
		gameOverPlayerFallRot_ = player_->GetRotation();
		player_->SetCutsceneOverride(true, gameOverPlayerFallPos_, gameOverPlayerFallRot_);
		player_->Update3DObjectOnly();
		player_->SetDead(true);

		// 初回被弾時の火花＆白煙エフェクト（作り直すため一旦無効化）
		// hitEffect_.SetPosition(player_->GetWorldPosition());
		// hitEffect_.Play();
	}

	// レールカメラの前進を即座に停止（その場にとどまる）
	if (railCameraController_)
	{
		railCameraController_->SetSpeedMultiplier(0.0f);
	}

	// 被弾の衝撃によるカメラ揺れ
	cameraShakeTimer_ = 25.0f;
}

void GamePlayScene::UpdateGameOverSequence(float dt)
{
	gameOverTimer_ += dt;

	// カメラ前進は完全停止を維持
	if (railCameraController_)
	{
		railCameraController_->SetSpeedMultiplier(0.0f);
	}

	switch (gameOverStep_)
	{
	case GameOverStep::FALLING:
	{
		// 重力加速度による急降下
		gameOverFallVelocity_ += 24.0f * dt;
		gameOverPlayerFallPos_.y -= gameOverFallVelocity_ * dt;
		gameOverPlayerFallPos_.z += 4.5f * dt; // 慣性による前方進行
		gameOverPlayerFallRot_.x += 2.2f * dt; // ピッチ機首下げ
		gameOverPlayerFallRot_.z += 5.5f * dt; // ロールキリモミ回転

		if (player_)
		{
			player_->SetCutsceneOverride(true, gameOverPlayerFallPos_, gameOverPlayerFallRot_);
			player_->Update3DObjectOnly();

			// 落下中の煙トレイル（自機のワールド座標から黒煙を連続放出）
			Vector3 pWorldPos = player_->GetWorldPosition();
			missileSmokeEffect_.SetPosition(pWorldPos);
			missileSmokeEffect_.Play();

			// 地上激突判定
			// ※被弾直後（0.5秒以内）は直前の障害物接触を無視し、確実に墜落フェーズを見せる！
			bool hitGround = false;
			if (gameOverTimer_ >= 0.5f)
			{
				OBB playerOBB = player_->GetWorldOBB();
				for (const auto& obs : obstacles_)
				{
					if (!obs || obs->IsDead()) continue;
					CollisionResult res;
					if (obs->CheckCollisionWithOBB(playerOBB, &res))
					{
						hitGround = true;
						break;
					}
				}
			}

			// 地形・障害物接触、または高度約-14m以下、または落下時間1.2秒経過で地上激突・爆散！
			if (hitGround || gameOverPlayerFallPos_.y <= -14.0f || gameOverTimer_ >= 1.2f)
			{
				gameOverStep_ = GameOverStep::EXPLODED;
				gameOverTimer_ = 0.0f;

				// 機体を非表示化（爆散消滅）
				player_->SetVisible(false);

				// 大爆発エフェクトを発生！
				explosionEffect_.SetPosition(pWorldPos);
				explosionEffect_.Play();

				// 大爆発による画面激震（カメラシェイク）
				cameraShakeTimer_ = 50.0f;
			}
		}
		break;
	}

	case GameOverStep::EXPLODED:
	{
		// 激突・爆散直後の余韻（0.8秒待機）
		if (gameOverTimer_ >= 0.8f)
		{
			gameOverStep_ = GameOverStep::SHOW_UI;
			gameOverTimer_ = 0.0f;
		}
		break;
	}

	case GameOverStep::SHOW_UI:
	{
		// W / S キー（または矢印キー上下）でリトライとタイトルを切り替え
		// SPACEキー（またはENTERキー）で決定
		auto sceneManager = GetSceneManager();
		if (sceneManager && !sceneManager->IsTransitioning())
		{
			auto input = EngineServices::GetInstance()->GetInput();
			if (input)
			{
				if (input->TriggerKey(DIK_W) || input->TriggerKey(DIK_UP))
				{
					gameOverSelectedOption_ = GameOverMenuOption::Retry;
				}
				else if (input->TriggerKey(DIK_S) || input->TriggerKey(DIK_DOWN))
				{
					gameOverSelectedOption_ = GameOverMenuOption::Title;
				}

				if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN))
				{
					if (gameOverSelectedOption_ == GameOverMenuOption::Retry)
					{
						sceneManager->ChangeScene("GAMEPLAY", 0.5f);
					}
					else if (gameOverSelectedOption_ == GameOverMenuOption::Title)
					{
						sceneManager->ChangeScene("TITLE", 0.6f);
					}
				}
			}
		}
		break;
	}
	}
}

void GamePlayScene::DrawGameOverUI()
{
#ifdef USE_IMGUI
	if (gamePhase_ != GamePhase::GAMEOVER) return;
	if (gameOverStep_ == GameOverStep::FALLING) return; // 落下中は映像のみに集中

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

	// ゲーム画面ビューポートの枠外へはみ出さないようにクリッピング
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	// 画面遷移の完全暗転中はUIテキストを描画せず、ビューポートを真っ黒にしてテキストを完全に「画面遷移の後ろ」に隠す
	if (transitionAlpha <= 0.001f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
		drawList->PopClipRect();
		return;
	}

	// スケーリング比率（1280x720 基準）
	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.3f) scale = 0.3f;

	// === 上下の映画風黒帯（シネマスコープ・レターボックス） ===
	float barHeightMax = screenH * 0.12f;
	float barProgress = 1.0f;
	if (gameOverStep_ == GameOverStep::EXPLODED)
	{
		barProgress = std::clamp(gameOverTimer_ / 0.5f, 0.0f, 1.0f);
	}
	float barHeight = barHeightMax * barProgress;
	if (barHeight > 0.5f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barHeight), IM_COL32(0, 0, 0, static_cast<int>(255 * transitionAlpha)));
		drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barHeight), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, static_cast<int>(255 * transitionAlpha)));
	}

	if (gameOverStep_ == GameOverStep::SHOW_UI)
	{
		auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
		auto textMgr = UITextManager::GetInstance();

		// フェードイン率 (0.0 -> 1.0, 0.6秒)
		float fadeAlpha = std::clamp(gameOverTimer_ / 0.6f, 0.0f, 1.0f);

		// 画面中央に濃い黒帯バナーを敷いて視認性を大幅強化（裏側が透けないよう不透明度を245に向上）
		float centerY = viewPos.y + screenH * 0.42f;
		float bannerH = 150.0f * scale;
		drawList->AddRectFilled(
			ImVec2(viewPos.x, centerY - bannerH * 0.5f),
			ImVec2(viewPos.x + screenW, centerY + bannerH * 0.5f),
			IM_COL32(8, 4, 4, static_cast<int>(245.0f * fadeAlpha * transitionAlpha))
		);

		// 汎用アイテム描画ラムダ（UITextManager設定に完全同期）
		auto renderTextItem = [&](const UITextItem* item, float alpha, float customScale = 1.0f, const std::string& overrideText = "", bool isHighlight = false) {
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

			Vector4 col = item->color;
			if (isHighlight)
			{
				col = { 1.0f, 0.95f, 0.35f, 1.0f }; // 選択中の強調ゴールド
			}
			ImU32 textCol = ImColor(col.x, col.y, col.z, col.w * alpha);
			drawList->AddText(font, fSize, tPos, textCol, str.c_str());
		};

		// 1. "GAME OVER" タイトル描画（UITextManager設定参照）
		const UITextItem* titleItem = textMgr ? textMgr->GetTextItem("GameOverTitle", "GAMEPLAY") : nullptr;
		if (titleItem)
		{
			renderTextItem(titleItem, fadeAlpha);

			// 赤色アクセントアンダーバー
			float barY = viewPos.y + (titleItem->position.y + titleItem->fontSize * 0.65f) * (screenH / 720.0f);
			float barHalfW = 160.0f * scale;
			drawList->AddLine(
				ImVec2(viewPos.x + screenW * 0.5f - barHalfW, barY),
				ImVec2(viewPos.x + screenW * 0.5f + barHalfW, barY),
				IM_COL32(235, 30, 45, static_cast<int>(220.0f * fadeAlpha)),
				2.5f * scale
			);
		}

		// 2. メニュー選択肢（W / S で選択、SPACEで決定）
		if (gameOverTimer_ >= 0.5f)
		{
			float guideFade = std::clamp((gameOverTimer_ - 0.5f) / 0.5f, 0.0f, 1.0f);
			float pulse = 0.85f + 0.15f * std::sin(gameOverTimer_ * 5.0f);

			// RETRY 項目
			const UITextItem* retryItem = textMgr ? textMgr->GetTextItem("GameOverRetry", "GAMEPLAY") : nullptr;
			if (retryItem)
			{
				bool isSelected = (gameOverSelectedOption_ == GameOverMenuOption::Retry);
				float itemAlpha = isSelected ? (guideFade * pulse) : (guideFade * 0.55f);
				std::string retryStr = isSelected ? ("> " + retryItem->text) : ("  " + retryItem->text);
				renderTextItem(retryItem, itemAlpha, isSelected ? 1.08f : 1.0f, retryStr, isSelected);
			}

			// TITLE 項目
			const UITextItem* titleNavItem = textMgr ? textMgr->GetTextItem("GameOverTitleNav", "GAMEPLAY") : nullptr;
			if (titleNavItem)
			{
				bool isSelected = (gameOverSelectedOption_ == GameOverMenuOption::Title);
				float itemAlpha = isSelected ? (guideFade * pulse) : (guideFade * 0.55f);
				std::string titleStr = isSelected ? ("> " + titleNavItem->text) : ("  " + titleNavItem->text);
				renderTextItem(titleNavItem, itemAlpha, isSelected ? 1.08f : 1.0f, titleStr, isSelected);
			}

			// 画面下部に操作ナビゲーション（控えめに表示）
			ImFont* fontSub = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::English_FiraMono) : ImGui::GetFont();
			if (!fontSub) fontSub = ImGui::GetFont();
			const char* navText = "[W / S] SELECT      [SPACE] DECIDE";
			float navFontSize = 18.0f * scale;
			ImVec2 navSize = fontSub->CalcTextSizeA(navFontSize, FLT_MAX, -1.0f, navText);
			ImVec2 navPos(viewPos.x + (screenW - navSize.x) * 0.5f, viewPos.y + screenH * 0.78f);
			drawList->AddText(fontSub, navFontSize, ImVec2(navPos.x + 1.5f, navPos.y + 1.5f), IM_COL32(0, 0, 0, static_cast<int>(180.0f * guideFade)), navText);
			drawList->AddText(fontSub, navFontSize, navPos, IM_COL32(180, 190, 200, static_cast<int>(200.0f * guideFade)), navText);
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

void GamePlayScene::StartClearSequence()
{
	if (gamePhase_ != GamePhase::PLAYING) return;

	gamePhase_ = GamePhase::CLEAR;
	clearStep_ = ClearStep::ASCENDING;
	clearTimer_ = 0.0f;
	clearTotalTimer_ = 0.0f;
	clearAscentSpeed_ = 10.0f; // 上昇初速

	if (player_)
	{
		clearStartPlayerPos_ = player_->GetTranslate();
		clearPlayerAscentPos_ = clearStartPlayerPos_;
		clearPlayerAscentRot_ = player_->GetRotation();
		player_->SetCutsceneOverride(true, clearPlayerAscentPos_, clearPlayerAscentRot_);
		player_->Update3DObjectOnly();
	}

	// 敵弾を全消去して安全・爽快にクリア
	enemyBullets_.clear();

	// レールカメラの前進を即座に停止（その場にとどまる）
	if (railCameraController_)
	{
		railCameraController_->SetSpeedMultiplier(0.0f);
		// 初期シネマティックカメラを設定（通常カメラ位置・注視点からシームレスに開始）
		Vector3 camPos = { 0.0f, 2.5f, -8.0f };
		Vector3 lookTarget = { 0.0f, clearStartPlayerPos_.y, clearStartPlayerPos_.z };
		railCameraController_->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
	}

	// 画面揺れ停止
	cameraShakeTimer_ = 0.0f;
}

void GamePlayScene::UpdateClearSequence(float dt)
{
	clearTimer_ += dt;
	clearTotalTimer_ += dt;

	// カメラ前進は完全停止を維持
	if (railCameraController_)
	{
		railCameraController_->SetSpeedMultiplier(0.0f);
	}

	// 自機の上昇・加速・姿勢制御
	clearAscentSpeed_ += 40.0f * dt; // ぐんぐん加速
	// 機首を上向きに傾ける（-0.5rad ≒ -28度）
	clearPlayerAscentRot_.x += (-0.5f - clearPlayerAscentRot_.x) * (3.0f * dt);
	// ロールとヨーを水平に復元
	clearPlayerAscentRot_.y += (0.0f - clearPlayerAscentRot_.y) * (4.0f * dt);
	clearPlayerAscentRot_.z += (0.0f - clearPlayerAscentRot_.z) * (4.0f * dt);

	// 位置更新（上空＋前方へ加速上昇）
	clearPlayerAscentPos_.y += (clearAscentSpeed_ * 0.85f) * dt;
	clearPlayerAscentPos_.z += (clearAscentSpeed_ * 1.35f) * dt;

	if (player_)
	{
		player_->SetCutsceneOverride(true, clearPlayerAscentPos_, clearPlayerAscentRot_);
		player_->Update3DObjectOnly();

		// スラスターエフェクト（ジェット排気）を自機後方に噴射
		Vector3 pWorld = player_->GetWorldPosition();
		thrusterEffect_.SetPosition(pWorld);
		thrusterEffect_.Play();
	}

	// === クリア演出のカメラワーク ===
	// 自機の上昇に合わせてカメラも少し持ち上げ、見上げるが、ある程度（見上げ角度約-18度）上がったら回転・移動を完全に停止・固定する
	if (railCameraController_)
	{
		// カメラ位置: 初期位置 {0, 2.5, -8} から自機の上昇に合わせて少し上 {0, 4.0, -9.0} へ滑らかに持ち上げる（通算タイマーで計算し、UI表示時も巻き戻らない！）
		float camRiseT = std::clamp(clearTotalTimer_ / 0.8f, 0.0f, 1.0f);
		float smoothRise = camRiseT * camRiseT * (3.0f - 2.0f * camRiseT); // SmoothStep
		Vector3 camPos = {
			0.0f,
			2.5f + 1.5f * smoothRise,  // 2.5m -> 4.0m に少し上昇
			-8.0f - 1.0f * smoothRise  // わずかに引いて飛び去る機体を綺麗に画角に収める
		};

		// 注視点: 自機の上昇に合わせて上を向くが、上限（初期Y + 8.5m）に達したらピタッと停止して固定！
		float maxLookUpY = clearStartPlayerPos_.y + 8.5f;
		float maxLookZ = clearStartPlayerPos_.z + 12.0f;
		float currentLookY = std::min(maxLookUpY, clearStartPlayerPos_.y + (clearPlayerAscentPos_.y - clearStartPlayerPos_.y) * 0.7f);
		float currentLookZ = std::min(maxLookZ, clearStartPlayerPos_.z + (clearPlayerAscentPos_.z - clearStartPlayerPos_.z) * 0.35f);

		Vector3 lookTarget = { 0.0f, currentLookY, currentLookZ };

		// シネマティックカメラとして適用（ブレンド率1.0で完全制御）
		railCameraController_->SetCinematicCamera(true, camPos, lookTarget, 1.0f);
	}

	switch (clearStep_)
	{
	case ClearStep::ASCENDING:
	{
		// 1.5秒ほど上昇したらUI表示ステップへ
		if (clearTimer_ >= 1.5f)
		{
			clearStep_ = ClearStep::SHOW_UI;
			clearTimer_ = 0.0f;
		}
		break;
	}
	case ClearStep::SHOW_UI:
	{
		// UI表示後、0.5秒の余韻を置いてからSPACE / ENTER 入力受付
		if (clearTimer_ >= 0.5f)
		{
			auto sceneManager = GetSceneManager();
			if (sceneManager && !sceneManager->IsTransitioning())
			{
				auto input = EngineServices::GetInstance()->GetInput();
				if (input)
				{
					if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN))
					{
						sceneManager->ChangeScene("TITLE", 0.6f);
					}
				}
			}
		}
		break;
	}
	}
}

void GamePlayScene::DrawClearUI()
{
#ifdef USE_IMGUI
	if (gamePhase_ != GamePhase::CLEAR) return;

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

	// ゲーム画面ビューポートの枠外へはみ出さないようにクリッピング
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	// 画面遷移の完全暗転中はUIテキストを描画せず、ビューポートを真っ黒にしてテキストを完全に「画面遷移の後ろ」に隠す
	if (transitionAlpha <= 0.001f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, 255));
		drawList->PopClipRect();
		return;
	}

	// スケーリング比率（1280x720 基準）
	float scale = std::min(screenW / 1280.0f, screenH / 720.0f);
	if (scale < 0.3f) scale = 0.3f;

	// === 上下の映画風黒帯（シネマスコープ・レターボックス） ===
	float barHeightMax = screenH * 0.12f;
	float barProgress = 1.0f;
	if (clearStep_ == ClearStep::ASCENDING)
	{
		barProgress = std::clamp(clearTimer_ / 0.8f, 0.0f, 1.0f);
	}
	float barHeight = barHeightMax * barProgress;
	if (barHeight > 0.5f)
	{
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barHeight), IM_COL32(0, 0, 0, static_cast<int>(255 * transitionAlpha)));
		drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barHeight), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(0, 0, 0, static_cast<int>(255 * transitionAlpha)));
	}

	if (clearStep_ == ClearStep::SHOW_UI)
	{
		auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
		auto textMgr = UITextManager::GetInstance();

		// フェードイン率 (0.0 -> 1.0, 0.6秒)
		float fadeAlpha = std::clamp(clearTimer_ / 0.6f, 0.0f, 1.0f);

		// 画面中央に濃い黒帯バナーを敷いて視認性を大幅強化（裏側が透けないよう不透明度を245に向上）
		float centerY = viewPos.y + screenH * 0.44f;
		float bannerH = 190.0f * scale;
		drawList->AddRectFilled(
			ImVec2(viewPos.x, centerY - bannerH * 0.5f),
			ImVec2(viewPos.x + screenW, centerY + bannerH * 0.5f),
			IM_COL32(4, 6, 12, static_cast<int>(245.0f * fadeAlpha * transitionAlpha))
		);

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

		// 1. "MISSION COMPLETE" タイトル描画（ポップアップ演出）
		const UITextItem* titleItem = textMgr ? textMgr->GetTextItem("ClearTitle", "GAMEPLAY") : nullptr;
		if (titleItem)
		{
			float popScale = 1.0f + 0.15f * (1.0f - fadeAlpha);
			renderTextItem(titleItem, fadeAlpha, popScale);
		}

		// 2. 最終スコア描画
		if (clearTimer_ >= 0.3f)
		{
			float scoreFade = std::clamp((clearTimer_ - 0.3f) / 0.4f, 0.0f, 1.0f);
			const UITextItem* scoreItem = textMgr ? textMgr->GetTextItem("ClearScore", "GAMEPLAY") : nullptr;
			if (scoreItem)
			{
				char scoreBuf[64];
				snprintf(scoreBuf, sizeof(scoreBuf), "SCORE: %06d", score_);
				renderTextItem(scoreItem, scoreFade, 1.0f, scoreBuf);
			}
		}

		// 3. タイトルへ戻る案内（呼吸点滅）
		if (clearTimer_ >= 0.6f)
		{
			float guideFade = std::clamp((clearTimer_ - 0.6f) / 0.4f, 0.0f, 1.0f);
			float pulse = 0.8f + 0.2f * std::sin(clearTimer_ * 5.0f);
			const UITextItem* returnItem = textMgr ? textMgr->GetTextItem("ClearReturn", "GAMEPLAY") : nullptr;
			if (returnItem)
			{
				renderTextItem(returnItem, guideFade * pulse);
			}
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

void GamePlayScene::StartBossWarningSequence()
{
	if (bossBattleStep_ != BossBattleStep::NOT_ACTIVE) return;

	bossBattleStep_ = BossBattleStep::WARNING_ALERT;
	bossWarningTimer_ = 0.0f;
	bossWarningSirenTimer_ = 0.0f;
	cameraShakeTimer_ = 0.6f;

	// アラーム音の再生
	SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.85f);

	// ボスをレール上の前方に出現スタンバイ
	if (armoredTrainBoss_)
	{
		armoredTrainBoss_->ResetChaseState();
		if (bossRail_ && bossRail_->IsValid())
		{
			// ボス専用レール（BossRail）を走行
			armoredTrainBoss_->SetRail(bossRail_.get());
			armoredTrainBoss_->SetRailProgress(0.0f);
		}
		else if (railCameraController_ && !mainRails_.empty())
		{
			// メインレールを前方追走
			float curProg = railCameraController_->GetProgress();
			float bossProg = (std::min)(0.98f, curProg + 0.12f);
			armoredTrainBoss_->SetRailProgress(bossProg);
			armoredTrainBoss_->SetRail(mainRails_[0].get());
		}
	}
}

void GamePlayScene::UpdateBossBattle(float dt)
{
	if (bossBattleStep_ == BossBattleStep::NOT_ACTIVE) return;

	if (bossBattleStep_ == BossBattleStep::WARNING_ALERT)
	{
		bossWarningTimer_ += dt;
		bossWarningSirenTimer_ += dt;

		// 0.85秒ごとにサイレンSEを反復再生
		if (bossWarningSirenTimer_ >= 0.85f && bossWarningTimer_ < kBossWarningDuration - 0.4f)
		{
			bossWarningSirenTimer_ = 0.0f;
			SoundManager::GetInstance()->PlaySE("Alarm01.wav", 0.85f);
		}

		// 警告時間終了 -> ボス戦開始！
		if (bossWarningTimer_ >= kBossWarningDuration)
		{
			bossBattleStep_ = BossBattleStep::BATTLE;
			isBossSpawned_ = true;
			if (armoredTrainBoss_)
			{
				armoredTrainBoss_->SetActive(true);
			}
			SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.9f);
			cameraShakeTimer_ = 0.5f;
		}
	}
	else if (bossBattleStep_ == BossBattleStep::BATTLE)
	{
		if (armoredTrainBoss_)
		{
			// HPゲージのスムーズ遅延追従（格闘ゲーム風のダメージ可視化演出）
			float currentHpRate = armoredTrainBoss_->GetTotalHpRate();
			if (bossDisplayedHpRate_ > currentHpRate)
			{
				bossDisplayedHpRate_ -= 0.35f * dt;
				if (bossDisplayedHpRate_ < currentHpRate)
				{
					bossDisplayedHpRate_ = currentHpRate;
				}
			}
			else
			{
				bossDisplayedHpRate_ = currentHpRate;
			}
		}
	}
	else if (bossBattleStep_ == BossBattleStep::DEFEATED_SEQUENCE)
	{
		bossDefeatTimer_ += dt;

		// 連続大爆発演出（0.12秒ごと）
		static float defeatExplodeSubTimer = 0.0f;
		defeatExplodeSubTimer += dt;
		if (defeatExplodeSubTimer >= 0.12f && armoredTrainBoss_)
		{
			defeatExplodeSubTimer = 0.0f;
			const auto& cars = armoredTrainBoss_->GetCarriages();
			if (!cars.empty())
			{
				int randCar = rand() % cars.size();
				if (cars[randCar].object)
				{
					Vector3 explodePos = cars[randCar].object->GetTranslate();
					explodePos.x += ((rand() % 100) / 50.0f - 1.0f) * 4.0f;
					explodePos.y += ((rand() % 100) / 50.0f - 1.0f) * 2.0f;
					explodePos.z += ((rand() % 100) / 50.0f - 1.0f) * 6.0f;
					explosionEffect_.SetPosition(explodePos);
					explosionEffect_.Play();
					SoundManager::GetInstance()->PlaySE("explosion.mp3", 0.85f);
					cameraShakeTimer_ = 0.25f;
				}
			}
		}

		// 撃破演出終了 -> ゲームクリア画面へ移行！
		if (bossDefeatTimer_ >= kBossDefeatDuration)
		{
			bossBattleStep_ = BossBattleStep::FINISHED;
			isBossSpawned_ = false;
			StartClearSequence();
		}
	}
}

void GamePlayScene::DrawBossUI()
{
#ifdef USE_IMGUI
	if (ImGui::GetCurrentContext() == nullptr) return;
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (!viewport) return;
	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	if (!drawList) return;

	// ゲーム画面領域（エディタモード／フルスクリーン対応）
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
	float scale = screenH / 720.0f;
	drawList->PushClipRect(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), true);

	auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();
	ImFont* fontMplus = imguiManager ? imguiManager->GetFont(ImGuiManager::FontType::Japanese_MPLUS) : ImGui::GetFont();
	if (!fontMplus) fontMplus = ImGui::GetFont();

	// 1. WARNING警告演出（赤フラッシュ、警告ストライプ、点滅テキスト）
	if (bossBattleStep_ == BossBattleStep::WARNING_ALERT)
	{
		float warnT = bossWarningTimer_;
		float pulse = 0.5f + 0.5f * std::sin(warnT * 12.0f); // 高速明滅パルス

		// 画面全体の赤色フラッシュ
		int flashAlpha = static_cast<int>(50.0f + 60.0f * pulse);
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(255, 20, 20, flashAlpha));

		// 上下の警告ストライプバー（高さ 44px）
		float barH = 44.0f * scale;
		// 上の警告帯
		drawList->AddRectFilled(viewPos, ImVec2(viewPos.x + screenW, viewPos.y + barH), IM_COL32(200, 20, 20, 230));
		// 下の警告帯
		drawList->AddRectFilled(ImVec2(viewPos.x, viewPos.y + screenH - barH), ImVec2(viewPos.x + screenW, viewPos.y + screenH), IM_COL32(200, 20, 20, 230));

		// 警告帯の斜線ストライプパターン
		float stripeW = 30.0f * scale;
		float stripeOffset = std::fmod(warnT * 80.0f * scale, stripeW * 2.0f);
		for (float x = -stripeW * 2.0f; x < screenW + stripeW * 2.0f; x += stripeW * 2.0f)
		{
			// 上部ストライプ
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x + stripeOffset, viewPos.y),
				ImVec2(viewPos.x + x + stripeOffset + stripeW * 0.6f, viewPos.y),
				ImVec2(viewPos.x + x + stripeOffset - stripeW * 0.4f, viewPos.y + barH),
				IM_COL32(20, 20, 20, 220)
			);
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x + stripeOffset + stripeW * 0.6f, viewPos.y),
				ImVec2(viewPos.x + x + stripeOffset + stripeW * 0.2f, viewPos.y + barH),
				ImVec2(viewPos.x + x + stripeOffset - stripeW * 0.4f, viewPos.y + barH),
				IM_COL32(20, 20, 20, 220)
			);
			// 下部ストライプ
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x - stripeOffset, viewPos.y + screenH - barH),
				ImVec2(viewPos.x + x - stripeOffset + stripeW * 0.6f, viewPos.y + screenH - barH),
				ImVec2(viewPos.x + x - stripeOffset - stripeW * 0.4f, viewPos.y + screenH),
				IM_COL32(20, 20, 20, 220)
			);
			drawList->AddTriangleFilled(
				ImVec2(viewPos.x + x - stripeOffset + stripeW * 0.6f, viewPos.y + screenH - barH),
				ImVec2(viewPos.x + x - stripeOffset + stripeW * 0.2f, viewPos.y + screenH),
				ImVec2(viewPos.x + x - stripeOffset - stripeW * 0.4f, viewPos.y + screenH),
				IM_COL32(20, 20, 20, 220)
			);
		}

		// 中央の「WARNING」テキスト描画
		const char* warnTitle = "WARNING";
		float titleFontSize = (54.0f + 6.0f * pulse) * scale;
		ImVec2 titleSize = fontMplus->CalcTextSizeA(titleFontSize, FLT_MAX, -1.0f, warnTitle);
		ImVec2 titlePos(viewPos.x + (screenW - titleSize.x) * 0.5f, viewPos.y + screenH * 0.40f - titleSize.y * 0.5f);

		// テキストシャドウ・アウトライン
		float thick = 3.0f * scale;
		for (float ox = -thick; ox <= thick; ox += thick)
		{
			for (float oy = -thick; oy <= thick; oy += thick)
			{
				drawList->AddText(fontMplus, titleFontSize, ImVec2(titlePos.x + ox, titlePos.y + oy), IM_COL32(0, 0, 0, 255), warnTitle);
			}
		}
		ImU32 warnCol = pulse > 0.3f ? IM_COL32(255, 40, 40, 255) : IM_COL32(255, 220, 50, 255);
		drawList->AddText(fontMplus, titleFontSize, titlePos, warnCol, warnTitle);

		// サブテキスト「EMERGENCY: MASSIVE ARMORED TRAIN DETECTED」
		const char* subText = "EMERGENCY: MASSIVE ARMORED TRAIN DETECTED";
		float subFontSize = 22.0f * scale;
		ImVec2 subSize = fontMplus->CalcTextSizeA(subFontSize, FLT_MAX, -1.0f, subText);
		ImVec2 subPos(viewPos.x + (screenW - subSize.x) * 0.5f, titlePos.y + titleSize.y + 12.0f * scale);
		for (float ox = -2.0f; ox <= 2.0f; ox += 2.0f)
		{
			for (float oy = -2.0f; oy <= 2.0f; oy += 2.0f)
			{
				drawList->AddText(fontMplus, subFontSize, ImVec2(subPos.x + ox, subPos.y + oy), IM_COL32(0, 0, 0, 220), subText);
			}
		}
		drawList->AddText(fontMplus, subFontSize, subPos, IM_COL32(255, 240, 240, 240), subText);
	}

	// 2. ボスHPゲージUI（戦闘中・撃破演出中）
	if ((bossBattleStep_ == BossBattleStep::BATTLE || bossBattleStep_ == BossBattleStep::DEFEATED_SEQUENCE) && armoredTrainBoss_)
	{
		float barW = 600.0f * scale;
		float barH = 18.0f * scale;
		float barX = viewPos.x + (screenW - barW) * 0.5f;
		float barY = viewPos.y + 36.0f * scale;

		float realHpRate = (std::clamp)(armoredTrainBoss_->GetTotalHpRate(), 0.0f, 1.0f);
		float delayHpRate = (std::clamp)(bossDisplayedHpRate_, 0.0f, 1.0f);

		// ボス名タイトル：「BOSS: ARMORED TRAIN "BEHEMOTH"」
		const char* bossTitle = "BOSS: ARMORED TRAIN \"BEHEMOTH\"";
		float bossTitleFontSize = 18.0f * scale;
		ImVec2 bossTitlePos(barX, barY - 24.0f * scale);
		// 黒アウトライン
		drawList->AddText(fontMplus, bossTitleFontSize, ImVec2(bossTitlePos.x + 1.5f, bossTitlePos.y + 1.5f), IM_COL32(0, 0, 0, 255), bossTitle);
		drawList->AddText(fontMplus, bossTitleFontSize, ImVec2(bossTitlePos.x - 1.5f, bossTitlePos.y - 1.5f), IM_COL32(0, 0, 0, 255), bossTitle);
		drawList->AddText(fontMplus, bossTitleFontSize, bossTitlePos, IM_COL32(255, 90, 80, 255), bossTitle);

		// HPパーセント表示（右揃え）
		char hpPercentBuf[32];
		snprintf(hpPercentBuf, sizeof(hpPercentBuf), "HP %3.0f%%", realHpRate * 100.0f);
		ImVec2 hpTxtSize = fontMplus->CalcTextSizeA(bossTitleFontSize, FLT_MAX, -1.0f, hpPercentBuf);
		ImVec2 hpTxtPos(barX + barW - hpTxtSize.x, barY - 24.0f * scale);
		drawList->AddText(fontMplus, bossTitleFontSize, ImVec2(hpTxtPos.x + 1.5f, hpTxtPos.y + 1.5f), IM_COL32(0, 0, 0, 255), hpPercentBuf);
		drawList->AddText(fontMplus, bossTitleFontSize, hpTxtPos, IM_COL32(255, 230, 200, 255), hpPercentBuf);

		// 外枠シャドウ
		drawList->AddRectFilled(ImVec2(barX - 3.0f, barY - 3.0f), ImVec2(barX + barW + 3.0f, barY + barH + 3.0f), IM_COL32(0, 0, 0, 200), 3.0f);
		// 背景
		drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(30, 32, 38, 240), 2.0f);

		// 遅延追従バー（黄色）
		if (delayHpRate > 0.001f)
		{
			drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * delayHpRate, barY + barH), IM_COL32(255, 200, 60, 220), 2.0f);
		}

		// メインHPバー（深紅〜赤グラデーション）
		if (realHpRate > 0.001f)
		{
			drawList->AddRectFilledMultiColor(
				ImVec2(barX, barY),
				ImVec2(barX + barW * realHpRate, barY + barH),
				IM_COL32(255, 50, 40, 255),
				IM_COL32(210, 20, 20, 255),
				IM_COL32(180, 10, 10, 255),
				IM_COL32(230, 40, 30, 255)
			);
		}

		// バー枠線
		drawList->AddRect(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(180, 180, 190, 220), 2.0f, 0, 1.5f);

		// 3. 各車両部位破壊インジケーター（4車両）
		const auto& cars = armoredTrainBoss_->GetCarriages();
		if (!cars.empty())
		{
			float indGap = 8.0f * scale;
			float indW = (barW - indGap * (cars.size() - 1)) / static_cast<float>(cars.size());
			float indH = 18.0f * scale;
			float indY = barY + barH + 6.0f * scale;

			for (size_t c = 0; c < cars.size(); ++c)
			{
				float cx = barX + c * (indW + indGap);
				bool destroyed = cars[c].isDestroyed;

				// 各車両枠背景
				ImU32 bgCol = destroyed ? IM_COL32(40, 20, 20, 180) : IM_COL32(25, 40, 50, 200);
				drawList->AddRectFilled(ImVec2(cx, indY), ImVec2(cx + indW, indY + indH), bgCol, 2.0f);

				// 車両個別HP率
				float carHpRate = cars[c].maxHp > 0 ? static_cast<float>(cars[c].hp) / cars[c].maxHp : 0.0f;
				if (!destroyed && carHpRate > 0.001f)
				{
					ImU32 carBarCol = (c == 0) ? IM_COL32(255, 120, 40, 200) : IM_COL32(50, 190, 220, 200);
					drawList->AddRectFilled(ImVec2(cx, indY), ImVec2(cx + indW * carHpRate, indY + indH), carBarCol, 2.0f);
				}

				// 車両枠線
				ImU32 borderCol = destroyed ? IM_COL32(150, 40, 40, 150) : IM_COL32(100, 180, 200, 200);
				drawList->AddRect(ImVec2(cx, indY), ImVec2(cx + indW, indY + indH), borderCol, 2.0f);

				// 車両名称略称表示
				const char* carShortName = "";
				if (c == 0) carShortName = "1:CORE 機関車";
				else if (c == 1) carShortName = "2:重砲塔車";
				else if (c == 2) carShortName = "3:ミサイル車";
				else if (c == 3) carShortName = "4:動力発電車";

				float cFontSize = 11.0f * scale;
				ImVec2 cTxtSize = fontMplus->CalcTextSizeA(cFontSize, FLT_MAX, -1.0f, carShortName);
				ImVec2 cTxtPos(cx + (indW - cTxtSize.x) * 0.5f, indY + (indH - cTxtSize.y) * 0.5f);

				if (destroyed)
				{
					const char* destText = "DESTROYED";
					ImVec2 dSize = fontMplus->CalcTextSizeA(cFontSize, FLT_MAX, -1.0f, destText);
					ImVec2 dPos(cx + (indW - dSize.x) * 0.5f, indY + (indH - dSize.y) * 0.5f);
					drawList->AddText(fontMplus, cFontSize, dPos, IM_COL32(255, 60, 60, 220), destText);
				}
				else
				{
					drawList->AddText(fontMplus, cFontSize, ImVec2(cTxtPos.x + 1.0f, cTxtPos.y + 1.0f), IM_COL32(0, 0, 0, 220), carShortName);
					drawList->AddText(fontMplus, cFontSize, cTxtPos, IM_COL32(230, 240, 250, 240), carShortName);
				}
			}
		}

		// 撃破演出中の「TARGET DESTROYED」テキスト表示
		if (bossBattleStep_ == BossBattleStep::DEFEATED_SEQUENCE)
		{
			const char* clearText = "TARGET DESTROYED";
			float dFontSize = 46.0f * scale;
			ImVec2 dSize = fontMplus->CalcTextSizeA(dFontSize, FLT_MAX, -1.0f, clearText);
			ImVec2 dPos(viewPos.x + (screenW - dSize.x) * 0.5f, viewPos.y + screenH * 0.42f);

			for (float ox = -3.0f; ox <= 3.0f; ox += 3.0f)
			{
				for (float oy = -3.0f; oy <= 3.0f; oy += 3.0f)
				{
					drawList->AddText(fontMplus, dFontSize, ImVec2(dPos.x + ox, dPos.y + oy), IM_COL32(0, 0, 0, 255), clearText);
				}
			}
			drawList->AddText(fontMplus, dFontSize, dPos, IM_COL32(255, 215, 0, 255), clearText);
		}
	}

	drawList->PopClipRect();
#endif
}

void GamePlayScene::LoadBossSettings()
{
	std::string filepath = "resources/json/settings/boss_settings.json";
	std::ifstream file(filepath);
	if (!file.is_open()) return;

	try
	{
		nlohmann::json root;
		file >> root;
		if (root.contains("boss_spawn_progress"))
		{
			bossSpawnProgressThreshold_ = root["boss_spawn_progress"].get<float>();
		}
		if (armoredTrainBoss_)
		{
			if (root.contains("is_follow_player"))
			{
				armoredTrainBoss_->SetFollowPlayer(root["is_follow_player"].get<bool>());
			}
			if (root.contains("boss_scale"))
			{
				armoredTrainBoss_->SetScaleMultiplier(root["boss_scale"].get<float>());
			}
			if (root.contains("desired_lead_distance"))
			{
				armoredTrainBoss_->SetDesiredLeadDistance(root["desired_lead_distance"].get<float>());
			}
			if (root.contains("is_random_lead_enabled"))
			{
				armoredTrainBoss_->SetRandomLeadDistanceEnabled(root["is_random_lead_enabled"].get<bool>());
			}
			if (root.contains("rush_speed_bonus"))
			{
				armoredTrainBoss_->SetRushSpeedBonus(root["rush_speed_bonus"].get<float>());
			}
			if (root.contains("is_ground_snap_enabled"))
			{
				armoredTrainBoss_->SetGroundSnapEnabled(root["is_ground_snap_enabled"].get<bool>());
			}
			if (root.contains("ground_snap_offset"))
			{
				armoredTrainBoss_->SetGroundSnapOffset(root["ground_snap_offset"].get<float>());
			}
		}
	}
	catch (...)
	{
	}
}

void GamePlayScene::SaveBossSettings()
{
	std::string dir = "resources/json/settings";
	std::filesystem::create_directories(dir);
	std::string filepath = dir + "/boss_settings.json";
	std::ofstream file(filepath);
	if (!file.is_open()) return;

	nlohmann::json root;
	root["boss_spawn_progress"] = bossSpawnProgressThreshold_;
	if (armoredTrainBoss_)
	{
		root["boss_scale"] = armoredTrainBoss_->GetScaleMultiplier();
		root["is_follow_player"] = armoredTrainBoss_->IsFollowPlayer();
		root["desired_lead_distance"] = armoredTrainBoss_->GetDesiredLeadDistance();
		root["is_random_lead_enabled"] = armoredTrainBoss_->IsRandomLeadDistanceEnabled();
		root["rush_speed_bonus"] = armoredTrainBoss_->GetRushSpeedBonus();
		root["is_ground_snap_enabled"] = armoredTrainBoss_->IsGroundSnapEnabled();
		root["ground_snap_offset"] = armoredTrainBoss_->GetGroundSnapOffset();
	}

	file << root.dump(4);
}
