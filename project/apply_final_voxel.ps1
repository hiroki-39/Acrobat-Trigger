$p = "Game/Scene/TitleScene.cpp"
$c = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)

# 1. Initialize
$oldInit = @"
    // GPUボクセルパーティクルシステムの初期化
    gpuParticleSystem_ = std::make_unique<GPUParticleSystem>();
    gpuParticleSystem_->Initialize(dxCommon, srvManager);
"@

$newInit = @"
    // GPUボクセルパーティクルシステムおよび描画用バッファの初期化
    gpuParticleSystem_ = std::make_unique<GPUParticleSystem>();
    gpuParticleSystem_->Initialize(dxCommon, srvManager);

    // 640個のボクセルインスタンス用バッファ（UPLOAD Heap: 640 × 64B = 40KB）
    voxelBufferResource_ = dxCommon->CreateBufferResource(sizeof(GPUParticleData) * kMaxVoxelCubes);
    voxelBufferResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVoxelBuffer_));
    std::memset(mappedVoxelBuffer_, 0, sizeof(GPUParticleData) * kMaxVoxelCubes);

    voxelSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforStructuredBuffer(voxelSrvIndex_, voxelBufferResource_.Get(), static_cast<UINT>(kMaxVoxelCubes), sizeof(GPUParticleData));

    voxelCubes_.resize(kMaxVoxelCubes);
"@

# 2. Insert SpawnShipVoxelCubes & UpdateShipVoxelCubes before TriggerStartTransition
$simCode = [System.IO.File]::ReadAllText("voxel_sim_code.txt", [System.Text.Encoding]::UTF8).Trim().Replace("`r`n", "`n")

$oldTriggerHead = "void TitleScene::TriggerStartTransition(bool isPreview)"
$newTriggerHead = $simCode + "`n`n" + $oldTriggerHead

# 3. Inside TriggerStartTransition
$oldTriggerBody = @"
    if (startTransitionType_ == StartTransitionType::VirtualShatter)
    {
        diveDuration_ = isPreview ? 1.35f : 1.15f;
        if (gpuParticleSystem_)
        {
            gpuParticleSystem_->Reset();
            GPUEmitterData emitter;
            emitter.translate = diveStartPos_;
            emitter.radius = 1.0f;
            emitter.baseVelocity = { 0.0f, 8.5f, 0.0f };
            emitter.speed = 9.0f;
            emitter.startColor = { 0.20f, 1.0f, 0.95f, 1.0f };
            emitter.endColor = { 0.08f, 0.95f, 0.75f, 0.0f };
            emitter.startScale = 0.16f;
            emitter.endScale = 0.0f;
            emitter.lifeTime = 0.85f;
            emitter.spread = 1.2f;
            emitter.gravity = -1.5f;
            emitter.count = 384;
            emitter.emit = 1;
            emitter.patternType = 3;
            gpuParticleSystem_->Emit(emitter);
        }
    }
"@

$newTriggerBody = @"
    if (startTransitionType_ == StartTransitionType::VirtualShatter)
    {
        diveDuration_ = isPreview ? 1.35f : 1.15f;
        SpawnShipVoxelCubes(diveStartPos_);
    }
"@

# 4. Preview reset
$oldPreviewReset = @"
                if (gpuParticleSystem_)
                {
                    gpuParticleSystem_->Reset();
                }
"@

$newPreviewReset = @"
                if (mappedVoxelBuffer_)
                {
                    for (size_t i = 0; i < kMaxVoxelCubes; ++i)
                    {
                        mappedVoxelBuffer_[i].scale = { 0.0f, 0.0f, 0.0f };
                        mappedVoxelBuffer_[i].color = { 0.0f, 0.0f, 0.0f, 0.0f };
                    }
                }
"@

# 5. VirtualShatter
$oldShatter = [System.IO.File]::ReadAllText("shatter_new.txt", [System.Text.Encoding]::UTF8).Trim().Replace("`r`n", "`n")

$newShatter = @"
        if (startTransitionType_ == StartTransitionType::VirtualShatter)
        {
            // =========================================================
            // 【仮想空間自機ボクセル昇華シーケンス】
            //  ・決定時の急加速（スピードアップ）を排除し、巡航速度のまま滑らかに等速前進
            //  ・0.00s〜0.48s で自機がボクセルディゾルブ消滅
            //  ・前回の美しいボクセル演出（機体輪郭からの湧き出し・上昇・ゆらぎ）を
            //    GPUインスタンシング（DrawCall 1回）で超高速描画
            // =========================================================

            // 決定時の急加速を排除し、巡航速度のまま等速前進
            flightProgressZ_ += forwardSpeed_ * dt;
            float deltaZ = flightProgressZ_ - diveStartProgressZ_;

            curPos.x = diveStartPos_.x;
            curPos.y = diveStartPos_.y;
            curPos.z = diveStartPos_.z + deltaZ;
            curRot = diveStartRot_;

            // 0.00s〜0.48s で自機がボクセルディゾルブ消滅（0.48s時点で完全に透明）
            dissolveProgress_ = std::clamp(diveTimer_ / 0.48f, 0.0f, 1.0f);

            // 自機のマテリアルアルファ値を減衰
            playerColor_.w = 1.0f - dissolveProgress_;
            if (playerObj_)
            {
                playerObj_->SetColor(playerColor_);
            }

            // 演出の画面ノイズ（ポストプロセスのグリッチ・ノイズ）を無効化（クリアな視界を維持）
            if (auto pp = services->GetPostProcess())
            {
                pp->SetEffectActive("Random(Glitch)", false);
            }

            dodgeThrustBoost_ = 0.0f;

            // カメラ制御: 巡航速度のまま等速前進追従
            if (!isDebugCamera_ || !debugCamera_)
            {
                cameraPos_.x = diveStartCamPos_.x;
                cameraPos_.y = diveStartCamPos_.y;
                cameraPos_.z = diveStartCamPos_.z + deltaZ;
                cameraRot_ = diveStartCamRot_;
            }

            // 前回の美しいボクセル演出シミュレーション（GPUバッファ書き込み）
            UpdateShipVoxelCubes(dt, curPos);
        }
"@

# 6. Draw
$oldDraw = @"
    if (gpuParticleSystem_ && activeCamera)
    {
        auto texManager = TextureManager::GetInstance();
        uint32_t whiteTexIndex = texManager ? texManager->GetSrvIndex("white.png") : 0;
        gpuParticleSystem_->DrawCube(activeCamera->GetViewProjectionMatrix(), whiteTexIndex, GPUParticleBlendMode::Alpha);
    }
"@

$newDraw = @"
    if (gpuParticleSystem_ && activeCamera && voxelBufferResource_)
    {
        auto texManager = TextureManager::GetInstance();
        uint32_t whiteTexIndex = texManager ? texManager->GetSrvIndex("white.png") : 0;
        gpuParticleSystem_->DrawCustomCubes(activeCamera->GetViewProjectionMatrix(), voxelSrvIndex_, static_cast<uint32_t>(kMaxVoxelCubes), whiteTexIndex, GPUParticleBlendMode::Alpha);
    }
"@

# 7. Finalize
$oldFin = @"
    gpuParticleSystem_.reset();
    titleMapNodes_.clear();
"@

$newFin = @"
    if (voxelBufferResource_)
    {
        voxelBufferResource_->Unmap(0, nullptr);
        mappedVoxelBuffer_ = nullptr;
    }
    voxelBufferResource_.Reset();
    gpuParticleSystem_.reset();
    titleMapNodes_.clear();
"@

$cLF = $c.Replace("`r`n", "`n")
$cLF = $cLF.Replace($oldInit.Replace("`r`n", "`n"), $newInit.Replace("`r`n", "`n"))
$cLF = $cLF.Replace($oldTriggerHead, $newTriggerHead)
$cLF = $cLF.Replace($oldTriggerBody.Replace("`r`n", "`n"), $newTriggerBody.Replace("`r`n", "`n"))
$cLF = $cLF.Replace($oldPreviewReset.Replace("`r`n", "`n"), $newPreviewReset.Replace("`r`n", "`n"))
$cLF = $cLF.Replace($oldShatter, $newShatter.Replace("`r`n", "`n"))
$cLF = $cLF.Replace($oldDraw.Replace("`r`n", "`n"), $newDraw.Replace("`r`n", "`n"))
$cLF = $cLF.Replace($oldFin.Replace("`r`n", "`n"), $newFin.Replace("`r`n", "`n"))

[System.IO.File]::WriteAllText($p, $cLF.Replace("`n", "`r`n"), [System.Text.Encoding]::UTF8)
Write-Host "Applied all changes to TitleScene.cpp successfully"
