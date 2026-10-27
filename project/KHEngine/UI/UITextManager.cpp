#include "UITextManager.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include "KHEngine/Scene/SceneManager.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>
#include <filesystem>
#include <cfloat>
#include <algorithm>

UITextManager* UITextManager::GetInstance()
{
	static UITextManager instance;
	return &instance;
}

void UITextManager::Initialize(const std::string& settingsFilePath)
{
	settingsFilePath_ = settingsFilePath;
	LoadSettings();
	SetupDefaultScenes();
}

SceneTextContainer& UITextManager::GetOrCreateScene(const std::string& sceneName)
{
	std::string name = sceneName.empty() ? currentScene_ : sceneName;
	auto it = scenes_.find(name);
	if (it == scenes_.end())
	{
		scenes_[name] = SceneTextContainer();
		if (std::find(knownScenes_.begin(), knownScenes_.end(), name) == knownScenes_.end())
		{
			knownScenes_.push_back(name);
		}
	}
	return scenes_[name];
}

void UITextManager::SetCurrentScene(const std::string& sceneName)
{
	if (sceneName.empty()) return;
	currentScene_ = sceneName;
	hudAlpha_ = 1.0f;
	GetOrCreateScene(currentScene_);

	if (followActiveScene_)
	{
		editingScene_ = currentScene_;
		selectedItemIndex_ = 0;
	}
}

void UITextManager::SetEditingScene(const std::string& sceneName)
{
	if (sceneName.empty()) return;
	editingScene_ = sceneName;
	GetOrCreateScene(editingScene_);
	selectedItemIndex_ = 0;
}

void UITextManager::SetupDefaultScenes()
{
	// 1. TITLE
	auto& titleScene = GetOrCreateScene("TITLE");
	if (titleScene.items.empty())
	{
		UITextItem titleMain;
		titleMain.id = "TitleMain";
		titleMain.text = "RAIL SHOOTER";
		titleMain.position = { 640.0f, 220.0f };
		titleMain.fontSize = 52.0f;
		titleMain.fontType = ImGuiManager::FontType::Japanese_MPLUS_Large;
		titleMain.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		titleMain.hasOutline = true;
		titleMain.outlineColor = { 0.0f, 0.2f, 0.4f, 1.0f };
		titleMain.outlineThickness = 2.5f;
		titleMain.hasShadow = true;
		titleMain.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		titleMain.shadowOffset = { 3.0f, 3.0f };
		titleMain.align = UITextAlign::Center;
		titleScene.items.push_back(titleMain);
		titleScene.itemMap[titleMain.id] = 0;

		UITextItem startPrompt;
		startPrompt.id = "StartPrompt";
		startPrompt.text = "PRESS SPACE TO START";
		startPrompt.position = { 640.0f, 520.0f };
		startPrompt.fontSize = 24.0f;
		startPrompt.fontType = ImGuiManager::FontType::English_FiraMono;
		startPrompt.color = { 0.3f, 0.85f, 1.0f, 1.0f };
		startPrompt.hasOutline = true;
		startPrompt.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		startPrompt.outlineThickness = 2.0f;
		startPrompt.align = UITextAlign::Center;
		titleScene.items.push_back(startPrompt);
		titleScene.itemMap[startPrompt.id] = 1;
	}

	// 2. GAMEPLAY
	auto& gameplayScene = GetOrCreateScene("GAMEPLAY");
	if (gameplayScene.items.empty())
	{
		UITextItem scoreItem;
		scoreItem.id = "Score";
		scoreItem.text = "SCORE: 000000";
		scoreItem.position = { 40.0f, 30.0f };
		scoreItem.fontSize = 28.0f;
		scoreItem.fontType = ImGuiManager::FontType::English_FiraMono;
		scoreItem.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		scoreItem.hasOutline = true;
		scoreItem.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		scoreItem.outlineThickness = 2.0f;
		scoreItem.hasShadow = true;
		scoreItem.shadowColor = { 0.0f, 0.0f, 0.0f, 0.6f };
		scoreItem.shadowOffset = { 2.0f, 2.0f };
		scoreItem.align = UITextAlign::Left;
		gameplayScene.items.push_back(scoreItem);
		gameplayScene.itemMap[scoreItem.id] = 0;

		UITextItem lockOnItem;
		lockOnItem.id = "LockOn";
		lockOnItem.text = "LOCK ON: 0 / 4";
		lockOnItem.position = { 40.0f, 65.0f };
		lockOnItem.fontSize = 22.0f;
		lockOnItem.fontType = ImGuiManager::FontType::English_FiraMono;
		lockOnItem.color = { 0.2f, 0.9f, 1.0f, 1.0f };
		lockOnItem.hasOutline = true;
		lockOnItem.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		lockOnItem.outlineThickness = 2.0f;
		lockOnItem.align = UITextAlign::Left;
		gameplayScene.items.push_back(lockOnItem);
		gameplayScene.itemMap[lockOnItem.id] = 1;

		UITextItem hpItem;
		hpItem.id = "PlayerHP";
		hpItem.text = "SHIELD / HP";
		hpItem.position = { 20.0f, 635.0f };
		hpItem.fontSize = 18.0f;
		hpItem.fontType = ImGuiManager::FontType::Japanese_MPLUS;
		hpItem.color = { 0.3f, 1.0f, 0.5f, 1.0f };
		hpItem.hasOutline = true;
		hpItem.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		hpItem.outlineThickness = 1.5f;
		hpItem.align = UITextAlign::Left;
		gameplayScene.items.push_back(hpItem);
		gameplayScene.itemMap[hpItem.id] = 2;

		UITextItem missionItem;
		missionItem.id = "MissionAlert";
		missionItem.text = "敵部隊ヲ殲滅セヨ";
		missionItem.position = { 640.0f, 100.0f };
		missionItem.fontSize = 32.0f;
		missionItem.fontType = ImGuiManager::FontType::Japanese_MPLUS_Large;
		missionItem.color = { 1.0f, 0.85f, 0.2f, 1.0f };
		missionItem.hasOutline = true;
		missionItem.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		missionItem.outlineThickness = 2.5f;
		missionItem.hasShadow = true;
		missionItem.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		missionItem.shadowOffset = { 3.0f, 3.0f };
		missionItem.align = UITextAlign::Center;
		missionItem.isVisible = false;
		gameplayScene.items.push_back(missionItem);
		gameplayScene.itemMap[missionItem.id] = 3;

		UITextItem readyItem;
		readyItem.id = "CutsceneReady";
		readyItem.text = "READY...";
		readyItem.position = { 640.0f, 280.0f };
		readyItem.fontSize = 42.0f;
		readyItem.fontType = ImGuiManager::FontType::English_FiraMono_Large;
		readyItem.color = { 1.0f, 0.9f, 0.3f, 1.0f };
		readyItem.hasOutline = true;
		readyItem.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		readyItem.outlineThickness = 2.5f;
		readyItem.hasShadow = true;
		readyItem.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		readyItem.shadowOffset = { 2.0f, 2.0f };
		readyItem.align = UITextAlign::Center;
		readyItem.isVisible = false;
		gameplayScene.items.push_back(readyItem);
		gameplayScene.itemMap[readyItem.id] = 4;

		UITextItem missionStartItem;
		missionStartItem.id = "CutsceneMissionStart";
		missionStartItem.text = "MISSION START!";
		missionStartItem.position = { 640.0f, 270.0f };
		missionStartItem.fontSize = 56.0f;
		missionStartItem.fontType = ImGuiManager::FontType::English_FiraMono_Large;
		missionStartItem.color = { 0.24f, 1.0f, 0.55f, 1.0f };
		missionStartItem.hasOutline = true;
		missionStartItem.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		missionStartItem.outlineThickness = 3.0f;
		missionStartItem.hasShadow = true;
		missionStartItem.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		missionStartItem.shadowOffset = { 3.0f, 3.0f };
		missionStartItem.align = UITextAlign::Center;
		missionStartItem.isVisible = false;
		gameplayScene.items.push_back(missionStartItem);
		gameplayScene.itemMap[missionStartItem.id] = 5;

		UITextItem gameOverTitle;
		gameOverTitle.id = "GameOverTitle";
		gameOverTitle.text = "GAME OVER";
		gameOverTitle.position = { 640.0f, 275.0f };
		gameOverTitle.fontSize = 64.0f;
		gameOverTitle.fontType = ImGuiManager::FontType::English_FiraMono_Large;
		gameOverTitle.color = { 0.20f, 0.98f, 1.00f, 1.0f };
		gameOverTitle.hasOutline = true;
		gameOverTitle.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		gameOverTitle.outlineThickness = 3.0f;
		gameOverTitle.hasShadow = true;
		gameOverTitle.shadowColor = { 0.02f, 0.20f, 0.35f, 0.85f };
		gameOverTitle.shadowOffset = { 3.0f, 3.0f };
		gameOverTitle.hasUnderline = true;
		gameOverTitle.underlineColor = { 0.25f, 0.98f, 1.00f, 0.90f };
		gameOverTitle.underlineWidth = 320.0f;
		gameOverTitle.underlineThickness = 2.5f;
		gameOverTitle.underlineOffsetY = 42.0f;
		gameOverTitle.align = UITextAlign::Center;
		gameOverTitle.isVisible = false;
		gameplayScene.items.push_back(gameOverTitle);
		gameplayScene.itemMap[gameOverTitle.id] = 6;

		UITextItem gameOverRetry;
		gameOverRetry.id = "GameOverRetry";
		gameOverRetry.text = "RETRY MISSION";
		gameOverRetry.position = { 640.0f, 420.0f };
		gameOverRetry.fontSize = 28.0f;
		gameOverRetry.fontType = ImGuiManager::FontType::English_FiraMono;
		gameOverRetry.color = { 1.0f, 0.9f, 0.6f, 1.0f };
		gameOverRetry.hasOutline = true;
		gameOverRetry.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		gameOverRetry.outlineThickness = 2.0f;
		gameOverRetry.hasShadow = true;
		gameOverRetry.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		gameOverRetry.shadowOffset = { 2.0f, 2.0f };
		gameOverRetry.align = UITextAlign::Center;
		gameOverRetry.isVisible = false;
		gameplayScene.items.push_back(gameOverRetry);
		gameplayScene.itemMap[gameOverRetry.id] = 7;

		UITextItem gameOverTitleNav;
		gameOverTitleNav.id = "GameOverTitleNav";
		gameOverTitleNav.text = "RETURN TO TITLE";
		gameOverTitleNav.position = { 640.0f, 470.0f };
		gameOverTitleNav.fontSize = 24.0f;
		gameOverTitleNav.fontType = ImGuiManager::FontType::English_FiraMono;
		gameOverTitleNav.color = { 0.7f, 0.75f, 0.8f, 1.0f };
		gameOverTitleNav.hasOutline = true;
		gameOverTitleNav.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		gameOverTitleNav.outlineThickness = 2.0f;
		gameOverTitleNav.hasShadow = true;
		gameOverTitleNav.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		gameOverTitleNav.shadowOffset = { 2.0f, 2.0f };
		gameOverTitleNav.align = UITextAlign::Center;
		gameOverTitleNav.isVisible = false;
		gameplayScene.items.push_back(gameOverTitleNav);
		gameplayScene.itemMap[gameOverTitleNav.id] = 8;

		UITextItem clearTitle;
		clearTitle.id = "ClearTitle";
		clearTitle.text = "MISSION COMPLETE";
		clearTitle.position = { 640.0f, 260.0f };
		clearTitle.fontSize = 54.0f;
		clearTitle.fontType = ImGuiManager::FontType::English_FiraMono_Large;
		clearTitle.color = { 1.0f, 0.85f, 0.2f, 1.0f };
		clearTitle.hasOutline = true;
		clearTitle.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearTitle.outlineThickness = 3.0f;
		clearTitle.hasShadow = true;
		clearTitle.shadowColor = { 0.0f, 0.0f, 0.0f, 0.8f };
		clearTitle.shadowOffset = { 3.0f, 3.0f };
		clearTitle.align = UITextAlign::Center;
		clearTitle.isVisible = false;
		gameplayScene.items.push_back(clearTitle);
		gameplayScene.itemMap[clearTitle.id] = 9;

		UITextItem clearScore;
		clearScore.id = "ClearScore";
		clearScore.text = "SCORE: 000000";
		clearScore.position = { 640.0f, 360.0f };
		clearScore.fontSize = 32.0f;
		clearScore.fontType = ImGuiManager::FontType::English_FiraMono;
		clearScore.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		clearScore.hasOutline = true;
		clearScore.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearScore.outlineThickness = 2.0f;
		clearScore.hasShadow = true;
		clearScore.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		clearScore.shadowOffset = { 2.0f, 2.0f };
		clearScore.align = UITextAlign::Center;
		clearScore.isVisible = false;
		gameplayScene.items.push_back(clearScore);
		gameplayScene.itemMap[clearScore.id] = 10;

		UITextItem clearReturn;
		clearReturn.id = "ClearReturn";
		clearReturn.text = "PRESS SPACE TO RETURN TO TITLE";
		clearReturn.position = { 640.0f, 520.0f };
		clearReturn.fontSize = 24.0f;
		clearReturn.fontType = ImGuiManager::FontType::English_FiraMono;
		clearReturn.color = { 0.85f, 0.9f, 0.95f, 1.0f };
		clearReturn.hasOutline = true;
		clearReturn.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearReturn.outlineThickness = 2.0f;
		clearReturn.hasShadow = true;
		clearReturn.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		clearReturn.shadowOffset = { 2.0f, 2.0f };
		clearReturn.align = UITextAlign::Center;
		clearReturn.isVisible = false;
		gameplayScene.items.push_back(clearReturn);
		gameplayScene.itemMap[clearReturn.id] = 11;
	}

	// 3. GAMECLEAR
	auto& clearScene = GetOrCreateScene("GAMECLEAR");
	if (clearScene.items.empty())
	{
		UITextItem clearTitle;
		clearTitle.id = "ClearTitle";
		clearTitle.text = "MISSION COMPLETE";
		clearTitle.position = { 640.0f, 240.0f };
		clearTitle.fontSize = 52.0f;
		clearTitle.fontType = ImGuiManager::FontType::Japanese_MPLUS_Large;
		clearTitle.color = { 1.0f, 0.85f, 0.2f, 1.0f };
		clearTitle.hasOutline = true;
		clearTitle.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearTitle.outlineThickness = 2.5f;
		clearTitle.hasShadow = true;
		clearTitle.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		clearTitle.shadowOffset = { 3.0f, 3.0f };
		clearTitle.align = UITextAlign::Center;
		clearScene.items.push_back(clearTitle);
		clearScene.itemMap[clearTitle.id] = 0;

		UITextItem clearReturn;
		clearReturn.id = "ClearReturn";
		clearReturn.text = "PRESS SPACE TO RETURN TO TITLE";
		clearReturn.position = { 640.0f, 520.0f };
		clearReturn.fontSize = 24.0f;
		clearReturn.fontType = ImGuiManager::FontType::English_FiraMono;
		clearReturn.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		clearReturn.hasOutline = true;
		clearReturn.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearReturn.outlineThickness = 2.0f;
		clearReturn.align = UITextAlign::Center;
		clearScene.items.push_back(clearReturn);
		clearScene.itemMap[clearReturn.id] = 1;
	}

	// 4. GAMEOVER
	auto& overScene = GetOrCreateScene("GAMEOVER");
	if (overScene.items.empty())
	{
		UITextItem overTitle;
		overTitle.id = "GameOverTitle";
		overTitle.text = "GAME OVER";
		overTitle.position = { 640.0f, 240.0f };
		overTitle.fontSize = 56.0f;
		overTitle.fontType = ImGuiManager::FontType::Japanese_MPLUS_Large;
		overTitle.color = { 1.0f, 0.2f, 0.2f, 1.0f };
		overTitle.hasOutline = true;
		overTitle.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		overTitle.outlineThickness = 2.5f;
		overTitle.hasShadow = true;
		overTitle.shadowColor = { 0.0f, 0.0f, 0.0f, 0.7f };
		overTitle.shadowOffset = { 3.0f, 3.0f };
		overTitle.hasUnderline = true;
		overTitle.underlineColor = { 0.92f, 0.12f, 0.18f, 0.86f };
		overTitle.underlineWidth = 320.0f;
		overTitle.underlineThickness = 2.5f;
		overTitle.underlineOffsetY = 42.0f;
		overTitle.align = UITextAlign::Center;
		overScene.items.push_back(overTitle);
		overScene.itemMap[overTitle.id] = 0;

		UITextItem overRetry;
		overRetry.id = "GameOverRetry";
		overRetry.text = "PRESS SPACE TO RETRY";
		overRetry.position = { 640.0f, 520.0f };
		overRetry.fontSize = 24.0f;
		overRetry.fontType = ImGuiManager::FontType::English_FiraMono;
		overRetry.color = { 1.0f, 1.0f, 1.0f, 1.0f };
		overRetry.hasOutline = true;
		overRetry.outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		overRetry.outlineThickness = 2.0f;
		overRetry.align = UITextAlign::Center;
		overScene.items.push_back(overRetry);
		overScene.itemMap[overRetry.id] = 1;
	}
}

void UITextManager::RegisterText(const UITextItem& item, const std::string& sceneName)
{
	auto& scene = GetOrCreateScene(sceneName);
	auto it = scene.itemMap.find(item.id);
	if (it != scene.itemMap.end())
	{
		scene.items[it->second].text = item.text;
		return;
	}

	size_t newIndex = scene.items.size();
	scene.items.push_back(item);
	scene.itemMap[item.id] = newIndex;
}

void UITextManager::SetText(const std::string& id, const std::string& text, const std::string& sceneName)
{
	auto& scene = GetOrCreateScene(sceneName);
	auto it = scene.itemMap.find(id);
	if (it != scene.itemMap.end())
	{
		scene.items[it->second].text = text;
	}
	else
	{
		UITextItem newItem;
		newItem.id = id;
		newItem.text = text;
		RegisterText(newItem, sceneName);
	}
}

void UITextManager::SetVisible(const std::string& id, bool isVisible, const std::string& sceneName)
{
	auto& scene = GetOrCreateScene(sceneName);
	auto it = scene.itemMap.find(id);
	if (it != scene.itemMap.end())
	{
		scene.items[it->second].isVisible = isVisible;
	}
}

void UITextManager::SetPosition(const std::string& id, const Vector2& position, const std::string& sceneName)
{
	auto& scene = GetOrCreateScene(sceneName);
	auto it = scene.itemMap.find(id);
	if (it != scene.itemMap.end())
	{
		scene.items[it->second].position = position;
	}
}

void UITextManager::SetColor(const std::string& id, const Vector4& color, const std::string& sceneName)
{
	auto& scene = GetOrCreateScene(sceneName);
	auto it = scene.itemMap.find(id);
	if (it != scene.itemMap.end())
	{
		scene.items[it->second].color = color;
	}
}

UITextItem* UITextManager::GetTextItem(const std::string& id, const std::string& sceneName)
{
	auto& scene = GetOrCreateScene(sceneName);
	auto it = scene.itemMap.find(id);
	if (it != scene.itemMap.end())
	{
		return &scene.items[it->second];
	}
	return nullptr;
}

bool UITextManager::HasTextItem(const std::string& id, const std::string& sceneName) const
{
	std::string sName = sceneName.empty() ? currentScene_ : sceneName;
	auto sIt = scenes_.find(sName);
	if (sIt != scenes_.end())
	{
		return sIt->second.itemMap.find(id) != sIt->second.itemMap.end();
	}
	return false;
}

void UITextManager::SaveSettings()
{
	try
	{
		std::filesystem::path path(settingsFilePath_);
		if (!path.parent_path().empty() && !std::filesystem::exists(path.parent_path()))
		{
			std::filesystem::create_directories(path.parent_path());
		}

		nlohmann::json root = nlohmann::json::object();
		nlohmann::json scenesObj = nlohmann::json::object();

		for (const auto& [sName, sData] : scenes_)
		{
			nlohmann::json itemArray = nlohmann::json::array();
			for (const auto& item : sData.items)
			{
				nlohmann::json j;
				j["id"] = item.id;
				j["text"] = item.text;
				j["posX"] = item.position.x;
				j["posY"] = item.position.y;
				j["fontSize"] = item.fontSize;
				j["fontType"] = static_cast<int>(item.fontType);
				j["color"] = { item.color.x, item.color.y, item.color.z, item.color.w };
				j["hasOutline"] = item.hasOutline;
				j["outlineColor"] = { item.outlineColor.x, item.outlineColor.y, item.outlineColor.z, item.outlineColor.w };
				j["outlineThickness"] = item.outlineThickness;
				j["hasShadow"] = item.hasShadow;
				j["shadowColor"] = { item.shadowColor.x, item.shadowColor.y, item.shadowColor.z, item.shadowColor.w };
				j["shadowOffset"] = { item.shadowOffset.x, item.shadowOffset.y };
				j["hasUnderline"] = item.hasUnderline;
				j["underlineColor"] = { item.underlineColor.x, item.underlineColor.y, item.underlineColor.z, item.underlineColor.w };
				j["underlineWidth"] = item.underlineWidth;
				j["underlineThickness"] = item.underlineThickness;
				j["underlineOffsetY"] = item.underlineOffsetY;
				j["align"] = static_cast<int>(item.align);
				j["isVisible"] = item.isVisible;

				itemArray.push_back(j);
			}
			scenesObj[sName] = itemArray;
		}

		root["scenes"] = scenesObj;

		std::ofstream file(settingsFilePath_);
		if (file.is_open())
		{
			file << root.dump(4);
			file.close();
		}
	}
	catch (...)
	{
	}
}

void UITextManager::LoadSettings()
{
	try
	{
		if (!std::filesystem::exists(settingsFilePath_))
		{
			return;
		}

		std::ifstream file(settingsFilePath_);
		if (!file.is_open()) return;

		nlohmann::json root;
		file >> root;
		file.close();

		auto parseItemArray = [](const nlohmann::json& arr, SceneTextContainer& dest) {
			dest.items.clear();
			dest.itemMap.clear();
			for (const auto& j : arr)
			{
				std::string id = j.value("id", "");
				if (id.empty()) continue;

				UITextItem item;
				item.id = id;
				item.text = j.value("text", "");
				item.position.x = j.value("posX", 0.0f);
				item.position.y = j.value("posY", 0.0f);
				item.fontSize = j.value("fontSize", 24.0f);
				item.fontType = static_cast<ImGuiManager::FontType>(j.value("fontType", 1));

				if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 4)
				{
					item.color = { j["color"][0], j["color"][1], j["color"][2], j["color"][3] };
				}

				item.hasOutline = j.value("hasOutline", true);
				if (j.contains("outlineColor") && j["outlineColor"].is_array() && j["outlineColor"].size() >= 4)
				{
					item.outlineColor = { j["outlineColor"][0], j["outlineColor"][1], j["outlineColor"][2], j["outlineColor"][3] };
				}
				item.outlineThickness = j.value("outlineThickness", 2.0f);

				item.hasShadow = j.value("hasShadow", false);
				if (j.contains("shadowColor") && j["shadowColor"].is_array() && j["shadowColor"].size() >= 4)
				{
					item.shadowColor = { j["shadowColor"][0], j["shadowColor"][1], j["shadowColor"][2], j["shadowColor"][3] };
				}
				if (j.contains("shadowOffset") && j["shadowOffset"].is_array() && j["shadowOffset"].size() >= 2)
				{
					item.shadowOffset = { j["shadowOffset"][0], j["shadowOffset"][1] };
				}

				item.hasUnderline = j.value("hasUnderline", (id == "GameOverTitle"));
				if (j.contains("underlineColor") && j["underlineColor"].is_array() && j["underlineColor"].size() >= 4)
				{
					item.underlineColor = { j["underlineColor"][0], j["underlineColor"][1], j["underlineColor"][2], j["underlineColor"][3] };
				}
				item.underlineWidth = j.value("underlineWidth", 320.0f);
				item.underlineThickness = j.value("underlineThickness", 2.5f);
				item.underlineOffsetY = j.value("underlineOffsetY", 42.0f);

				item.align = static_cast<UITextAlign>(j.value("align", 0));
				item.isVisible = j.value("isVisible", true);

				size_t idx = dest.items.size();
				dest.items.push_back(item);
				dest.itemMap[id] = idx;
			}
		};

		// 新形式: "scenes": { "TITLE": [...], "GAMEPLAY": [...] }
		if (root.contains("scenes") && root["scenes"].is_object())
		{
			for (auto& [sName, arr] : root["scenes"].items())
			{
				if (arr.is_array())
				{
					auto& sceneData = GetOrCreateScene(sName);
					parseItemArray(arr, sceneData);
				}
			}
		}
		// 旧形式下位互換: "items": [...] を "GAMEPLAY" に流し込み
		else if (root.contains("items") && root["items"].is_array())
		{
			auto& sceneData = GetOrCreateScene("GAMEPLAY");
			parseItemArray(root["items"], sceneData);
		}
	}
	catch (...)
	{
	}
}

#ifdef USE_IMGUI
void UITextManager::DrawImGuiEditor()
{
	ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "■ UIテキスト配置エディタ (シーン別)");
	ImGui::Separator();

	// 編集対象シーン選択
	ImGui::Text("編集対象シーン:");
	ImGui::SameLine();

	std::vector<const char*> sceneNamePtrs;
	int currentSceneIdx = -1;
	for (size_t i = 0; i < knownScenes_.size(); ++i)
	{
		sceneNamePtrs.push_back(knownScenes_[i].c_str());
		if (knownScenes_[i] == editingScene_)
		{
			currentSceneIdx = static_cast<int>(i);
		}
	}

	if (currentSceneIdx == -1)
	{
		knownScenes_.push_back(editingScene_);
		sceneNamePtrs.push_back(editingScene_.c_str());
		currentSceneIdx = static_cast<int>(knownScenes_.size() - 1);
	}

	if (ImGui::Combo("##SelectEditingScene", &currentSceneIdx, sceneNamePtrs.data(), static_cast<int>(sceneNamePtrs.size())))
	{
		SetEditingScene(knownScenes_[currentSceneIdx]);
	}

	ImGui::SameLine();
	ImGui::Checkbox("実行中シーンに自動追従", &followActiveScene_);

	ImGui::Spacing();

	// 保存 & 読み込みボタン
	if (ImGui::Button("設定を保存 (Save JSON)", ImVec2(160, 26)))
	{
		SaveSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("設定を再読込 (Reload)", ImVec2(140, 26)))
	{
		LoadSettings();
	}
	ImGui::SameLine();
	ImGui::Checkbox("選択枠を表示", &showGizmo_);
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "(配置変更は自動保存されます)");

	ImGui::Spacing();

	// アイテム新規追加
	static char newIdBuffer[64] = "";
	ImGui::InputTextWithHint("##NewTextID", "新規登録ID (例: StageName)", newIdBuffer, sizeof(newIdBuffer));
	ImGui::SameLine();
	if (ImGui::Button("追加 (Add)") && strlen(newIdBuffer) > 0)
	{
		std::string newId = newIdBuffer;
		auto& scene = GetOrCreateScene(editingScene_);
		if (scene.itemMap.find(newId) == scene.itemMap.end())
		{
			UITextItem newItem;
			newItem.id = newId;
			newItem.text = newId;
			newItem.position = { 640.0f, 360.0f };
			RegisterText(newItem, editingScene_);
			newIdBuffer[0] = '\0';
			selectedItemIndex_ = static_cast<int>(scene.items.size() - 1);
			SaveSettings();
		}
	}

	ImGui::Separator();

	auto& scene = GetOrCreateScene(editingScene_);
	if (scene.items.empty())
	{
		ImGui::TextDisabled("シーン「%s」に登録されているUIテキストがありません。", editingScene_.c_str());
		return;
	}

	if (selectedItemIndex_ >= static_cast<int>(scene.items.size()))
	{
		selectedItemIndex_ = 0;
	}

	// リスト表示とプロパティ
	ImGui::Columns(2, "UITextColumns", true);
	static bool setColWidth = true;
	if (setColWidth)
	{
		ImGui::SetColumnWidth(0, 160.0f);
		setColWidth = false;
	}

	// 左カラム: アイテムリスト
	ImGui::BeginChild("TextItemList", ImVec2(0, 280), true);
	for (int i = 0; i < static_cast<int>(scene.items.size()); ++i)
	{
		std::string label = scene.items[i].id;
		if (!scene.items[i].isVisible)
		{
			label += " (非表示)";
		}
		if (ImGui::Selectable(label.c_str(), selectedItemIndex_ == i))
		{
			selectedItemIndex_ = i;
		}
	}
	ImGui::EndChild();

	ImGui::NextColumn();

	// 右カラム: 選択中アイテムの編集
	if (selectedItemIndex_ >= 0 && selectedItemIndex_ < static_cast<int>(scene.items.size()))
	{
		auto& item = scene.items[selectedItemIndex_];

		ImGui::BeginChild("TextItemProps", ImVec2(0, 280), false);
		ImGui::Text("ID: %s (シーン: %s)", item.id.c_str(), editingScene_.c_str());
		ImGui::SameLine();
		if (ImGui::Button("削除 (Delete)") && scene.items.size() > 1)
		{
			scene.items.erase(scene.items.begin() + selectedItemIndex_);
			scene.itemMap.clear();
			for (size_t k = 0; k < scene.items.size(); ++k)
			{
				scene.itemMap[scene.items[k].id] = k;
			}
			selectedItemIndex_ = 0;
			SaveSettings();
			ImGui::EndChild();
			ImGui::Columns(1);
			return;
		}

		bool isChanged = false;
		if (ImGui::Checkbox("表示 (Visible)", &item.isVisible)) isChanged = true;

		// 表示文字列
		char textBuf[256];
		strncpy_s(textBuf, item.text.c_str(), sizeof(textBuf));
		if (ImGui::InputText("表示文字列", textBuf, sizeof(textBuf)))
		{
			item.text = textBuf;
			isChanged = true;
		}

		// 座標ドラッグ (1280x720 基準)
		if (ImGui::DragFloat2("座標 (X, Y)", &item.position.x, 1.0f, 0.0f, 1280.0f, "%.1f")) isChanged = true;

		// 位置プリセットボタン
		ImGui::TextDisabled("プリセット位置:");
		ImGui::SameLine();
		if (ImGui::SmallButton("左上")) { item.position = { 40.0f, 30.0f }; item.align = UITextAlign::Left; isChanged = true; }
		ImGui::SameLine();
		if (ImGui::SmallButton("右上")) { item.position = { 1240.0f, 30.0f }; item.align = UITextAlign::Right; isChanged = true; }
		ImGui::SameLine();
		if (ImGui::SmallButton("中央")) { item.position = { 640.0f, 360.0f }; item.align = UITextAlign::Center; isChanged = true; }
		ImGui::SameLine();
		if (ImGui::SmallButton("下中央")) { item.position = { 640.0f, 650.0f }; item.align = UITextAlign::Center; isChanged = true; }

		// 揃え
		int alignInt = static_cast<int>(item.align);
		const char* alignNames[] = { "左揃え (Left)", "中央揃え (Center)", "右揃え (Right)" };
		if (ImGui::Combo("テキスト揃え", &alignInt, alignNames, IM_ARRAYSIZE(alignNames)))
		{
			item.align = static_cast<UITextAlign>(alignInt);
			isChanged = true;
		}

		// フォント選択
		int fontInt = static_cast<int>(item.fontType);
		const char* fontNames[] = {
			"デフォルト (YuGoth 14px)",
			"日本語 MPLUS (24px)",
			"日本語 MPLUS 大 (48px)",
			"等幅英数 FiraMono (24px)",
			"等幅英数 FiraMono 大 (48px)"
		};
		if (ImGui::Combo("フォント", &fontInt, fontNames, IM_ARRAYSIZE(fontNames)))
		{
			item.fontType = static_cast<ImGuiManager::FontType>(fontInt);
			isChanged = true;
		}

		// サイズ
		if (ImGui::SliderFloat("フォントサイズ (px)", &item.fontSize, 10.0f, 100.0f, "%.1f")) isChanged = true;

		// 文字色
		if (ImGui::ColorEdit4("文字色 (Color)", &item.color.x)) isChanged = true;

		// 縁取り (アウトライン)
		if (ImGui::CollapsingHeader("縁取り設定 (Outline)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Checkbox("縁取りを有効化", &item.hasOutline)) isChanged = true;
			if (item.hasOutline)
			{
				if (ImGui::SliderFloat("縁取り太さ", &item.outlineThickness, 0.5f, 5.0f, "%.1f")) isChanged = true;
				if (ImGui::ColorEdit4("縁取り色", &item.outlineColor.x)) isChanged = true;
			}
		}

		// ドロップシャドウ
		if (ImGui::CollapsingHeader("影設定 (Shadow)"))
		{
			if (ImGui::Checkbox("影を有効化", &item.hasShadow)) isChanged = true;
			if (item.hasShadow)
			{
				if (ImGui::DragFloat2("影オフセット", &item.shadowOffset.x, 0.5f, -20.0f, 20.0f, "%.1f")) isChanged = true;
				if (ImGui::ColorEdit4("影の色", &item.shadowColor.x)) isChanged = true;
			}
		}

		// 下線 (アンダーバー)
		if (ImGui::CollapsingHeader("下線設定 (Underline)", item.hasUnderline ? ImGuiTreeNodeFlags_DefaultOpen : 0))
		{
			if (ImGui::Checkbox("下線を有効化", &item.hasUnderline)) isChanged = true;
			if (item.hasUnderline)
			{
				if (ImGui::SliderFloat("下線の長さ (幅)", &item.underlineWidth, 10.0f, 1280.0f, "%.1f")) isChanged = true;
				if (ImGui::SliderFloat("下線の太さ", &item.underlineThickness, 0.5f, 20.0f, "%.1f")) isChanged = true;
				if (ImGui::DragFloat("Yオフセット (上下位置)", &item.underlineOffsetY, 0.5f, -100.0f, 200.0f, "%.1f")) isChanged = true;
				if (ImGui::ColorEdit4("下線の色", &item.underlineColor.x)) isChanged = true;
			}
			}

		if (isChanged)
		{
			SaveSettings();
		}

		ImGui::EndChild();
	}

	ImGui::Columns(1);
}

void UITextManager::Draw(ImDrawList* drawList, const ImVec2& screenOffset, const ImVec2& screenSize)
{
	if (!drawList || screenSize.x <= 0.0f || screenSize.y <= 0.0f) return;
	if (hudAlpha_ <= 0.001f) return;

	// トランジション（画面遷移）中のアルファ制御
	float transitionAlpha = 1.0f;
	if (auto sceneManager = EngineServices::GetInstance()->GetSceneManager())
	{
		if (sceneManager->IsTransitioning())
		{
			auto state = sceneManager->GetTransitionState();
			float prog = sceneManager->GetTransitionProgress();
			switch (state)
			{
			case SceneManager::TransitionState::FadeOut:
				// 画面が閉じるにつれてフェードアウト (prog: 0.0 -> 1.0)
				transitionAlpha = (std::max)(0.0f, 1.0f - prog);
				break;
			case SceneManager::TransitionState::FadeOutHold:
			case SceneManager::TransitionState::Loading:
			case SceneManager::TransitionState::FadeInWait:
				// 暗転中・ロード中は完全に非表示
				transitionAlpha = 0.0f;
				break;
			case SceneManager::TransitionState::FadeIn:
				// 画面が開くにつれてフェードイン (prog: 0.0 -> 1.0)
				transitionAlpha = (std::min)(1.0f, prog);
				break;
			default:
				transitionAlpha = 1.0f;
				break;
			}
		}
	}

	// 画面遷移の完全暗転中はUIテキストを描画せず、ビューポートを真っ黒にしてテキストを完全に「画面遷移の後ろ」に隠す
	if (transitionAlpha <= 0.001f)
	{
		drawList->AddRectFilled(
			screenOffset,
			ImVec2(screenOffset.x + screenSize.x, screenOffset.y + screenSize.y),
			IM_COL32(0, 0, 0, 255)
		);
		return;
	}

	auto imguiManager = EngineServices::GetInstance()->GetImGuiManager();

	// 1280x720 基準のスケール比率
	float scaleX = screenSize.x / 1280.0f;
	float scaleY = screenSize.y / 720.0f;
	float uniformScale = (scaleX < scaleY) ? scaleX : scaleY;

	// 描画対象シーン（通常は現在アクティブな currentScene_）
	std::string targetSceneName = currentScene_;
	auto sIt = scenes_.find(targetSceneName);
	if (sIt == scenes_.end() || sIt->second.items.empty()) return;

	auto& sceneItems = sIt->second.items;
	bool isEditingCurrentScene = (editingScene_ == currentScene_);

	float effectiveAlpha = transitionAlpha * hudAlpha_;

	for (size_t i = 0; i < sceneItems.size(); ++i)
	{
		auto& item = sceneItems[i];
		if (!item.isVisible || item.text.empty()) continue;

		ImFont* font = imguiManager ? imguiManager->GetFont(item.fontType) : ImGui::GetFont();
		if (!font) font = ImGui::GetFont();

		float scaledFontSize = item.fontSize * uniformScale;
		if (scaledFontSize < 6.0f) scaledFontSize = 6.0f;

		// スクリーン座標計算 (1280x720 空間からの変換)
		ImVec2 textPos(screenOffset.x + item.position.x * scaleX, screenOffset.y + item.position.y * scaleY);

		// テキストサイズの計測
		ImVec2 textSize = font->CalcTextSizeA(scaledFontSize, FLT_MAX, -1.0f, item.text.c_str());

		// アライメント調整
		if (item.align == UITextAlign::Center)
		{
			textPos.x -= textSize.x * 0.5f;
		}
		else if (item.align == UITextAlign::Right)
		{
			textPos.x -= textSize.x;
		}

		// 1. 影の描画
		if (item.hasShadow)
		{
			ImU32 shadowCol = ImColor(item.shadowColor.x, item.shadowColor.y, item.shadowColor.z, item.shadowColor.w * effectiveAlpha);
			ImVec2 shadowPos(textPos.x + item.shadowOffset.x * uniformScale, textPos.y + item.shadowOffset.y * uniformScale);
			drawList->AddText(font, scaledFontSize, shadowPos, shadowCol, item.text.c_str());
		}

		// 2. 縁取り (アウトライン) の描画
		if (item.hasOutline)
		{
			ImU32 outlineCol = ImColor(item.outlineColor.x, item.outlineColor.y, item.outlineColor.z, item.outlineColor.w * effectiveAlpha);
			float thick = item.outlineThickness * uniformScale;
			const float offsets[8][2] = {
				{ -thick, -thick }, {  0.0f, -thick }, {  thick, -thick },
				{ -thick,  0.0f },                     {  thick,  0.0f },
				{ -thick,  thick }, {  0.0f,  thick }, {  thick,  thick }
			};
			for (int k = 0; k < 8; ++k)
			{
				drawList->AddText(font, scaledFontSize, ImVec2(textPos.x + offsets[k][0], textPos.y + offsets[k][1]), outlineCol, item.text.c_str());
			}
		}

		// 3. 本体のテキスト描画
		ImU32 textCol = ImColor(item.color.x, item.color.y, item.color.z, item.color.w * effectiveAlpha);
		drawList->AddText(font, scaledFontSize, textPos, textCol, item.text.c_str());

		// 3.5. 下線 (アンダーバー) の描画
		if (item.hasUnderline)
		{
			ImU32 lineCol = ImColor(item.underlineColor.x, item.underlineColor.y, item.underlineColor.z, item.underlineColor.w * effectiveAlpha);
			float lineThickness = item.underlineThickness * uniformScale;
			float halfW = (item.underlineWidth > 0.0f ? item.underlineWidth : textSize.x) * 0.5f * uniformScale;
			float lineCenterY = textPos.y + item.underlineOffsetY * uniformScale;
			float centerX = textPos.x + textSize.x * 0.5f;

			drawList->AddLine(
				ImVec2(centerX - halfW, lineCenterY),
				ImVec2(centerX + halfW, lineCenterY),
				lineCol,
				lineThickness
			);
		}

		// 4. エディタモード中かつ選択中の項目の場合のみ、ギズモ（枠線）を表示 & マウス直接ドラッグ（通常プレイ中・リリース時は絶対に非表示）
#ifdef ENABLE_EDITOR
		bool isEditorActive = false;
		if (auto services = EngineServices::GetInstance())
		{
			isEditorActive = services->GetEditorMode();
		}

		if (isEditorActive && transitionAlpha >= 0.99f && isEditingCurrentScene && showGizmo_ && static_cast<int>(i) == selectedItemIndex_)
		{
			ImVec2 boxMin(textPos.x - 4.0f, textPos.y - 2.0f);
			ImVec2 boxMax(textPos.x + textSize.x + 4.0f, textPos.y + textSize.y + 2.0f);
			drawList->AddRect(boxMin, boxMax, IM_COL32(0, 200, 255, 200), 2.0f, 0, 1.5f);
			drawList->AddText(ImVec2(boxMin.x, boxMin.y - 14.0f), IM_COL32(0, 200, 255, 220), item.id.c_str());

			// ゲーム画面上での直接マウスドラッグ操作
			ImGuiIO& io = ImGui::GetIO();
			ImVec2 mousePos = io.MousePos;
			bool isHovered = (mousePos.x >= boxMin.x && mousePos.x <= boxMax.x && mousePos.y >= boxMin.y && mousePos.y <= boxMax.y);

			static bool isDragging = false;
			if (isHovered && ImGui::IsMouseClicked(0))
			{
				isDragging = true;
			}
			if (isDragging)
			{
				if (ImGui::IsMouseDown(0))
				{
					if (scaleX > 0.0001f && scaleY > 0.0001f)
					{
						item.position.x += io.MouseDelta.x / scaleX;
						item.position.y += io.MouseDelta.y / scaleY;
					}
				}
				else
				{
					isDragging = false;
					SaveSettings(); // ドラッグ完了時に自動保存
				}
			}
		}
#endif
	}

	// 画面遷移（トランジション）の黒カーテンをテキストの手前に被せることで、テキストを完全に「画面遷移の後ろ」に配置
	if (transitionAlpha < 0.999f)
	{
		float blackAlpha = std::clamp(1.0f - transitionAlpha, 0.0f, 1.0f);
		drawList->AddRectFilled(
			screenOffset,
			ImVec2(screenOffset.x + screenSize.x, screenOffset.y + screenSize.y),
			IM_COL32(0, 0, 0, static_cast<int>(255.0f * blackAlpha))
		);
	}
}
#endif
