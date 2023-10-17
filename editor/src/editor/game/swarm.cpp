#include "editor/game/swarm.h"

using namespace editor;
using namespace editor::game;

namespace {
    SwarmGame *getSwarm(GameLevel *pLevel) {
        return static_cast<SwarmGame*>(pLevel);
    }

    const SwarmGameInfo& getInfo(GameLevel *pLevel) {
        SwarmGame *pSwarm = getSwarm(pLevel);
        return pSwarm->getInfo();
    }

    GameInputClient *getInput(GameLevel *pLevel) {
        const auto& info = getInfo(pLevel);
        return info.pInputClient;
    }
}

// alien

OAlien::OAlien(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    const auto& info = getInfo(pLevel);
    const auto *pSwarm = getSwarm(pLevel);

    setMesh(info.pAlienMesh);
    setTextureId(info.alienTextureId);

    position = float3::from(2.f, pSwarm->getAlienSpawnPoint());
    rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
}

void OAlien::tick(float delta) {
    auto *pSwarm = static_cast<SwarmGame*>(pLevel);
    float2 limits = pSwarm->getWorldLimits();

    position.y += speed * delta;
    if (position.y > limits.y) {
        position.y = 0.f;
    }
}

// bullet

OBullet::OBullet(GameLevel *pLevel, IGameObject *pParent, float2 velocity)
    : IGameObject(pLevel, "bullet")
    , pParent(pParent)
    , velocity(velocity)
{
    const auto& info = getInfo(pLevel);

    setMesh(info.pBulletMesh);
    setTextureId(info.bulletTextureId);
    setShouldCull(true);
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
    const auto& info = getInfo(pLevel);

    setMesh(info.pPlayerMesh);
    setTextureId(info.playerTextureId);
    setShouldCull(false);
}

// player

OPlayer::OPlayer(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    auto *pSwarm = getSwarm(pLevel);
    const auto& info = getInfo(pLevel);

    setMesh(info.pPlayerMesh);
    setTextureId(info.playerTextureId);
    setShouldCull(false); // TODO: we shouldnt need this

    position = float3::from(1.f, pSwarm->getPlayerSpawnPoint());
    rotation = { -90 * kDegToRad<float>, 0.f, 0.f };

    createLives();
}

void OPlayer::tick(float delta) {
    auto *pSwarm = getSwarm(pLevel);
    auto *pInput = getInput(pLevel);
    auto limits = pSwarm->getWorldLimits();

    float moveVertical = pInput->getHorizontalAxis();
    float moveHorizontal = pInput->getVerticalAxis();

    position += float3(0.f, moveVertical * speed * delta, moveHorizontal * speed * delta);

    position.y = math::clamp(position.y, 0.f, limits.x);
    position.z = math::clamp(position.z, 0.f, limits.y);

    float angle = std::atan2(moveHorizontal, moveVertical);

    if (moveVertical != 0.f || moveHorizontal != 0.f)
        rotation.x = -angle;

    if (pInput->isShootPressed())
        tryShootBullet(angle);
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

    auto *pSwarm = getSwarm(pLevel);

    OLife *pLife = pSwarm->addObject<OLife>(currentLives);
    pLife->position = pSwarm->getWorldPos(float(pSwarm->getWidth() - currentLives), -1.f);
    pLife->rotation = { -90 * kDegToRad<float>, 0.f, 0.f };
    lifeObjects.push_back(pLife);
    currentLives += 1;
}

void OPlayer::removeLife() {
    if (currentLives == 0) {
        // TODO: report game over
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

        auto *pSwarm = getSwarm(pLevel);

        float2 velocity = float2::from(std::cos(angle), std::sin(angle)) * bulletSpeed;

        auto *pBullet = pSwarm->addObject<OBullet>(this, velocity);
        pBullet->position = position;
        pBullet->rotation = rotation;
        pBullet->scale *= 0.3f;
    }
}

// eggs

OEgg::OEgg(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    const auto& info = getInfo(pLevel);

    setMesh(info.pEggSmallMesh);
    setTextureId(info.eggSmallTextureId);
}

void OEgg::tick(float delta) {
    auto *pSwarm = getSwarm(pLevel);
    const auto& info = getInfo(pLevel);

    timeAlive += delta;

    if (timeAlive > timeToHatch) {
        pLevel->deleteObject(this);
        pLevel->addObject<OBullet>(pSwarm->getAlien(), getShootVector(pSwarm->getPlayer()));
    } else if (timeAlive > timeToLarge) {
        setMesh(info.pEggLargeMesh);
        setTextureId(info.eggLargeTextureId);
    } else if (timeAlive > timeToMedium) {
        setMesh(info.pEggMediumMesh);
        setTextureId(info.eggMediumTextureId);
    }
}

float2 OEgg::getShootVector(IGameObject *pTarget) const {
    float2 playerPos = pTarget->position.xy();
    float2 eggPos = position.xy();

    float2 delta = playerPos - eggPos;
    float2 dir = delta.normalize();

    return dir;
}

// grid

OGrid::OGrid(GameLevel *pLevel, std::string name)
    : IGameObject(pLevel, name)
{
    const auto& info = getInfo(pLevel);
    setMesh(info.pGridMesh);
    setTextureId(info.gridTextureId);
}

// game level

void SwarmGame::create(const SwarmGameInfo& createInfo) {
    info = createInfo;

    pAlien = addObject<OAlien>("alien");
    pPlayer = addObject<OPlayer>("player");
    pGrid = addObject<OGrid>("grid");
    pGrid->rotation = float3::from(-90.f * kDegToRad<float>, 0.f, 0.f);

    cameraPosition = float3::from(10.f, float(getWidth()) / 2, float(getHeight()) / 2);
    cameraRotation = float3::from(-1.f, 0.f, 0.f);

    lastTick = getCurrentTime();
}

void SwarmGame::tick() {
    float now = getCurrentTime();
    float delta = now - lastTick;
    lastTick = now;

    beginTick();

    useEachObject([this, delta](IGameObject *pObject) {
        if (shouldCullObject(pObject))
            deleteObject(pObject);
        else
            pObject->tick(delta);
    });

    endTick();
}

bool SwarmGame::shouldCullObject(IGameObject *pObject) const {
    if (!pObject->canCull()) return false;

    float2 pos = pObject->position.xy();
    float2 limits = getWorldLimits();

    return pos.x < 0.f || pos.x > limits.x || pos.y < 0.f || pos.y > limits.y;
}
