#include <examples/tower_defence/pch.h>

#include <examples/tower_defence/tilemap.h>

struct EnemyPath
{
    std::string debugName;
    glm::vec4 debugColor;
    std::vector<glm::vec2> waypoints;

    void DebugRender()
    {
        Im3d::PushAlpha(0.5f);
        Im3d::PushColor({ debugColor.r, debugColor.g, debugColor.b, debugColor.a });

        Im3d::Text(glm::vec3(waypoints[0], 0.0f), Im3d::TextFlags_AlignTop, debugName.c_str());

        Im3d::PushSize(3.0f);

        Im3d::BeginLineStrip();
        for (auto point : waypoints)
        {
            Im3d::Vertex(glm::vec3(point, 0.0f));
        }
        Im3d::End();

        Im3d::PopSize();
        Im3d::PopColor();
        Im3d::PopAlpha();
    }
};

typedef u32 EnemyTypeId;

struct Enemy
{
    glm::vec2 position;
    glm::vec2 direction;

    EnemyTypeId typeId;

    float remainingHealth;

    u32 targetPointIdx;
};

struct EnemyType
{
    float maxHealth;
    float speed;
    u32 unitsPerSpawn;
    TileSet::TileId tileId;

    std::string name;

    Enemy CreateEnemy(const EnemyPath& path, EnemyTypeId typeId)
    {
        Enemy enemy;
        const auto a = path.waypoints[0];
        const auto b = path.waypoints[1];
        enemy.position = a;
        enemy.direction = glm::normalize(b - a);
        enemy.typeId = typeId;
        enemy.remainingHealth = maxHealth;
        enemy.targetPointIdx = 1;

        return enemy;
    }
};

int GameMain()
{
    auto window = cgt::WindowConfig::Default()
        .WithTitle("Tower Defence")
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

    EnemyPath enemyPath;
    auto* pathLayer = map.getLayer("Paths");
    CGT_ASSERT_ALWAYS(pathLayer && pathLayer->getType() == tson::LayerType::ObjectGroup);

    {
        auto object = pathLayer->getObjectsByType(tson::ObjectType::Polyline)[0];
        enemyPath.debugName = object.getName();
        auto color = object.get<tson::Colori>("Color").asFloat();
        enemyPath.debugColor = { color.r, color.g, color.b, color.a };

        glm::vec3 basePosition(
            (float)object.getPosition().x / map.getTileSize().x - 0.5f,
            (float)object.getPosition().y / map.getTileSize().y * -1.0f + 0.5f,
            0.0f);

        glm::mat4 baseRotation = glm::rotate(
            glm::mat4(1.0f),
            glm::radians(object.getRotation()),
            { 0.0f, 0.0f, 1.0f });

        for (auto& point: object.getPolylines())
        {
            glm::vec3 pointPosition(
                (float)point.x / map.getTileSize().x,
                (float)point.y / map.getTileSize().y * -1.0f,
                0.0f);

            glm::vec3 finalPosition = baseRotation * glm::vec4(pointPosition, 1.0f);
            finalPosition += basePosition;

            enemyPath.waypoints.emplace_back(finalPosition);
        }
    }

    std::vector<EnemyType> enemyTypes;
    auto* enemyLayer = map.getLayer("EnemyTypes");
    CGT_ASSERT_ALWAYS(enemyLayer && enemyLayer->getType() == tson::LayerType::ObjectGroup);
    for (auto& enemyData : enemyLayer->getObjectsByType(tson::ObjectType::Object))
    {
        auto& enemyType = enemyTypes.emplace_back();
        enemyType.name = enemyData.getName();
        enemyType.tileId = enemyData.getGid() - rawTileset->getFirstgid();
        enemyType.maxHealth = enemyData.get<float>("Health");
        enemyType.speed = enemyData.get<float>("Speed");
        enemyType.unitsPerSpawn = enemyData.get<int>("UnitsPerSpawn");
    }

    std::vector<Enemy> enemies;
    
    std::default_random_engine randEngine;
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    cgt::Clock clock;
    cgt::render::RenderQueue renderQueue;
    cgt::render::RenderStats renderStats {};
    SDL_Event event {};

    bool quitRequested = false;
    while (!quitRequested)
    {
        ZoneScopedN("Main Loop");

        const float dt = clock.Tick();
        imguiHelper->NewFrame(dt, camera);

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

            if (ImGui::BeginCombo("Enemy Type", enemyTypes[selectedEnemyIdx].name.c_str()))
            {
                for (u32 i = 0; i < enemyTypes.size(); ++i)
                {
                    bool selected = i == selectedEnemyIdx;
                    if (ImGui::Selectable(enemyTypes[i].name.c_str(), selected))
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
                    EnemyType& enemyType = enemyTypes[selectedEnemyIdx];
                    auto enemy = enemyType.CreateEnemy(enemyPath, selectedEnemyIdx);
                    glm::vec2 randomShift(
                        distribution(randEngine),
                        distribution(randEngine));
                    enemy.position += randomShift;

                    enemies.emplace_back(enemy);
                }
            }

            if (ImGui::Button("Despawn All"))
            {
                enemies.clear();
            }

            ImGui::End();
        }

        static float flockWaypointSteeringFactor = 1.0f;
        static float flockCommonDirectionFactor = 0.2f;

        static float flockSightRange = 3.0f;

        static float flockMaxPathDistance = 0.8f;
        static float flockPathReadjustFactor = 0.4f;

        static float flockDesiredSpacing = 0.3f;
        static float flockPushbackFactor = 0.7f;

        static float flockCenteringFactor = 0.3f;
        {
            ImGui::Begin("Flocking Options");

            ImGui::DragFloat("Sight Range", &flockSightRange, 0.1f, 0.0f, 5.0f);
            ImGui::DragFloat("Max Path Distance", &flockMaxPathDistance, 0.1f, 0.0f, 5.0f);
            ImGui::DragFloat("Desired Spacing", &flockDesiredSpacing, 0.1f, 0.0f, 5.0f);

            ImGui::TextUnformatted("Factors");
            ImGui::DragFloat("Waypoint", &flockWaypointSteeringFactor, 0.1f, 0.0f, 1.0f);
            ImGui::DragFloat("Common Direction", &flockCommonDirectionFactor, 0.1f, 0.0f, 1.0f);
            ImGui::DragFloat("Path Readjust", &flockPathReadjustFactor, 0.1f, 0.0f, 1.0f);
            ImGui::DragFloat("Pushback", &flockPushbackFactor, 0.1f, 0.0f, 1.0f);
            ImGui::DragFloat("Centering", &flockCenteringFactor, 0.1f, 0.0f, 1.0f);

            ImGui::End();
        }

        for (auto& enemy : enemies)
        {
            if (enemy.targetPointIdx >= enemyPath.waypoints.size())
            {
                continue;
            }

            auto& enemyType = enemyTypes[enemy.typeId];

            const glm::vec2 a = enemyPath.waypoints[enemy.targetPointIdx - 1];
            const glm::vec2 b = enemyPath.waypoints[enemy.targetPointIdx];

            const glm::vec2 nextWaypointDirection = glm::normalize(b - enemy.position);

            const glm::vec2 closestPathPoint = cgt::math::AABBClosestPoint(
                enemy.position,
                glm::min(a, b),
                glm::max(a, b));

            if (glm::distance(closestPathPoint, b) < 0.1f)
            {
                ++enemy.targetPointIdx;
            }

            u32 othersInSight = 0;
            glm::vec2 othersCumulativePosition(0.0f);
            glm::vec2 othersCumulativeDirection(0.0f);
            glm::vec2 pushbackFromOthers(0.0f);
            for (const auto& otherEnemy : enemies)
            {
                if (&enemy == &otherEnemy)
                {
                    continue;
                }

                glm::vec2 fromOther = enemy.position - otherEnemy.position;
                float fromOtherDistSqr = glm::dot(fromOther, fromOther);
                if (fromOtherDistSqr > flockSightRange * flockSightRange)
                {
                    continue;
                }

                ++othersInSight;

                othersCumulativePosition += otherEnemy.position;
                othersCumulativeDirection += otherEnemy.direction;

                if (fromOtherDistSqr < flockDesiredSpacing * flockDesiredSpacing)
                {
                    float fromOtherDist = glm::sqrt(fromOtherDistSqr);
                    glm::vec2 fromOtherNorm = fromOther / fromOtherDist;
                    float pushbackForce = 1.0f - glm::smoothstep(0.0f, flockDesiredSpacing, fromOtherDist);
                    pushbackFromOthers += fromOtherNorm * pushbackForce;
                }
            }

            glm::vec2 directionDelta = nextWaypointDirection * flockWaypointSteeringFactor;
            if (othersInSight > 0)
            {
                glm::vec2 othersAveragePosition = othersCumulativePosition / (float)othersInSight;
                glm::vec2 centeringDirection = glm::normalize(othersAveragePosition - enemy.position);
                directionDelta += centeringDirection * flockCenteringFactor;

                directionDelta += glm::normalize(othersCumulativeDirection) * flockCommonDirectionFactor;

                directionDelta += pushbackFromOthers;
            }

            enemy.direction = glm::normalize(enemy.direction + directionDelta);
            enemy.position += enemy.direction * enemyType.speed * scaledDt;
        }

        renderQueue.Reset();
        renderQueue.clearColor = { 0.5f, 0.5f, 0.5f, 1.0f };

        baseMapLayer->Render(renderQueue, *tileset);
        propsMapLayer->Render(renderQueue, *tileset);

        for (auto& enemy : enemies)
        {
            auto& enemyType = enemyTypes[enemy.typeId];

            cgt::render::SpriteDrawRequest sprite;
            sprite.rotation = cgt::math::VectorAngle(enemy.direction);
            sprite.position = enemy.position;
            sprite.texture = tileset->GetTexture();
            auto uv = (*tileset)[enemyType.tileId];
            sprite.uvMin = uv.min;
            sprite.uvMax = uv.max;
            renderQueue.sprites.emplace_back(std::move(sprite));
        }

        enemyPath.DebugRender();

        renderStats = render->Submit(renderQueue, camera);
        imguiHelper->RenderUi(camera);
        render->Present();

        TracyPlot("Sprites", (i64)renderStats.spriteCount);
        TracyPlot("Drawcalls", (i64)renderStats.drawcallCount);
    }

    return 0;
}