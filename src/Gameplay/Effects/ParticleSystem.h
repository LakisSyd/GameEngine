#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include <DamageEvent.h>

class PhysicsDebugRenderer;
class ParticleRenderer;
struct ID3D11Device;
struct ID3D11DeviceContext;

enum class ParticleEffectType : uint8_t
{
    Fire,
    Smoke,
    Explosion
};

enum class ParticleShape : uint8_t
{
    SoftDisc,
    Flame,
    Smoke,
    Spark,
    Shockwave,
    Texture
};

enum class ParticleBlendMode : uint8_t
{
    Alpha,
    Additive
};

using ParticleEmitterId = uint32_t;
constexpr ParticleEmitterId INVALID_PARTICLE_EMITTER = 0;

// Полное описание эмиттера. Для нового эффекта обычно достаточно скопировать
// один из пресетов в ParticleSystem.cpp и изменить эти значения.
struct ParticleEmitterDesc
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 direction = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 acceleration = { 0.0f, 0.0f, 0.0f };

    DirectX::XMFLOAT4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };

    float particlesPerSecond = 20.0f;
    float positionRadius = 0.05f;
    float directionSpread = 0.25f;
    float speedMin = 0.5f;
    float speedMax = 1.0f;
    float lifetimeMin = 0.5f;
    float lifetimeMax = 1.0f;
    float startSizeMin = 0.10f;
    float startSizeMax = 0.20f;
    float endSizeMin = 0.20f;
    float endSizeMax = 0.40f;
    float rotationSpeedMin = -1.0f;
    float rotationSpeedMax = 1.0f;
    float drag = 0.0f;
    float aspectRatio = 1.0f;

    // Для струи из дула длинная ось следует стволу, а не случайному углу билборда.
    bool alignToDirection = false;

    // If shape == Texture, this image is sampled by the pixel shader. PNG,
    // JPG and TGA are supported by TextureClass. PNG/TGA are recommended
    // because their alpha channel gives the particle a non-rectangular shape.
    std::string texturePath;
    uint16_t atlasColumns = 1;
    uint16_t atlasRows = 1;
    uint16_t atlasFrameCount = 1;
    float atlasFramesPerSecond = 0.0f;
    bool atlasLoop = true;
    bool randomStartFrame = false;

    ParticleShape shape = ParticleShape::SoftDisc;
    ParticleBlendMode blendMode = ParticleBlendMode::Alpha;
    bool emitImmediately = true;
};

// Это уже подготовленные данные для рендера. Симуляция не знает о Direct3D,
// поэтому при желании ParticleRenderer можно заменить, не меняя игровую логику.
struct ParticleRenderItem
{
    DirectX::XMFLOAT3 axis = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float size = 0.1f;
    float aspectRatio = 1.0f;
    float rotation = 0.0f;
    float normalizedAge = 0.0f;
    ParticleShape shape = ParticleShape::SoftDisc;
    ParticleBlendMode blendMode = ParticleBlendMode::Alpha;
    std::string texturePath;
    DirectX::XMFLOAT2 uvMin = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 uvMax = { 1.0f, 1.0f };
};

class ParticleSystem
{
public:
    static constexpr size_t MAX_PARTICLES = 4096;
    static constexpr size_t MAX_EMITTERS = 128;

    ParticleSystem();
    ~ParticleSystem();

    // В приложении сейчас один игровой мир. Renderer использует этот указатель,
    // чтобы забрать готовую очередь без зависимости ApplicationClass от рендера.
    static ParticleSystem* GetActiveSystem() { return s_activeSystem; }

    // Универсальный API для длительных источников. ID нужно сохранить у объекта,
    // чтобы затем двигать, выключать или удалять его вместе с объектом.
    ParticleEmitterId CreateEmitter(const ParticleEmitterDesc& desc);
    ParticleEmitterId CreateEmitter(
        ParticleEffectType effect,
        const DirectX::XMFLOAT3& position,
        float scale = 1.0f);

    bool SetEmitterPosition(ParticleEmitterId id, const DirectX::XMFLOAT3& position);
    bool SetEmitterActive(ParticleEmitterId id, bool active);
    bool UpdateEmitter(ParticleEmitterId id, const ParticleEmitterDesc& desc);
    void DestroyEmitter(ParticleEmitterId id);

    // Универсальный разовый выброс и готовый многослойный взрыв.
    void EmitBurst(const ParticleEmitterDesc& desc, int particleCount);
    void EmitExplosion(const DirectX::XMFLOAT3& position, float scale = 1.0f);
    void EmitMuzzleFlash(const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& direction, float scale = 1.0f);
    void ReloadTextures();

    // Совместимость с существующей системой повреждений корпуса.
    void EmitMetalImpact(const DamageEvent& damageEvent);
    void EmitHullDent(const DamageEvent& damageEvent);
    void EmitHullCrack(const DamageEvent& damageEvent);
    void EmitHullBreach(const DamageEvent& damageEvent);

    void Update(float deltaTime, PhysicsDebugRenderer* debugRenderer = nullptr);
    void Render(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& cameraPosition,
        float fallbackDeltaTime);
    void Clear();

    const std::vector<ParticleRenderItem>& GetRenderItems() const { return m_renderItems; }
    size_t GetAliveParticleCount() const { return m_particles.size(); }
    size_t GetEmitterCount() const { return m_emitters.size(); }

private:
    struct Particle
    {
        DirectX::XMFLOAT3 axis = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT3 velocity{};
        DirectX::XMFLOAT3 acceleration{};
        DirectX::XMFLOAT4 startColor{};
        DirectX::XMFLOAT4 endColor{};
        float age = 0.0f;
        float lifetime = 1.0f;
        float startSize = 0.1f;
        float endSize = 0.1f;
        float aspectRatio = 1.0f;
        float rotation = 0.0f;
        float rotationSpeed = 0.0f;
        float drag = 0.0f;
        ParticleShape shape = ParticleShape::SoftDisc;
        ParticleBlendMode blendMode = ParticleBlendMode::Alpha;
        std::string texturePath;
        uint16_t atlasColumns = 1;
        uint16_t atlasRows = 1;
        uint16_t atlasFrameCount = 1;
        uint16_t atlasStartFrame = 0;
        float atlasFramesPerSecond = 0.0f;
        bool atlasLoop = true;
        bool debugTrail = false;
    };

    struct Emitter
    {
        ParticleEmitterId id = INVALID_PARTICLE_EMITTER;
        ParticleEmitterDesc desc{};
        float spawnAccumulator = 0.0f;
        bool active = true;
    };

    struct CrackLine
    {
        DirectX::XMFLOAT3 start{};
        DirectX::XMFLOAT3 end{};
        float age = 0.0f;
        float lifetime = 2.5f;
    };

    static ParticleEmitterDesc MakePreset(
        ParticleEffectType effect,
        const DirectX::XMFLOAT3& position,
        float scale);

    void SpawnParticle(const ParticleEmitterDesc& desc, bool debugTrail = false);
    void EmitDirectedBurst(
        const DamageEvent& damageEvent,
        const ParticleEmitterDesc& prototype,
        int particleCount);
    void RebuildRenderItems();

    float NextRandom01();
    float NextRandomSigned();
    DirectX::XMFLOAT3 RandomUnitVector();

    std::vector<Particle> m_particles;
    std::vector<Emitter> m_emitters;
    std::vector<ParticleRenderItem> m_renderItems;
    std::vector<CrackLine> m_cracks;
    ParticleEmitterId m_nextEmitterId = 1;
    uint32_t m_randomState = 0xA531u;
    static ParticleSystem* s_activeSystem;
    std::unique_ptr<ParticleRenderer> m_renderer;
    bool m_rendererInitializationAttempted = false;
    bool m_updatedSinceLastRender = false;
};
