#include "swarm/levels.h"

#include "editor/game/game.h"

#include "imgui/imgui.h"

using namespace editor;
using namespace editor::game;

using namespace swarm;

namespace {
    static constexpr auto kProjectionNames = std::to_array({ "Perspective", "Orthographic" });

    PlayLevel *getPlayLevel(GameLevel *pLevel) {
        return static_cast<PlayLevel*>(pLevel);
    }
}

// alien

OAlien::OAlien(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    const auto *pSwarm = getPlayLevel(pLevel);

    rng.seed(seed);
    dist = std::uniform_real_distribution<float>(0.f, float(pSwarm->getHeight()));

    setMesh("alien.model");
    setTexture("alien.png");

    position = float3::from(2.f, pSwarm->getAlienSpawnPoint());
    rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
}

void OAlien::tick(float) {
    move();
    spawnEgg();
}

void OAlien::move() {
    if (!canMove()) return;
    lastMove = pLevel->getCurrentTime();

    auto *pSwarm = getPlayLevel(pLevel);
    float2 limits = pSwarm->getWorldLimits();

    position.y += 1.f;

    if (position.y > limits.y) {
        position.y = 0.f;
    }
}

bool OAlien::canMove() const {
    float now = pLevel->getCurrentTime();
    return now - lastMove > moveRate;
}

void OAlien::spawnEgg() {
    if (!canSpawnEgg()) return;
    lastEggSpawn = pLevel->getCurrentTime();

    auto *pSwarm = getPlayLevel(pLevel);

    float vertical = roundf(dist(rng));
    float horizontal = roundf(position.y);

    auto *pEgg = pSwarm->newObject<OEgg>("egg");
    pEgg->position = float3::from(2.f, horizontal, vertical);
}

bool OAlien::canSpawnEgg() const {
    float now = pLevel->getCurrentTime();
    return now - lastEggSpawn > eggSpawnRate;
}

// bullet

OBullet::OBullet(GameLevel *pLevel, IGameObject *pParent, float2 velocity)
    : IGameObject(pLevel, "bullet")
    , pParent(pParent)
    , velocity(velocity)
{
    setMesh("bullet.model");
    setTextureHandle(pParent->getTexture()); // TODO: this assumes the parent mesh is loaded already
    setShouldCull(true);

    scale /= 3.f;
}

void OBullet::tick(float delta) {
    position += float3::from(0.f, velocity * delta);
}

bool OBullet::isParent(IGameObject *pObject) const {
    return pParent == pObject;
}

// lives

OLife::OLife(GameLevel *pLevel, size_t life)
    : IGameObject(pLevel, std::format("life-{}", life))
{
    setMesh("ship.model");
    setTexture("player.png");
    setShouldCull(false);
}

// player

OPlayer::OPlayer(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    auto *pSwarm = getPlayLevel(pLevel);

    setMesh("ship.model");
    setTexture("player.png");
    setShouldCull(false); // TODO: we shouldnt need this

    position = float3::from(1.f, pSwarm->getPlayerSpawnPoint());
    rotation = { -90 * kDegToRad<float>, 0.f, 0.f };

    createLives();
}

void OPlayer::tick(float delta) {
    auto *pSwarm = getPlayLevel(pLevel);
    auto *pInput = swarm::getInputClient();
    auto limits = pSwarm->getWorldLimits();

    float moveVertical = 0.f; // = pInput->getHorizontalAxis();
    float moveHorizontal = 0.f; // pInput->getVerticalAxis();

    if (pInput->consumeMoveDown()) {
        moveVertical = -1.f;
    } else if (pInput->consumeMoveUp()) {
        moveVertical = 1.f;
    }

    if (pInput->consumeMoveLeft()) {
        moveHorizontal = -1.f;
    } else if (pInput->consumeMoveRight()) {
        moveHorizontal = 1.f;
    }

    position += float3(0.f, moveHorizontal, moveVertical);

    position.y = math::clamp(position.y, 0.f, limits.x);
    position.z = math::clamp(position.z, 0.f, limits.y);

    float angle = std::atan2(moveVertical, moveHorizontal);

    if (moveVertical != 0.f || moveHorizontal != 0.f)
        rotation.x = -angle;

    if (pInput->isShootPressed())
        tryShootBullet(-rotation.x);
}

void OPlayer::onHit() {
    if (isInvulnerable()) return;

    lastHit = pLevel->getCurrentTime();
    removeLife();
}

bool OPlayer::isInvulnerable() const {
    float now = pLevel->getCurrentTime();
    return now - lastHit < invulnTime;
}

void OPlayer::createLives() {
    for (size_t i = 0; i < initialLives; i++) {
        addLife();
    }
}

void OPlayer::addLife() {
    if (currentLives >= maxLives) {
        return;
    }

    auto *pSwarm = getPlayLevel(pLevel);

    OLife *pLife = pSwarm->newObject<OLife>(currentLives);
    pLife->position = pSwarm->getWorldPos(float(pSwarm->getWidth() - currentLives - 1), -1.f);
    pLife->rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
    lifeObjects.push_back(pLife);
    currentLives += 1;
}

void OPlayer::removeLife() {
    if (currentLives == 0) {
        game::getInstance()->add("game-over", [] {
            game::getInstance()->pushLevel(new GameOverLevel());
        });
        return;
    }

    currentLives -= 1;
    pLevel->deleteObject(lifeObjects.back());
    lifeObjects.pop_back();
}

void OPlayer::tryShootBullet(float angle) {
    float now = pLevel->getCurrentTime();
    if (now - lastFire > fireRate) {
        lastFire = now;

        auto *pSwarm = getPlayLevel(pLevel);

        float2 velocity = float2::from(std::cos(angle), std::sin(angle)) * bulletSpeed;

        auto *pBullet = pSwarm->newObject<OBullet>(this, velocity);
        pBullet->position = position;
        pBullet->rotation = rotation;
    }
}

void OPlayer::debug() {
    ImGui::InputFloat("Speed", &speed);
    ImGui::InputFloat("Bullet Speed", &bulletSpeed);

    ImGui::InputFloat("Fire Rate", &fireRate);

    if (ImGui::Button("Add Life")) {
        addLife();
    }

    if (ImGui::Button("Remove Life")) {
        removeLife();
    }
}

// eggs

OEgg::OEgg(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    setMesh("egg-small.model");
    setTexture("alien.png");
}

void OEgg::tick(float delta) {
    auto *pSwarm = getPlayLevel(pLevel);

    timeAlive += delta;

    if (timeAlive > timeToHatch) {
        auto *pBullet = pSwarm->newObject<OAggroAlien>(pSwarm->getAlien());
        pBullet->position = position;

        pLevel->deleteObject(this);
    } else if (timeAlive > timeToLarge) {
        setMesh("egg-large.model");
    } else if (timeAlive > timeToMedium) {
        setMesh("egg-medium.model");
    }
}

float2 OEgg::getShootVector(IGameObject *pTarget) const {
    float2 targetPos = pTarget->position.yz();
    float2 eggPos = position.yz();

    float2 delta = targetPos - eggPos;
    float2 dir = delta.normalize();

    return dir * bulletSpeed;
}

// aggro alien

OAggroAlien::OAggroAlien(game::GameLevel *pLevel, game::IGameObject *pParent)
    : IGameObject(pLevel, "aggro-alien")
    , pParent(pParent)
{
    setMesh("alien.model");
    setTextureHandle(pParent->getTexture()); // TODO: this assumes the parent mesh is loaded already
    setShouldCull(false);

    rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
}

void OAggroAlien::tick(float) {
    move();
    hitPlayer();
}

void OAggroAlien::move() {
    if (!canMove()) return;
    lastMove = pLevel->getCurrentTime();

    auto *pSwarm = getPlayLevel(pLevel);
    float2 limits = pSwarm->getWorldLimits();

    if (position.y <= 0) {
        bMovingRight = true;
    } else if (position.y >= limits.x) {
        bMovingRight = false;
    }

    if (position.z <= 0) {
        bMovingUp = true;
    } else if (position.z >= limits.y) {
        bMovingUp = false;
    }

    position.z += bMovingUp ? 1.f : -1.f;
    position.y += bMovingRight ? 1.f : -1.f;
}

bool OAggroAlien::canMove() const {
    float now = pLevel->getCurrentTime();
    return now - lastMove > moveRate;
}

void OAggroAlien::hitPlayer() {
    auto *pSwarm = getPlayLevel(pLevel);
    auto *pPlayer = pSwarm->getPlayer();

    float distance = (pPlayer->position.yz() - position.yz()).length();
    if (distance < 0.3f) {
        pPlayer->onHit();
        pLevel->deleteObject(this);
    }
}

// grid

OGrid::OGrid(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    setMesh("grid.model");
    setTexture("cross.png");
}

// plane

OGameOver::OGameOver(game::GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    setMesh("plane.model");
    setTexture("death.png");
    setShouldCull(false);

    rotation = { 0.f, 0.f, 90 * kDegToRad<float> };
    scale = 10.f;
}

void OGameOver::tick(float delta) {
    auto *pInput = swarm::getInputClient();

    if (pInput->isShootPressed()) {
        game::getInstance()->add("play-level", [] {
            game::getInstance()->pushLevel(new swarm::PlayLevel());
        });
    } else if (pInput->isQuitPressed()) {
        game::getInstance()->add("quit", [] {
            game::getInstance()->quit();
        });
    }
}

// game level

PlayLevel::PlayLevel() {
    name = "Swarm:PlayLevel";
    pProjection = projections[currentProjection];

    pAlien = newObject<OAlien>("alien");
    pPlayer = newObject<OPlayer>("player");
    pGrid = newObject<OGrid>("grid");
    pGrid->rotation = float3::from(-90.f * kDegToRad<float>, 0.f, 0.f);

    cameraPosition = float3::from(10.f, float(getWidth()) / 2, float(getHeight()) / 2);
    cameraRotation = float3::from(-1.f, 0.f, 0.f);
}

void PlayLevel::tick(float delta) {
    useEachObject([this, delta](IGameObject *pObject) {
        if (shouldCullObject(pObject))
            deleteObject(pObject);
        else
            pObject->tick(delta);
    });
}

bool PlayLevel::shouldCullObject(IGameObject *pObject) const {
    if (!pObject->canCull()) return false;

    float2 pos = pObject->position.yz();
    float2 limits = getWorldLimits();

    return pos.x < 0.f || pos.x > limits.x || pos.y < 0.f || pos.y > limits.y;
}

void PlayLevel::debug() {
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::SliderFloat3("Position", cameraPosition.data(), -20.f, 20.f);
        ImGui::SliderFloat3("Rotation", cameraRotation.data(), -1.f, 1.f);

        int projection = currentProjection;
        if (ImGui::Combo("Projection", &projection, kProjectionNames.data(), kProjectionNames.size())) {
            setProjection(Projection(projection));
        }

        debug::DebugHandle *pCameraDebug = pProjection->getDebugHandle();
        ImGui::SeparatorText(pCameraDebug->getName());
        pCameraDebug->draw();
    }

    GameLevel::debug();
}

// game over

GameOverLevel::GameOverLevel() {
    name = "Swarm:GameOverLevel";
    pProjection = new game::Orthographic(24.f, 24.f);

    addObject<OGameOver>("game-over");
}

void GameOverLevel::tick(float delta) {
    useEachObject([delta](IGameObject *pObject) {
        pObject->tick(delta);
    });
}

void GameOverLevel::debug() {
    ImGui::SliderFloat3("Position", cameraPosition.data(), -20.f, 20.f);
    ImGui::SliderFloat3("Rotation", cameraRotation.data(), -1.f, 1.f);

    if (cameraPosition == 0.f) {
        cameraPosition.x = 1.f; // just a little nudge :^)
    }

    debug::DebugHandle *pCameraDebug = pProjection->getDebugHandle();
    ImGui::SeparatorText(pCameraDebug->getName());
    pCameraDebug->draw();

    GameLevel::debug();
}
