#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <assimp/scene.h>

using namespace DirectX;

// ============================================================================
// Компонент анимации для работы с Assimp моделями
// Базовый функционал: загрузка, обновление, интерполяция трансформаций костей
// ============================================================================

class AnimationComponent
{
public:
    // ── Ключевой кадр позиции ────────────────────────────────────────────────
    struct KeyPosition
    {
        XMFLOAT3 position;
        float timeStamp;
    };

    // ── Ключевой кадр вращения (кватернион) ──────────────────────────────────
    struct KeyRotation
    {
        XMFLOAT4 orientation; // Кватернион (x, y, z, w)
        float timeStamp;
    };

    // ── Ключевой кадр масштаба ───────────────────────────────────────────────
    struct KeyScale
    {
        XMFLOAT3 scale;
        float timeStamp;
    };

    // ── Канал анимации для одной кости ───────────────────────────────────────
    struct BoneChannel
    {
        std::string boneName;
        std::vector<KeyPosition> positions;
        std::vector<KeyRotation> rotations;
        std::vector<KeyScale> scales;
    };

    // ── Клип анимации ────────────────────────────────────────────────────────
    struct AnimationClip
    {
        std::string name;
        float duration;           // Длительность в секундах
        float ticksPerSecond;     // Тиков в секунду
        std::vector<BoneChannel> channels;
    };

    // ── Информация о кости ───────────────────────────────────────────────────
    struct BoneInfo
    {
        std::string name;
        XMFLOAT4X4 offsetMatrix;  // Матрица смещения кости (Bone Offset)
        int boneIndex;            // Индекс в массиве матриц для шейдера
    };

public:
    AnimationComponent();
    ~AnimationComponent();

    // ── Инициализация ────────────────────────────────────────────────────────
    void LoadBones(const aiScene* scene);
    void LoadAnimations(const aiScene* scene);
    void Shutdown();

    // ── Управление анимацией ─────────────────────────────────────────────────
    void UpdateAnimation(float deltaTime, int animIndex = 0);
    void SetAnimationTime(float time);
    float GetAnimationTime() const { return m_animationTime; }
    
    // ── Получение данных ─────────────────────────────────────────────────────
    bool HasAnimations() const { return !m_animations.empty(); }
    int GetAnimationCount() const { return static_cast<int>(m_animations.size()); }
    
    const std::vector<XMFLOAT4X4>& GetFinalBoneMatrices() const { return m_finalBoneMatrices; }
    const std::unordered_map<std::string, BoneInfo>& GetBoneInfoMap() const { return m_boneInfoMap; }
    
    // ── Вспомогательные методы ───────────────────────────────────────────────
    XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& from);
    
private:
    // ── Интерполяция ─────────────────────────────────────────────────────────
    XMFLOAT3 InterpolatePosition(float animationTime, const std::vector<KeyPosition>& positions);
    XMFLOAT4 InterpolateRotation(float animationTime, const std::vector<KeyRotation>& rotations);
    XMFLOAT3 InterpolateScale(float animationTime, const std::vector<KeyScale>& scales);
    
    // ── Построение иерархии ──────────────────────────────────────────────────
    void ReadNodeHierarchy(const aiNode* node, const XMFLOAT4X4& parentTransform, 
                          const AnimationClip& anim, float animationTime);
    
    // ── Поиск ключей ─────────────────────────────────────────────────────────
    int GetPositionIndex(float animationTime, const std::vector<KeyPosition>& positions);
    int GetRotationIndex(float animationTime, const std::vector<KeyRotation>& rotations);
    int GetScaleIndex(float animationTime, const std::vector<KeyScale>& scales);

private:
    std::unordered_map<std::string, BoneInfo> m_boneInfoMap;  // Информация о костях
    std::vector<AnimationClip> m_animations;                   // Загруженные анимации
    std::vector<XMFLOAT4X4> m_finalBoneMatrices;               // Финальные матрицы для шейдера
    
    float m_animationTime;                                      // Текущее время анимации
    int m_maxBones;                                             // Максимальное количество костей
};
