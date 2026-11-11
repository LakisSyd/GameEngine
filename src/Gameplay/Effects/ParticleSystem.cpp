#include "ParticleSystem.h"
#include "ParticleRenderer.h"

#include <algorithm>
#include <cmath>

#include <PhysicsDebugRenderer.h>

using namespace DirectX;

ParticleSystem* ParticleSystem::s_activeSystem = nullptr;

namespace
{
    float Clamp01(float value)
    {
        return (std::max)(0.0f, (std::min)(1.0f, value));
    }

    float Lerp(float from, float to, float amount)
    {
        return from + (to - from) * amount;
    }

    XMFLOAT4 LerpColor(const XMFLOAT4& from, const XMFLOAT4& to, float amount)
    {
        return {
            Lerp(from.x, to.x, amount),
            Lerp(from.y, to.y, amount),
            Lerp(from.z, to.z, amount),
            Lerp(from.w, to.w, amount)
        };
    }

    XMFLOAT3 NormalizeOrUp(const XMFLOAT3& value)
    {
        XMVECTOR vector = XMLoadFloat3(&value);
        if (XMVectorGetX(XMVector3LengthSq(vector)) < 0.000001f)
            return { 0.0f, 0.0f, 0.0f };

        XMFLOAT3 result{};
        XMStoreFloat3(&result, XMVector3Normalize(vector));
        return result;
    }
}

ParticleSystem::ParticleSystem()
{
    s_activeSystem = this;
    m_particles.reserve(MAX_PARTICLES);
    m_emitters.reserve(MAX_EMITTERS);
    m_renderItems.reserve(MAX_PARTICLES);
}

ParticleSystem::~ParticleSystem()
{
    m_renderer.reset();
    if (s_activeSystem == this)
        s_activeSystem = nullptr;
}

void ParticleSystem::Render(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& cameraPosition,
    float fallbackDeltaTime)
{
    // В Play Mode система уже обновлена ApplicationClass. В Editor Mode этот
    // запасной шаг позволяет сразу проверять F7/F8/F9 без двойной симуляции.
    if (!m_updatedSinceLastRender)
        Update(fallbackDeltaTime, nullptr);

    if (!m_renderer && !m_rendererInitializationAttempted)
    {
        m_rendererInitializationAttempted = true;
        auto renderer = std::make_unique<ParticleRenderer>();
        if (renderer->Initialize(device))
            m_renderer = std::move(renderer);
        else
            OutputDebugStringA("[Particles] Failed to initialize particle renderer.\n");
    }

    if (m_renderer)
        m_renderer->Render(context, view, projection, cameraPosition, m_renderItems);

    m_updatedSinceLastRender = false;
}

ParticleEmitterId ParticleSystem::CreateEmitter(const ParticleEmitterDesc& desc)
{
    if (m_emitters.size() >= MAX_EMITTERS)
        return INVALID_PARTICLE_EMITTER;

    Emitter emitter;
    emitter.id = m_nextEmitterId++;
    if (emitter.id == INVALID_PARTICLE_EMITTER)
        emitter.id = m_nextEmitterId++;
    emitter.desc = desc;
    emitter.spawnAccumulator = desc.emitImmediately ? 1.0f : 0.0f;
    m_emitters.push_back(emitter);
    return emitter.id;
}

ParticleEmitterId ParticleSystem::CreateEmitter(
    ParticleEffectType effect,
    const XMFLOAT3& position,
    float scale)
{
    if (effect == ParticleEffectType::Explosion)
    {
        EmitExplosion(position, scale);
        return INVALID_PARTICLE_EMITTER;
    }

    return CreateEmitter(MakePreset(effect, position, scale));
}

bool ParticleSystem::SetEmitterPosition(ParticleEmitterId id, const XMFLOAT3& position)
{
    const auto it = std::find_if(
        m_emitters.begin(),
        m_emitters.end(),
        [id](const Emitter& emitter) { return emitter.id == id; });

    if (it == m_emitters.end())
        return false;

    it->desc.position = position;
    return true;
}

bool ParticleSystem::SetEmitterActive(ParticleEmitterId id, bool active)
{
    const auto it = std::find_if(
        m_emitters.begin(),
        m_emitters.end(),
        [id](const Emitter& emitter) { return emitter.id == id; });

    if (it == m_emitters.end())
        return false;

    it->active = active;
    return true;
}

bool ParticleSystem::UpdateEmitter(ParticleEmitterId id, const ParticleEmitterDesc& desc)
{
    const auto it = std::find_if(
        m_emitters.begin(),
        m_emitters.end(),
        [id](const Emitter& emitter) { return emitter.id == id; });

    if (it == m_emitters.end())
        return false;

    // Keep the fractional accumulator so editing a live emitter does not cause
    // a visible pause or an accidental burst.
    it->desc = desc;
    return true;
}

void ParticleSystem::DestroyEmitter(ParticleEmitterId id)
{
    m_emitters.erase(
        std::remove_if(
            m_emitters.begin(),
            m_emitters.end(),
            [id](const Emitter& emitter) { return emitter.id == id; }),
        m_emitters.end());
}

void ParticleSystem::EmitBurst(const ParticleEmitterDesc& desc, int particleCount)
{
    particleCount = (std::max)(0, particleCount);
    for (int i = 0; i < particleCount && m_particles.size() < MAX_PARTICLES; ++i)
        SpawnParticle(desc);
}

void ParticleSystem::ReloadTextures()
{
    if (m_renderer)
        m_renderer->ClearTextureCache();
}

void ParticleSystem::EmitExplosion(const XMFLOAT3& position, float scale)
{
    scale = (std::max)(0.05f, scale);

    // Короткая центральная вспышка.
    ParticleEmitterDesc flash;
    flash.position = position;
    flash.direction = { 0.0f, 0.0f, 0.0f };
    flash.positionRadius = 0.08f * scale;
    flash.speedMin = 0.0f;
    flash.speedMax = 0.35f * scale;
    flash.lifetimeMin = 0.10f;
    flash.lifetimeMax = 0.18f;
    flash.startSizeMin = 0.7f * scale;
    flash.startSizeMax = 1.0f * scale;
    flash.endSizeMin = 2.0f * scale;
    flash.endSizeMax = 2.8f * scale;
    flash.startColor = { 1.0f, 0.95f, 0.68f, 1.0f };
    flash.endColor = { 1.0f, 0.16f, 0.01f, 0.0f };
    flash.shape = ParticleShape::SoftDisc;
    flash.blendMode = ParticleBlendMode::Additive;
    EmitBurst(flash, 7);

    // Основной огненный объём.
    ParticleEmitterDesc fireball = MakePreset(ParticleEffectType::Fire, position, scale * 2.2f);
    fireball.direction = { 0.0f, 0.0f, 0.0f };
    fireball.positionRadius = 0.18f * scale;
    fireball.speedMin = 0.7f * scale;
    fireball.speedMax = 3.4f * scale;
    fireball.lifetimeMin = 0.30f;
    fireball.lifetimeMax = 0.75f;
    fireball.acceleration = { 0.0f, 0.7f, 0.0f };
    EmitBurst(fireball, 46);

    // Дым живёт дольше огня и раскрывает форму взрыва после вспышки.
    ParticleEmitterDesc smoke = MakePreset(ParticleEffectType::Smoke, position, scale * 1.5f);
    smoke.direction = { 0.0f, 0.0f, 0.0f };
    smoke.positionRadius = 0.22f * scale;
    smoke.speedMin = 0.5f * scale;
    smoke.speedMax = 2.5f * scale;
    smoke.lifetimeMin = 1.4f;
    smoke.lifetimeMax = 3.0f;
    EmitBurst(smoke, 32);

    // Искры дают скорости и масштабу понятный визуальный ориентир.
    ParticleEmitterDesc sparks;
    sparks.position = position;
    sparks.direction = { 0.0f, 0.0f, 0.0f };
    sparks.acceleration = { 0.0f, -5.0f, 0.0f };
    sparks.positionRadius = 0.10f * scale;
    sparks.speedMin = 4.0f * scale;
    sparks.speedMax = 11.0f * scale;
    sparks.lifetimeMin = 0.35f;
    sparks.lifetimeMax = 0.85f;
    sparks.startSizeMin = 0.05f * scale;
    sparks.startSizeMax = 0.11f * scale;
    sparks.endSizeMin = 0.01f * scale;
    sparks.endSizeMax = 0.03f * scale;
    sparks.startColor = { 1.0f, 0.82f, 0.30f, 1.0f };
    sparks.endColor = { 1.0f, 0.12f, 0.01f, 0.0f };
    sparks.drag = 0.35f;
    sparks.aspectRatio = 0.18f;
    sparks.shape = ParticleShape::Spark;
    sparks.blendMode = ParticleBlendMode::Additive;
    EmitBurst(sparks, 72);

    // Расширяющееся кольцо подчёркивает ударную волну.
    ParticleEmitterDesc shockwave;
    shockwave.position = position;
    shockwave.speedMin = 0.0f;
    shockwave.speedMax = 0.0f;
    shockwave.lifetimeMin = 0.30f;
    shockwave.lifetimeMax = 0.38f;
    shockwave.startSizeMin = 0.4f * scale;
    shockwave.startSizeMax = 0.4f * scale;
    shockwave.endSizeMin = 5.0f * scale;
    shockwave.endSizeMax = 5.0f * scale;
    shockwave.startColor = { 1.0f, 0.76f, 0.30f, 0.75f };
    shockwave.endColor = { 1.0f, 0.20f, 0.02f, 0.0f };
    shockwave.shape = ParticleShape::Shockwave;
    shockwave.blendMode = ParticleBlendMode::Additive;
    EmitBurst(shockwave, 1);
}

void ParticleSystem::EmitMetalImpact(const DamageEvent& damageEvent)
{
    ParticleEmitterDesc sparks;
    sparks.startColor = { 1.0f, 0.85f, 0.30f, 1.0f };
    sparks.endColor = { 1.0f, 0.10f, 0.01f, 0.0f };
    sparks.speedMin = 2.0f;
    sparks.speedMax = 5.0f;
    sparks.lifetimeMin = 0.12f;
    sparks.lifetimeMax = 0.35f;
    sparks.startSizeMin = 0.035f;
    sparks.startSizeMax = 0.075f;
    sparks.endSizeMin = 0.01f;
    sparks.endSizeMax = 0.02f;
    sparks.directionSpread = 0.70f;
    sparks.acceleration = { 0.0f, -6.8f, 0.0f };
    sparks.drag = 0.45f;
    sparks.aspectRatio = 0.2f;
    sparks.shape = ParticleShape::Spark;
    sparks.blendMode = ParticleBlendMode::Additive;
    EmitDirectedBurst(damageEvent, sparks, 10);
}

void ParticleSystem::EmitHullDent(const DamageEvent& damageEvent)
{
    EmitMetalImpact(damageEvent);

    ParticleEmitterDesc dust;
    dust.startColor = { 0.42f, 0.44f, 0.47f, 0.55f };
    dust.endColor = { 0.16f, 0.17f, 0.18f, 0.0f };
    dust.speedMin = 0.5f;
    dust.speedMax = 1.8f;
    dust.lifetimeMin = 0.45f;
    dust.lifetimeMax = 0.9f;
    dust.startSizeMin = 0.08f;
    dust.startSizeMax = 0.18f;
    dust.endSizeMin = 0.25f;
    dust.endSizeMax = 0.55f;
    dust.directionSpread = 0.85f;
    dust.acceleration = { 0.0f, 0.15f, 0.0f };
    dust.drag = 1.8f;
    dust.shape = ParticleShape::Smoke;
    EmitDirectedBurst(damageEvent, dust, 7);
}

void ParticleSystem::EmitHullCrack(const DamageEvent& damageEvent)
{
    XMVECTOR normal = XMLoadFloat3(&damageEvent.hit_normal);
    if (XMVectorGetX(XMVector3LengthSq(normal)) < 0.0001f)
        normal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    normal = XMVector3Normalize(normal);

    const XMVECTOR reference = std::fabs(XMVectorGetY(normal)) < 0.9f
        ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    const XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(reference, normal));
    const XMVECTOR bitangent = XMVector3Normalize(XMVector3Cross(normal, tangent));
    const XMVECTOR center = XMVectorAdd(
        XMLoadFloat3(&damageEvent.hit_position),
        XMVectorScale(normal, 0.015f));

    constexpr int branchCount = 8;
    for (int i = 0; i < branchCount; ++i)
    {
        const float angle = XM_2PI * static_cast<float>(i) /
            static_cast<float>(branchCount) + NextRandomSigned() * 0.18f;
        const float length = 0.16f + NextRandom01() * 0.22f;
        const XMVECTOR direction = XMVectorAdd(
            XMVectorScale(tangent, std::cos(angle)),
            XMVectorScale(bitangent, std::sin(angle)));

        CrackLine crack;
        XMStoreFloat3(&crack.start, center);
        XMStoreFloat3(
            &crack.end,
            XMVectorAdd(center, XMVectorScale(direction, length)));
        m_cracks.push_back(crack);
    }

    EmitHullDent(damageEvent);
}

void ParticleSystem::EmitHullBreach(const DamageEvent& damageEvent)
{
    EmitHullDent(damageEvent);

    ParticleEmitterDesc hotGas = MakePreset(
        ParticleEffectType::Fire,
        damageEvent.hit_position,
        0.65f);
    hotGas.speedMin = 2.5f;
    hotGas.speedMax = 7.0f;
    hotGas.lifetimeMin = 0.25f;
    hotGas.lifetimeMax = 0.65f;
    hotGas.directionSpread = 0.6f;
    EmitDirectedBurst(damageEvent, hotGas, 32);

    ParticleEmitterDesc smoke = MakePreset(
        ParticleEffectType::Smoke,
        damageEvent.hit_position,
        0.55f);
    smoke.speedMin = 0.8f;
    smoke.speedMax = 2.2f;
    smoke.directionSpread = 0.9f;
    EmitDirectedBurst(damageEvent, smoke, 16);
}

void ParticleSystem::Update(float deltaTime, PhysicsDebugRenderer* debugRenderer)
{
    m_updatedSinceLastRender = true;
    if (deltaTime <= 0.0f)
        return;

    // Ограничение шага не даёт частицам телепортироваться после паузы в отладчике.
    deltaTime = (std::min)(deltaTime, 0.10f);

    for (Emitter& emitter : m_emitters)
    {
        if (!emitter.active || emitter.desc.particlesPerSecond <= 0.0f)
            continue;

        emitter.spawnAccumulator += emitter.desc.particlesPerSecond * deltaTime;
        const int spawnCount = (std::min)(
            static_cast<int>(emitter.spawnAccumulator),
            128);
        emitter.spawnAccumulator -= static_cast<float>(spawnCount);

        for (int i = 0; i < spawnCount && m_particles.size() < MAX_PARTICLES; ++i)
            SpawnParticle(emitter.desc);
    }

    for (Particle& particle : m_particles)
    {
        const XMFLOAT3 previousPosition = particle.position;

        particle.velocity.x += particle.acceleration.x * deltaTime;
        particle.velocity.y += particle.acceleration.y * deltaTime;
        particle.velocity.z += particle.acceleration.z * deltaTime;

        const float damping = std::exp(-particle.drag * deltaTime);
        particle.velocity.x *= damping;
        particle.velocity.y *= damping;
        particle.velocity.z *= damping;

        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;
        particle.rotation += particle.rotationSpeed * deltaTime;
        particle.age += deltaTime;

        if (debugRenderer && particle.debugTrail && particle.age < particle.lifetime)
        {
            const float life = 1.0f - Clamp01(particle.age / particle.lifetime);
            XMFLOAT4 color = LerpColor(particle.startColor, particle.endColor, 1.0f - life);
            debugRenderer->AddEffectLine(previousPosition, particle.position, color, 0.04f);
        }
    }

    m_particles.erase(
        std::remove_if(
            m_particles.begin(),
            m_particles.end(),
            [](const Particle& particle) { return particle.age >= particle.lifetime; }),
        m_particles.end());

    for (CrackLine& crack : m_cracks)
    {
        crack.age += deltaTime;
        if (debugRenderer && crack.age < crack.lifetime)
        {
            const float alpha = 1.0f - Clamp01(crack.age / crack.lifetime);
            debugRenderer->AddEffectLine(
                crack.start,
                crack.end,
                { 0.12f, 0.04f, 0.02f, alpha },
                0.035f);
        }
    }

    m_cracks.erase(
        std::remove_if(
            m_cracks.begin(),
            m_cracks.end(),
            [](const CrackLine& crack) { return crack.age >= crack.lifetime; }),
        m_cracks.end());

    RebuildRenderItems();
}

void ParticleSystem::Clear()
{
    m_particles.clear();
    m_emitters.clear();
    m_renderItems.clear();
    m_cracks.clear();
}

ParticleEmitterDesc ParticleSystem::MakePreset(
    ParticleEffectType effect,
    const XMFLOAT3& position,
    float scale)
{
    scale = (std::max)(0.05f, scale);
    ParticleEmitterDesc desc;
    desc.position = position;

    if (effect == ParticleEffectType::Fire)
    {
        desc.direction = { 0.0f, 1.0f, 0.0f };
        desc.acceleration = { 0.0f, 0.9f * scale, 0.0f };
        desc.startColor = { 1.0f, 0.88f, 0.24f, 0.95f };
        desc.endColor = { 0.95f, 0.04f, 0.005f, 0.0f };
        desc.particlesPerSecond = 46.0f * scale;
        desc.positionRadius = 0.16f * scale;
        desc.directionSpread = 0.55f;
        desc.speedMin = 0.45f * scale;
        desc.speedMax = 1.45f * scale;
        desc.lifetimeMin = 0.38f;
        desc.lifetimeMax = 0.82f;
        desc.startSizeMin = 0.20f * scale;
        desc.startSizeMax = 0.42f * scale;
        desc.endSizeMin = 0.06f * scale;
        desc.endSizeMax = 0.18f * scale;
        desc.rotationSpeedMin = -2.2f;
        desc.rotationSpeedMax = 2.2f;
        desc.drag = 0.7f;
        desc.aspectRatio = 0.68f;
        desc.shape = ParticleShape::Flame;
        desc.blendMode = ParticleBlendMode::Additive;
    }
    else
    {
        desc.direction = { 0.0f, 1.0f, 0.0f };
        desc.acceleration = { 0.0f, 0.32f * scale, 0.0f };
        desc.startColor = { 0.19f, 0.21f, 0.22f, 0.50f };
        desc.endColor = { 0.055f, 0.06f, 0.065f, 0.0f };
        desc.particlesPerSecond = 14.0f * scale;
        desc.positionRadius = 0.20f * scale;
        desc.directionSpread = 0.8f;
        desc.speedMin = 0.25f * scale;
        desc.speedMax = 0.75f * scale;
        desc.lifetimeMin = 1.6f;
        desc.lifetimeMax = 3.2f;
        desc.startSizeMin = 0.25f * scale;
        desc.startSizeMax = 0.48f * scale;
        desc.endSizeMin = 1.1f * scale;
        desc.endSizeMax = 1.8f * scale;
        desc.rotationSpeedMin = -0.65f;
        desc.rotationSpeedMax = 0.65f;
        desc.drag = 0.55f;
        desc.aspectRatio = 1.0f;
        desc.shape = ParticleShape::Smoke;
        desc.blendMode = ParticleBlendMode::Alpha;
    }

    return desc;
}

void ParticleSystem::SpawnParticle(const ParticleEmitterDesc& desc, bool debugTrail)
{
    if (m_particles.size() >= MAX_PARTICLES)
        return;

    const XMFLOAT3 normalizedDirection = NormalizeOrUp(desc.direction);
    XMFLOAT3 direction{};

    if (normalizedDirection.x == 0.0f &&
        normalizedDirection.y == 0.0f &&
        normalizedDirection.z == 0.0f)
    {
        direction = RandomUnitVector();
    }
    else
    {
        const XMFLOAT3 randomDirection = RandomUnitVector();
        XMVECTOR mixed = XMVectorAdd(
            XMLoadFloat3(&normalizedDirection),
            XMVectorScale(XMLoadFloat3(&randomDirection), desc.directionSpread));
        if (XMVectorGetX(XMVector3LengthSq(mixed)) < 0.000001f)
            mixed = XMLoadFloat3(&normalizedDirection);
        XMStoreFloat3(&direction, XMVector3Normalize(mixed));
    }

    const XMFLOAT3 positionJitter = RandomUnitVector();
    const float positionDistance = NextRandom01() * desc.positionRadius;
    const float speed = Lerp(desc.speedMin, desc.speedMax, NextRandom01());

    Particle particle;
    particle.position = {
        desc.position.x + positionJitter.x * positionDistance,
        desc.position.y + positionJitter.y * positionDistance,
        desc.position.z + positionJitter.z * positionDistance
    };
    particle.velocity = {
        direction.x * speed,
        direction.y * speed,
        direction.z * speed
    };
    particle.acceleration = desc.acceleration;
    particle.startColor = desc.startColor;
    particle.endColor = desc.endColor;
    particle.lifetime = (std::max)(0.01f, Lerp(
        desc.lifetimeMin,
        desc.lifetimeMax,
        NextRandom01()));
    particle.startSize = (std::max)(0.001f, Lerp(
        desc.startSizeMin,
        desc.startSizeMax,
        NextRandom01()));
    particle.endSize = (std::max)(0.001f, Lerp(
        desc.endSizeMin,
        desc.endSizeMax,
        NextRandom01()));
    particle.aspectRatio = (std::max)(0.05f, desc.aspectRatio);
    particle.rotation = NextRandom01() * XM_2PI;
    particle.rotationSpeed = Lerp(
        desc.rotationSpeedMin,
        desc.rotationSpeedMax,
        NextRandom01());
    particle.drag = (std::max)(0.0f, desc.drag);
    particle.shape = desc.shape;
    particle.blendMode = desc.blendMode;
    if (desc.shape == ParticleShape::Texture && !desc.texturePath.empty())
    {
        particle.texturePath = desc.texturePath;
        particle.atlasColumns = (std::max)(static_cast<uint16_t>(1), desc.atlasColumns);
        particle.atlasRows = (std::max)(static_cast<uint16_t>(1), desc.atlasRows);
        const uint32_t cellCount = static_cast<uint32_t>(particle.atlasColumns) *
            static_cast<uint32_t>(particle.atlasRows);
        particle.atlasFrameCount = static_cast<uint16_t>((std::min)(
            cellCount,
            static_cast<uint32_t>((std::max)(static_cast<uint16_t>(1), desc.atlasFrameCount))));
        particle.atlasFramesPerSecond = (std::max)(0.0f, desc.atlasFramesPerSecond);
        particle.atlasLoop = desc.atlasLoop;
        if (desc.randomStartFrame && particle.atlasFrameCount > 1)
        {
            particle.atlasStartFrame = static_cast<uint16_t>(
                NextRandom01() * static_cast<float>(particle.atlasFrameCount));
            particle.atlasStartFrame = (std::min)(
                particle.atlasStartFrame,
                static_cast<uint16_t>(particle.atlasFrameCount - 1));
        }
    }
    particle.debugTrail = debugTrail;
    m_particles.push_back(particle);
}

void ParticleSystem::EmitDirectedBurst(
    const DamageEvent& damageEvent,
    const ParticleEmitterDesc& prototype,
    int particleCount)
{
    ParticleEmitterDesc desc = prototype;
    desc.position = damageEvent.hit_position;
    desc.direction = damageEvent.hit_normal;

    particleCount = (std::max)(0, particleCount);
    for (int i = 0; i < particleCount && m_particles.size() < MAX_PARTICLES; ++i)
        SpawnParticle(desc, desc.shape == ParticleShape::Spark);
}

void ParticleSystem::RebuildRenderItems()
{
    m_renderItems.clear();
    m_renderItems.reserve(m_particles.size());

    for (const Particle& particle : m_particles)
    {
        const float age01 = Clamp01(particle.age / particle.lifetime);
        ParticleRenderItem item;
        item.position = particle.position;
        item.color = LerpColor(particle.startColor, particle.endColor, age01);
        item.size = Lerp(particle.startSize, particle.endSize, age01);
        item.aspectRatio = particle.aspectRatio;
        item.rotation = particle.rotation;
        item.normalizedAge = age01;
        item.shape = particle.shape;
        item.blendMode = particle.blendMode;
        item.texturePath = particle.texturePath;

        if (!particle.texturePath.empty())
        {
            const uint32_t frameCount = (std::max)(1u,
                static_cast<uint32_t>(particle.atlasFrameCount));
            uint32_t elapsedFrames = 0;
            if (particle.atlasFramesPerSecond > 0.0f)
            {
                elapsedFrames = static_cast<uint32_t>(
                    particle.age * particle.atlasFramesPerSecond);
            }
            else if (frameCount > 1)
            {
                elapsedFrames = (std::min)(
                    frameCount - 1,
                    static_cast<uint32_t>(age01 * static_cast<float>(frameCount)));
            }

            uint32_t frame = static_cast<uint32_t>(particle.atlasStartFrame) + elapsedFrames;
            frame = particle.atlasLoop ? frame % frameCount : (std::min)(frame, frameCount - 1);

            const uint32_t columns = (std::max)(1u,
                static_cast<uint32_t>(particle.atlasColumns));
            const uint32_t row = frame / columns;
            const uint32_t column = frame % columns;
            const float cellWidth = 1.0f / static_cast<float>(columns);
            const float cellHeight = 1.0f / static_cast<float>((std::max)(
                static_cast<uint16_t>(1), particle.atlasRows));
            item.uvMin = {
                static_cast<float>(column) * cellWidth,
                static_cast<float>(row) * cellHeight
            };
            item.uvMax = { item.uvMin.x + cellWidth, item.uvMin.y + cellHeight };
        }
        m_renderItems.push_back(item);
    }
}

float ParticleSystem::NextRandom01()
{
    m_randomState = m_randomState * 1664525u + 1013904223u;
    return static_cast<float>(m_randomState & 0x00FFFFFFu) /
        static_cast<float>(0x01000000u);
}

float ParticleSystem::NextRandomSigned()
{
    return NextRandom01() * 2.0f - 1.0f;
}

XMFLOAT3 ParticleSystem::RandomUnitVector()
{
    XMFLOAT3 result{};
    do
    {
        result = { NextRandomSigned(), NextRandomSigned(), NextRandomSigned() };
    } while (result.x * result.x + result.y * result.y + result.z * result.z < 0.0001f);

    XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&result));
    XMStoreFloat3(&result, vector);
    return result;
}
