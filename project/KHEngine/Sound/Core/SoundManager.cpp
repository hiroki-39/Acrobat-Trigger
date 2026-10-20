#include "SoundManager.h"
#include "KHEngine/Core/Utility/String/StringUtility.h"
#include "KHEngine/Core/Resource/ResourceLocator.h"
#include "KHEngine/Core/Services/EngineServices.h"
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include "externals/imgui/imgui.h"
#include <algorithm>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")


// シングルトンインスタンスの取得
SoundManager* SoundManager::GetInstance()
{
	static SoundManager instance;
	return &instance;
}

// 初期化
void SoundManager::Initialize()
{
	HRESULT result;

	// XAudio2の初期化
	result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	// マスターボイスの生成
	result = xAudio2.Get()->CreateMasteringVoice(&masteringVoice);
	assert(SUCCEEDED(result));

	// Media Foundationの初期化
	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));
}

// 終了処理
void SoundManager::Finalize()
{
	// 多重呼び出し防止
	if (!xAudio2) return;

	StopBGM();
	StopAllSE();
	soundCache_.clear();

	HRESULT result;

	// Media Foundationの終了処理
	result = MFShutdown();
	assert(SUCCEEDED(result));

	// マスターボイスの破棄（xAudio2が有効な場合のみ）
	if (masteringVoice && xAudio2)
	{
		masteringVoice->DestroyVoice();
		masteringVoice = nullptr;
	}

	// XAudio2内部スレッドの処理完了を待つ
	if (xAudio2) {
		xAudio2->CommitChanges(0); // 0は全ての変更を即時反映
		xAudio2->StopEngine();     // エンジン停止で内部スレッド終了を待つ
	}

	// XAudio2の解放
	xAudio2.Reset();
}

SoundManager::SoundData SoundManager::SoundLoadWave(const char* filename)
{
	HRESULT result = {};

	/*---　1. ファイルを開く ---*/
	//ファイル入力ストリームのインスタンス
	std::ifstream file;

	//.wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);

	//とりあえず開かなかったら止める
	assert(file.is_open());

	/*---　2. .wavデータ読み込み ---*/
	//RIFFヘッダーの読み込み
	RiffHeader riff;

	//チャンクヘッダーの確認
	file.read((char*)&riff, sizeof(riff));

	//ファイルがRIFFかチェックする
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0)
	{
		assert(0);
	}

	//ファイルがWAVEかチェックする
	if (strncmp(riff.type, "WAVE", 4) != 0)
	{
		assert(0);
	}

	//Formatチャンクの読み込み
	FormatChunk format = {};

	//チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));

	//ファイルがfmtかチェックする
	if (strncmp(format.chunk.id, "fmt ", 4) != 0)
	{
		assert(0);
	}

	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);

	//Dataチャンクの読み込み
	ChunkHeader data;

	//チャンクヘッダーの確認
	file.read((char*)&data, sizeof(data));

	//JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0)
	{
		//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);

		//再読み込み
		file.read((char*)&data, sizeof(data));
	}

	if (strncmp(data.id, "data", 4) != 0)
	{
		assert(0);
	}

	//Dataチャンクのデータ部(波形データ)の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	/*---　3. ファイルを閉じる ---*/
	//Waveファイルを閉じる
	file.close();

	/*--- 4. 読み込んだ音声データをreturnする ---*/
	//returnするための音声データ
	SoundData soundData = {};

	//波形フォーマット
	soundData.wfex = format.fmt;
	////波形データ
	//soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	////波形データのサイズ
	//soundData.buffersize = data.size;

	return soundData;
}

SoundManager::SoundData SoundManager::SoundLoadFile(const std::string& filename)
{
	SoundData soundData = {};

	// 論理名を実パスに解決（例: "bgm.mp3" -> "resources/audio/bgm.mp3"）
	std::string resolved = ResourceLocator::Resolve(filename, ResourceLocator::AssetType::Audio);
	if (resolved.empty())
	{
		// 解決できなければ元の名前を使う（互換性）
		resolved = filename;
	}

	// フルパスをワイド文字列に変換
	std::wstring wfilename = StringUtility::ConvertString(resolved);
	HRESULT result;

	// SourceReaderの作成
	Microsoft::WRL::ComPtr<IMFSourceReader> pReader;
	result = MFCreateSourceReaderFromURL(wfilename.c_str(), nullptr, &pReader);
	if (FAILED(result) || !pReader)
	{
		char msg[256];
		snprintf(msg, sizeof(msg), "[SoundManager] Failed to load audio file: %s (HRESULT: 0x%08X)\n", filename.c_str(), static_cast<unsigned int>(result));
		OutputDebugStringA(msg);
		return soundData;
	}

	// PCM形式にフォーマット指定する
	Microsoft::WRL::ComPtr<IMFMediaType> pPCMType;
	MFCreateMediaType(&pPCMType);
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	result = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCMType.Get());
	assert(SUCCEEDED(result));

	// 実際にセットされたメディアタイプを取得する
	Microsoft::WRL::ComPtr<IMFMediaType> pCurrentType;
	result = pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pCurrentType);

	// Waveフォーマットを取得する
	WAVEFORMATEX* waveFormat = nullptr;
	MFCreateWaveFormatExFromMFMediaType(pCurrentType.Get(), &waveFormat, nullptr);

	// コンテナに格納する音声データ
	if (waveFormat)
	{
		soundData.wfex = *waveFormat;
		CoTaskMemFree(waveFormat);
	}

	// PCMデータのバッファを構築
	while (true)
	{
		Microsoft::WRL::ComPtr<IMFSample> pSample;
		DWORD streamIndex = 0;
		DWORD flags = 0;
		LONGLONG llTimestamp = 0;

		// サンプルの読み込み
		result = pReader->ReadSample(
			MF_SOURCE_READER_FIRST_AUDIO_STREAM,
			0,
			&streamIndex,
			&flags,
			&llTimestamp,
			&pSample);

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			break;
		}

		if (pSample)
		{
			Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
			// サンプルに含まれるサウンドデータのバッファを一繋ぎにして取得
			pSample->ConvertToContiguousBuffer(&pBuffer);

			// データ読み取り用ポインタ
			BYTE* pData = nullptr;
			DWORD maxLength = 0;
			DWORD currentLength = 0;
			// バッファ読み込み用にロック
			pBuffer->Lock(&pData, &maxLength, &currentLength);

			// バッファの末尾にデータを追加
			soundData.buffer.insert(soundData.buffer.end(), pData, pData + currentLength);
			pBuffer->Unlock();
		}
	}

	return soundData;
}

void SoundManager::SoundUnload(SoundData* soundData)
{
	soundData->buffer.clear();
	soundData->wfex = {};
}

IXAudio2* SoundManager::GetXAudio2() const
{
	return xAudio2.Get();
}

const SoundManager::SoundData& SoundManager::GetOrLoadSound(const std::string& filename)
{
	auto it = soundCache_.find(filename);
	if (it != soundCache_.end())
	{
		return it->second;
	}

	SoundData data = SoundLoadFile(filename);
	auto inserted = soundCache_.emplace(filename, std::move(data));
	return inserted.first->second;
}

void SoundManager::PlayBGM(const std::string& filename, float volume, bool loop)
{
	bgmVolume_ = volume;

	// 保留情報を更新（どのシーンでも呼ばれた最新のBGMを記録）
	pendingBgm_.filename = filename;
	pendingBgm_.volume = volume;
	pendingBgm_.loop = loop;
	pendingBgm_.hasPending = true;

#ifdef ENABLE_EDITOR
	// Develop構成時、ゲーム再生ボタンが押されるまでBGMは鳴らさない
	if (!EngineServices::GetInstance()->IsGamePlaying())
	{
		// すでに再生中のBGMがあれば停止しておく（保留情報は保持）
		if (bgmVoice_)
		{
			StopBGM(false);
		}
		isBgmPaused_ = false;
		return;
	}
#endif

	if (currentBgmName_ == filename && bgmVoice_ != nullptr)
	{
		bgmVolume_ = volume;
		bgmVoice_->SetVolume(isBgmMuted_ ? 0.0f : bgmVolume_);
		if (isBgmPaused_)
		{
			bgmVoice_->Start();
			isBgmPaused_ = false;
		}
		return;
	}

	StopBGM(false);

	if (filename.empty() || !xAudio2) return;

	const SoundData& sound = GetOrLoadSound(filename);
	if (sound.buffer.empty()) return;

	HRESULT hr = xAudio2->CreateSourceVoice(&bgmVoice_, &sound.wfex);
	if (FAILED(hr) || !bgmVoice_) return;

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = sound.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(sound.buffer.size());
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	bgmVolume_ = volume;
	bgmVoice_->SetVolume(isBgmMuted_ ? 0.0f : bgmVolume_);
	bgmVoice_->SubmitSourceBuffer(&buf);
	bgmVoice_->Start();

	currentBgmName_ = filename;
	isBgmPaused_ = false;
}

void SoundManager::StopBGM(bool clearPending)
{
	if (bgmVoice_)
	{
		bgmVoice_->Stop();
		bgmVoice_->FlushSourceBuffers();
		bgmVoice_->DestroyVoice();
		bgmVoice_ = nullptr;
	}
	currentBgmName_.clear();
	isBgmPaused_ = false;
	if (clearPending)
	{
		pendingBgm_.hasPending = false;
	}
}

void SoundManager::OnGamePlayStateChanged(bool isPlaying)
{
#ifdef ENABLE_EDITOR
	if (isPlaying)
	{
		// ゲーム再生開始時: 一時停止中だった場合は再開、そうでなく保留BGMがあれば新規再生
		if (isBgmPaused_ && bgmVoice_)
		{
			bgmVoice_->Start();
			isBgmPaused_ = false;
		}
		else if (pendingBgm_.hasPending && !pendingBgm_.filename.empty())
		{
			PlayBGM(pendingBgm_.filename, pendingBgm_.volume, pendingBgm_.loop);
		}
	}
	else
	{
		// ゲーム一時停止・停止時: 再生中のBGMを一時停止し、SEをすべて停止
		if (bgmVoice_ && !isBgmPaused_)
		{
			bgmVoice_->Stop();
			isBgmPaused_ = true;
		}
		StopAllSE();
	}
#endif
}

void SoundManager::SetBGMVolume(float volume)
{
	bgmVolume_ = std::clamp(volume, 0.0f, 1.0f);
	if (bgmVoice_)
	{
		bgmVoice_->SetVolume(isBgmMuted_ ? 0.0f : bgmVolume_);
	}
}

bool SoundManager::IsBGMPlaying() const
{
	return bgmVoice_ != nullptr;
}

void SoundManager::SetMasterMute(bool mute)
{
	isMasterMuted_ = mute;
	if (masteringVoice)
	{
		masteringVoice->SetVolume(isMasterMuted_ ? 0.0f : masterVolume_);
	}
}

void SoundManager::SetMasterVolume(float volume)
{
	masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
	if (masteringVoice && !isMasterMuted_)
	{
		masteringVoice->SetVolume(masterVolume_);
	}
}

void SoundManager::SetBGMMute(bool mute)
{
	isBgmMuted_ = mute;
	if (bgmVoice_)
	{
		bgmVoice_->SetVolume(isBgmMuted_ ? 0.0f : bgmVolume_);
	}
}

void SoundManager::SetSEMute(bool mute)
{
	isSeMuted_ = mute;
	if (isSeMuted_)
	{
		StopAllSE();
	}
}

void SoundManager::SetSEVolume(float volume)
{
	seVolume_ = std::clamp(volume, 0.0f, 1.0f);
}

void SoundManager::PlaySE(const std::string& filename, float volume)
{
#ifdef ENABLE_EDITOR
	// Develop構成時、ゲーム再生ボタンが押されるまでSEは鳴らさない
	if (!EngineServices::GetInstance()->IsGamePlaying())
	{
		return;
	}
#endif

	// SEミュートまたはマスターミュート時は再生スキップ
	if (isSeMuted_ || isMasterMuted_) return;

	if (filename.empty() || !xAudio2) return;

	// 再生完了済みボイスの回収
	Update();

	const SoundData& sound = GetOrLoadSound(filename);
	if (sound.buffer.empty()) return;

	IXAudio2SourceVoice* voice = nullptr;
	HRESULT hr = xAudio2->CreateSourceVoice(&voice, &sound.wfex);
	if (FAILED(hr) || !voice) return;

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = sound.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(sound.buffer.size());
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = 0;

	voice->SetVolume(volume * seVolume_);
	voice->SubmitSourceBuffer(&buf);
	voice->Start();

	activeSEVoices_.push_back(voice);
}

void SoundManager::StopAllSE()
{
	for (auto voice : activeSEVoices_)
	{
		if (voice)
		{
			voice->Stop();
			voice->FlushSourceBuffers();
			voice->DestroyVoice();
		}
	}
	activeSEVoices_.clear();
}

void SoundManager::Update()
{
	// 再生完了したSEボイスを安全に破棄
	for (auto it = activeSEVoices_.begin(); it != activeSEVoices_.end();)
	{
		IXAudio2SourceVoice* voice = *it;
		if (!voice)
		{
			it = activeSEVoices_.erase(it);
			continue;
		}

		XAUDIO2_VOICE_STATE state{};
		voice->GetState(&state);
		if (state.BuffersQueued == 0)
		{
			voice->Stop();
			voice->FlushSourceBuffers();
			voice->DestroyVoice();
			it = activeSEVoices_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void SoundManager::DrawImGuiAudioControls()
{
	ImGui::Spacing();
	ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "AUDIO / SOUND");
	ImGui::Separator();

	// 1. 全ミュートボタン
	if (isMasterMuted_)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
		if (ImGui::Button("[全ミュート解除] (Muted)##MasterMute", ImVec2(-1, 30)))
		{
			ToggleMasterMute();
		}
		ImGui::PopStyleColor(2);
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.60f, 0.35f, 1.0f));
		if (ImGui::Button("[全ミュート] (Mute All)##MasterMute", ImVec2(-1, 30)))
		{
			ToggleMasterMute();
		}
		ImGui::PopStyleColor(2);
	}

	ImGui::Spacing();

	// 2. BGM / SE 個別ミュートボタン
	float availWidth = ImGui::GetContentRegionAvail().x;
	float btnWidth = (availWidth - 8.0f) * 0.5f;

	if (isBgmMuted_)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
		if (ImGui::Button("BGM: 消音中##BgmMuteBtn", ImVec2(btnWidth, 26)))
		{
			ToggleBGMMute();
		}
		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.65f, 1.0f));
		if (ImGui::Button("BGM: 再生中##BgmMuteBtn", ImVec2(btnWidth, 26)))
		{
			ToggleBGMMute();
		}
		ImGui::PopStyleColor();
	}

	ImGui::SameLine();

	if (isSeMuted_)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
		if (ImGui::Button("SE: 消音中##SeMuteBtn", ImVec2(btnWidth, 26)))
		{
			ToggleSEMute();
		}
		ImGui::PopStyleColor();
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.45f, 1.0f));
		if (ImGui::Button("SE: 再生中##SeMuteBtn", ImVec2(btnWidth, 26)))
		{
			ToggleSEMute();
		}
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();

	// 3. 音量スライダー
	float masterVol = masterVolume_;
	if (ImGui::SliderFloat("Master##MasterVol", &masterVol, 0.0f, 1.0f, "%.2f"))
	{
		SetMasterVolume(masterVol);
	}

	float bgmVol = bgmVolume_;
	if (ImGui::SliderFloat("BGM##BgmVol", &bgmVol, 0.0f, 1.0f, "%.2f"))
	{
		SetBGMVolume(bgmVol);
	}

	float seVol = seVolume_;
	if (ImGui::SliderFloat("SE##SeVol", &seVol, 0.0f, 1.0f, "%.2f"))
	{
		SetSEVolume(seVol);
	}
}