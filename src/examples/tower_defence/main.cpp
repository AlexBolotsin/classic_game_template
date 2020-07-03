#include <examples/tower_defence/pch.h>

#include <examples/tower_defence/tilemap.h>
#include <examples/tower_defence/enemy_data.h>
#include <examples/tower_defence/tower_data.h>
#include <examples/tower_defence/map_data.h>
#include <examples/tower_defence/game_state.h>

int GameMain()
{
    auto window = cgt::WindowConfig::Default()
        .WithTitle("Tower Defence")
        .WithDimensions(1920, 1080)
        .Build();

    auto render = cgt::render::RenderConfig::Default(window)
        .Build();

    auto imguiHelper = cgt::ImGuiHelper::Create(window, render);

    const float basePPU = 64.0f;
    const float SCALE_FACTORS[] = { 4.0f, 3.0f, 2.0f, 1.0f, 1.0f / 2.0f, 1.0f / 3.0f, 1.0f / 4.0f, 1.0f / 5.0f, 1.0f / 6.0f, 1.0f / 7.0f, 1.0f / 8.0f, 1.0f / 9.0f, 1.0f / 10.0f };
    i32 scaleFactorIdx = 3;

    cgt::render::CameraSimpleOrtho camera(*window);
    camera.pixelsPerUnit = 64.0f;

    tson::Tileson mapParser;
    tson::Map map = mapParser.parse(cgt::AssetPath("examples/maps/tower_defense.json"));
    CGT_ASSERT_ALWAYS(map.getStatus() == tson::ParseStatus::OK);

    tson::Tileset* rawTileset = map.getTileset("tower_defense");
    CGT_ASSERT_ALWAYS(rawTileset);

    cgt::render::TextureHandle tilesetTexture = render->LoadTexture(
        cgt::AssetPath("examples/maps") / rawTileset->getImagePath());

    auto tileset = TileSet::Load(*rawTileset, tilesetTexture);
    auto baseMapLayer = StaticTileGrid::Load(map, *map.getLayer("Base"), *rawTileset, 0);
    auto propsMapLayer = StaticTileGrid::Load(map, *map.getLayer("Props"), *rawTileset, 1);

    MapData mapData;
    MapData::Load(map, mapData);

    // https://www.gafferongames.com/post/fix_your_timestep
    const float FIXED_DELTA = 1.0f / 60.0f;
    GameState gameStates[2];
    GameState* prevState = &gameStates[0];
    GameState* nextState = &gameStates[1];

    GameState interpolatedState;

    std::default_random_engine randEngine;
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    cgt::Clock clock;
    float accumulatedDelta = 0.0f;
    cgt::render::RenderQueue renderQueue;
    cgt::render::RenderStats renderStats {};
    SDL_Event event {};

    bool quitRequested = false;
    while (!quitRequested)
    {
        ZoneScopedN("Main Loop");

        const float dt = clock.Tick();
        accumulatedDelta += dt;

        imguiHelper->NewFrame(dt, camera);

        // NOTE: prone to "spiral of death"
        // see https://www.gafferongames.com/post/fix_your_timestep/
        while (accumulatedDelta > FIXED_DELTA)
        {
            accumulatedDelta -= FIXED_DELTA;
            std::swap(prevState, nextState);
            GameState::TimeStep(mapData, *prevState, *nextState, FIXED_DELTA);
        }

        const float interpolationFactor = glm::smoothstep(0.0f, FIXED_DELTA, accumulatedDelta);
        GameState::Interpolate(*prevState, *nextState, interpolatedState, interpolationFactor);

        {
            ImGui::SetNextWindowSize({200, 80}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Render Stats");
            ImGui::Text("Frame time: %.2fms", dt * 1000.0f);
            ImGui::Text("Sprites: %u", renderStats.spriteCount);
            ImGui::Text("Drawcalls: %u", renderStats.drawcallCount);
            ImGui::End();
        }

        while (window->PollEvent(event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                quitRequested = true;
                break;
            case SDL_MOUSEWHEEL:
                auto wheel = event.wheel;
                scaleFactorIdx -= wheel.y;
                scaleFactorIdx = glm::clamp(scaleFactorIdx, 0, (i32)SDL_arraysize(SCALE_FACTORS) - 1);
            }
        }

        glm::vec2 cameraMovInput(0.0f);
        const float CAMERA_SPEED = 5.0f;
        auto* keyboard = SDL_GetKeyboardState(nullptr);
        if (keyboard[SDL_SCANCODE_S])
        {
            cameraMovInput.y -= 1.0f;
        }
        if (keyboard[SDL_SCANCODE_W])
        {
            cameraMovInput.y += 1.0f;
        }
        if (keyboard[SDL_SCANCODE_A])
        {
            cameraMovInput.x -= 1.0f;
        }
        if (keyboard[SDL_SCANCODE_D])
        {
            cameraMovInput.x += 1.0f;
        }

        const float cameraMovInputLengthSqr = glm::dot(cameraMovInput, cameraMovInput);
        if (cameraMovInputLengthSqr > 0.0f)
        {
            cameraMovInput /= glm::sqrt(cameraMovInputLengthSqr);
            camera.position += cameraMovInput * dt * CAMERA_SPEED * (1.0f / SCALE_FACTORS[scaleFactorIdx]);
        }

        camera.pixelsPerUnit = basePPU * SCALE_FACTORS[scaleFactorIdx];


        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        glm::vec2 world = camera.ScreenToWorld((u32)mouseX, (u32)mouseY);
        auto tilePos = mapData.buildableMap.WorldToTile(world);
        bool buildable = mapData.buildableMap.Query(world);

        {
            ImGui::Begin("Mouse To World");

            ImGui::Text("Screen position: (%d, %d)", mouseX, mouseY);
            ImGui::Text("World position: (%.2f, %.2f)", world.x, world.y);
            ImGui::Text("Tile Pos: (%d, %d)", tilePos.x, tilePos.y);
            ImGui::Text("Buildable: %d", (int)buildable);

            ImGui::End();
        }

        auto selectionColor = buildable
            ? glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)
            : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        {
            Im3d::PushAlpha(0.2f);
            Im3d::PushColor(Im3d::Color(selectionColor));
            Im3d::DrawAlignedBoxFilled({ tilePos.x - 0.5f, tilePos.y - 0.5f, 0.0f }, { tilePos.x + 0.5f, tilePos.y + 0.5f, 0.0f });
            Im3d::PopColor();
            Im3d::PopAlpha();
        }

        const float DT_SCALE_FACTORS[] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        const char* DT_SCALE_FACTORS_STR[] = { "0", "0.25", "0.5", "1.0", "2.0", "4.0" };
        static u32 selectedDtScaleIdx = 3;
        {
            ImGui::Begin("Gameplay Settings");

            for (u32 i = 0; i < SDL_arraysize(DT_SCALE_FACTORS); ++i)
            {
                if (ImGui::RadioButton(DT_SCALE_FACTORS_STR[i], i == selectedDtScaleIdx))
                {
                    selectedDtScaleIdx = i;
                }
            }

            ImGui::End();
        }

        const float scaledDt = dt * DT_SCALE_FACTORS[selectedDtScaleIdx];

        {
            static u32 selectedEnemyIdx = 0, selectedPathIdx = 0;
            ImGui::Begin("Spawn Enemies");

            if (ImGui::BeginCombo("Enemy Type", mapData.enemyTypes[selectedEnemyIdx].name.c_str()))
            {
                for (u32 i = 0; i < mapData.enemyTypes.size(); ++i)
                {
                    bool selected = i == selectedEnemyIdx;
                    if (ImGui::Selectable(mapData.enemyTypes[i].name.c_str(), selected))
                    {
                        selectedEnemyIdx = i;
                    }

                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            static i32 enemiesToSpawn = 20;

            ImGui::InputInt("Enemies To Spawn", &enemiesToSpawn);

            if (ImGui::Button("Spawn Enemy"))
            {
                for (i32 i = 0; i < enemiesToSpawn; ++i)
                {
//                    EnemyType& enemyType = enemyTypes[selectedEnemyIdx];
//                    auto enemy = enemyType.CreateEnemy(enemyPath, selectedEnemyIdx);
//                    glm::vec2 randomShift(
//                        distribution(randEngine),
//                        distribution(randEngine));
//                    enemy.position += randomShift;
//
//                    enemies.emplace_back(enemy);
                }
            }

            if (ImGui::Button("Despawn All"))
            {
                //enemies.clear();
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Towers");
            for (auto& towerType : mapData.towerTypes)
            {
                auto textureId = render->GetImTextureID(tileset->GetTexture());
                auto uv = (*tileset)[towerType.tileId];

                if (ImGui::ImageButton(textureId, { 64.0f, 64.0f }, { uv.min.x, uv.min.y }, { uv.max.x, uv.max.y }))
                {

                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();

                    ImGui::Text("%s", towerType.name.c_str());
                    ImGui::Separator();

                    ImGui::TextColored({ 0.9f, 0.8f, 0.2f, 1.0f }, "%.0f$", towerType.cost);

                    ImGui::Text("Range: ");
                    ImGui::SameLine();
                    ImGui::TextColored({ 0.2f, 0.8f, 0.2f, 1.0f }, "%.0f tiles", towerType.range);

                    ImGui::Text("Damage: ");
                    ImGui::SameLine();
                    ImGui::TextColored({ 0.8f, 0.2f, 0.2f, 1.0f }, "%.0f", towerType.damage);

                    if (towerType.splashRadius > 0.0f)
                    {
                        ImGui::Text("Splash: ");
                        ImGui::SameLine();
                        ImGui::TextColored({ 0.8f, 0.2f, 0.2f, 1.0f }, "%.1f", towerType.splashRadius);
                    }

                    ImGui::Text("Shots per second: ");
                    ImGui::SameLine();
                    ImGui::TextColored({ 0.2f, 0.8f, 0.2f, 1.0f }, "%.1f", towerType.shotsPerSecond);

                    ImGui::EndTooltip();
                }

                ImGui::SameLine();
            }
            ImGui::End();
        }


        renderQueue.Reset();
        renderQueue.clearColor = { 0.5f, 0.5f, 0.5f, 1.0f };

        baseMapLayer->Render(renderQueue, *tileset);
        propsMapLayer->Render(renderQueue, *tileset);

        for (auto& enemy : interpolatedState.enemies)
        {
            auto& enemyType = mapData.enemyTypes[enemy.type];

            cgt::render::SpriteDrawRequest sprite;
            sprite.rotation = cgt::math::VectorAngle(enemy.direction);
            sprite.position = enemy.position;
            sprite.texture = tileset->GetTexture();
            auto uv = (*tileset)[enemyType.tileId];
            sprite.uvMin = uv.min;
            sprite.uvMax = uv.max;
            renderQueue.sprites.emplace_back(std::move(sprite));
        }

        mapData.enemyPath.DebugRender();

        renderStats = render->Submit(renderQueue, camera);
        imguiHelper->RenderUi(camera);
        render->Present();

        TracyPlot("Sprites", (i64)renderStats.spriteCount);
        TracyPlot("Drawcalls", (i64)renderStats.drawcallCount);
    }

    return 0;
}