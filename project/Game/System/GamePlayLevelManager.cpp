#define NOMINMAX
#include "GamePlayLevelManager.h"
#include "Game/Actor/Player/Player.h"
#include "Game/Actor/Enemy/Enemy.h"
#include "Game/Actor/Enemy/ArmoredTrainBoss.h"
#include "Game/Actor/Enemy/EnemyPresetManager.h"
#include "Game/Actor/Obstacle/Obstacle.h"
#include "Game/Actor/Item/EnhanceRing.h"
#include "Game/Actor/Item/MemoryItem.h"
#include "Game/System/Rail.h"
#include "Game/System/RailCameraController.h"
#include "Game/System/BossBattleController.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Graphics/3d/Object/Object3d.h"
#include "KHEngine/Graphics/3d/Model/Model.h"
#include "KHEngine/Graphics/3d/Model/ModelManager.h"
#include "KHEngine/Graphics/Resource/Texture/TextureManager.h"
#include "KHEngine/Graphics/3d/Skybox/Skybox.h"
#include "KHEngine/Graphics/3d/Camera/Camera.h"
#include "KHEngine/Scene/LevelLoader.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <Windows.h>

GamePlayLevelManager::GamePlayLevelManager() = default;
GamePlayLevelManager::~GamePlayLevelManager() = default;

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


void GamePlayLevelManager::PreloadResources()
{
	auto texManager = TextureManager::GetInstance();
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

}

void GamePlayLevelManager::RespawnActorsFromCache(const LevelLoadContext& ctx)
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services ? services->GetObject3dCommon() : nullptr;
	if (cachedLevelData_ && object3dCommon && ctx.skybox)
	{
		for (const auto& objData : cachedLevelData_->objects)
		{
			LoadEnemiesOnlyFromNode(objData, object3dCommon, ctx.skybox->GetCubemapSrvIndex(), *ctx.enemies, *ctx.obstacles, *ctx.enhanceRings);
		}
		ApplyVirtualSpaceStyle(*ctx.modelInstances, *ctx.obstacles, ctx.skybox, ctx.isVirtualSpace);
	}
}

void GamePlayLevelManager::ReloadLevel(const LevelLoadContext& ctx)
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();


	while ((*ctx.modelInstances).size() > 1)
	{
		(*ctx.modelInstances).pop_back();
	}

	(*ctx.enemies).clear();
	(*ctx.obstacles).clear();


	(*ctx.railVisualizers).clear();
	(*ctx.enemyRailVisualizers).clear();
	if ((*ctx.railCameraController))
	{
		// 解放される古いレールへのダングリングポインタアクセスを完全に防止
		(*ctx.railCameraController)->ClearRails();
	}
	(*ctx.mainRails).clear();
	(*ctx.bossRail).reset();

	if (ctx.player)
	{
		ctx.player->SetPowerUpLevel(0);
	}

	// ボス戦状態のリセット
	ctx.bossBattle->Reset();
	(*ctx.isBossSpawned) = false;
	if ((*ctx.railCameraController))
	{
		(*ctx.railCameraController)->SetBaseSpeed(50.0f);
	}
	if ((*ctx.armoredTrainBoss))
	{
		(*ctx.armoredTrainBoss)->SetActive(false);
		(*ctx.armoredTrainBoss)->SetRail(nullptr);
		(*ctx.armoredTrainBoss)->SetTerrainObjects(nullptr);
	}


	auto levelData = LevelLoader::Load("resources/json/maps/template/template.json");
	if (levelData)
	{
		cachedLevelData_ = std::move(levelData);
		for (const auto& objData : cachedLevelData_->objects)
		{
			CreateObjectFromNode(objData, nullptr, (*ctx.modelInstances), (*ctx.mainRails), (*ctx.bossRail), ctx.bossBattle->SpawnProgressThreshold(), object3dCommon, ctx.skybox->GetCubemapSrvIndex(), (*ctx.enemies), (*ctx.obstacles), (*ctx.enhanceRings));
		}
		ApplyVirtualSpaceStyle((*ctx.modelInstances), (*ctx.obstacles), ctx.skybox, ctx.isVirtualSpace);
		OutputDebugStringA("LevelLoader: Successfully reloaded objects.\n");

		(*ctx.totalEnemiesCount) = static_cast<int>((*ctx.enemies).size());
		(*ctx.killedEnemiesCount) = 0;



		// BossRailが存在する場合、マップからの指定がなければ始点近傍のメインレール進行度を自動算出
		if ((*ctx.bossRail) && (*ctx.bossRail)->IsValid() && !(*ctx.mainRails).empty() && (*ctx.mainRails)[0]->IsValid())
		{
			if (ctx.bossBattle->SpawnProgressThreshold() == 0.60f)
			{
				Vector3 bossStartPos = (*ctx.bossRail)->GetPosition(0.0f);
				ctx.bossBattle->SpawnProgressThreshold() = (*ctx.mainRails)[0]->GetClosestProgress(bossStartPos);
			}
		}

		// 保存された設定ファイル（boss_settings.json）があれば読み込み（手動保存設定を優先）
		ctx.bossBattle->LoadSettings((*ctx.armoredTrainBoss).get());
		if ((*ctx.armoredTrainBoss))
		{
			(*ctx.armoredTrainBoss)->SetTerrainObjects(&(*ctx.modelInstances));
		}


		if ((*ctx.railCameraController))
		{
			std::vector<Rail*> railsRaw;
			for (auto& r : (*ctx.mainRails))
			{
				if (r && r->IsValid())
				{
					railsRaw.push_back(r.get());
				}
			}
			(*ctx.railCameraController)->Initialize(railsRaw, ctx.activeCamera, ctx.cameraObject);
		}
	}
	else
	{
		OutputDebugStringA("LevelLoader: Failed to reload level.\n");
	}


	(*ctx.railVisualizers).clear();
	(*ctx.railModels).clear();
	if (!(*ctx.mainRails).empty())
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
		for (auto& rail : (*ctx.mainRails))
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
				obj->SetEnvironmentTextureIndex(ctx.skybox->GetCubemapSrvIndex());

				(*ctx.railVisualizers).push_back(std::move(obj));
			}

			(*ctx.railModels).push_back(std::move(railModel));
			colorIndex++;
		}
	}


	(*ctx.enemyRailModel) = std::make_unique<Model>();
	std::string resolved = ResourceLocator::Resolve("rail.obj", ResourceLocator::AssetType::Model3D);
	if (!resolved.empty())
	{
		std::filesystem::path rp(reinterpret_cast<const char8_t*>(resolved.c_str()));
		(*ctx.enemyRailModel)->Initialize(ModelManager::GetInstance()->GetModelCommon(), rp.parent_path().string(), rp.filename().string());
	}
	else
	{
		(*ctx.enemyRailModel)->Initialize(ModelManager::GetInstance()->GetModelCommon(), "resources/models", "rail.obj");
	}
	(*ctx.enemyRailModel)->SetColor({ 0.5f, 1.0f, 0.0f, 1.0f });

	int sampleCount = 100;
	for (auto& enemy : (*ctx.enemies))
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
			obj->SetModel((*ctx.enemyRailModel).get());
			obj->SetTranslate(center);
			float yaw = std::atan2(dir.x, dir.z);
			float pitch = std::asin(-dir.y);
			obj->SetRotation(Vector3(pitch, yaw, 0.0f));

			obj->SetScale(Vector3(0.015f, 0.015f, length));

			obj->SetEnvironmentCoefficient(0.0f);
			obj->SetEnvironmentTextureIndex(ctx.skybox->GetCubemapSrvIndex());

			(*ctx.enemyRailVisualizers).push_back(std::move(obj));
		}
	}

	// レベル再読込時も降下演出を開始
	
}


void GamePlayLevelManager::ApplyVirtualSpaceStyle(const std::vector<std::unique_ptr<Object3d>>& modelInstances, const std::list<std::unique_ptr<Obstacle>>& obstacles, Skybox* skybox, bool isVirtualSpace)
{
	if (skybox)
	{
		skybox->SetVirtualSpace(isVirtualSpace);
	}
	int32_t lightingMode = isVirtualSpace ? 7 : 1;
	for (auto& obj : modelInstances)
	{
		if (obj)
		{
			obj->SetSelectLightings(lightingMode);
		}
	}
	for (auto& obs : obstacles)
	{
		if (obs && obs->GetObject3d())
		{
			obs->GetObject3d()->SetSelectLightings(lightingMode);
		}
	}
}


void GamePlayLevelManager::ReloadEnemiesOnly(const LevelLoadContext& ctx)
{
	auto services = EngineServices::GetInstance();
	auto object3dCommon = services->GetObject3dCommon();

	(*ctx.enemies).clear();
	(*ctx.obstacles).clear();
	(*ctx.enemyRailVisualizers).clear();

	auto levelData = LevelLoader::Load("resources/json/maps/template/template.json");
	if (levelData)
	{
		for (const auto& objData : levelData->objects)
		{
			LoadEnemiesOnlyFromNode(objData, object3dCommon, ctx.skybox->GetCubemapSrvIndex(), (*ctx.enemies), (*ctx.obstacles), (*ctx.enhanceRings));
		}
		ApplyVirtualSpaceStyle((*ctx.modelInstances), (*ctx.obstacles), ctx.skybox, ctx.isVirtualSpace);
		OutputDebugStringA("LevelLoader: Successfully respawned enemies.\n");


		if ((*ctx.enemyRailModel))
		{
			int sampleCount = 100;
			for (auto& enemy : (*ctx.enemies))
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
					obj->SetModel((*ctx.enemyRailModel).get());
					obj->SetTranslate(center);
					float yaw = std::atan2(dir.x, dir.z);
					float pitch = std::asin(-dir.y);
					obj->SetRotation(Vector3(pitch, yaw, 0.0f));
					obj->SetScale(Vector3(0.015f, 0.015f, length));
					obj->SetEnvironmentCoefficient(0.0f);
					obj->SetEnvironmentTextureIndex(ctx.skybox->GetCubemapSrvIndex());
					(*ctx.enemyRailVisualizers).push_back(std::move(obj));
				}
			}
		}
	}
	else
	{
		OutputDebugStringA("LevelLoader: Failed to respawn enemies.\n");
	}
}
