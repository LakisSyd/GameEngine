// ============================================================================
// AnimationComponent.cpp - Реализация компонента анимации для Assimp моделей
// ============================================================================

#include "AnimationComponent.h"
#include <algorithm>
#include <DirectXCollision.h>
#include <iostream>

using namespace DirectX;

// ============================================================================
// Конструктор / Деструктор
// ============================================================================
AnimationComponent::AnimationComponent()
    : m_animationTime(0.0f),
      m_maxBones(0)
{
}

AnimationComponent::~AnimationComponent()
{
    Shutdown();
}

// ============================================================================
// Shutdown - Очистка ресурсов
// ============================================================================
void AnimationComponent::Shutdown()
{
    m_boneInfoMap.clear();
    m_animations.clear();
    m_finalBoneMatrices.clear();
    m_animationTime = 0.0f;
    m_maxBones = 0;
}

// ============================================================================
// ConvertMatrix - Конвертация матрицы Assimp в DirectX XMFLOAT4X4
// ============================================================================
XMFLOAT4X4 AnimationComponent::ConvertMatrix(const aiMatrix4x4& from)
{
    XMFLOAT4X4 to;
    
    // Транспонируем при копировании (Assimp использует column-major, DirectX - row-major)
    to._11 = from.a1; to._12 = from.b1; to._13 = from.c1; to._14 = from.d1;
    to._21 = from.a2; to._22 = from.b2; to._23 = from.c2; to._24 = from.d2;
    to._31 = from.a3; to._32 = from.b3; to._33 = from.c3; to._34 = from.d3;
    to._41 = from.a4; to._42 = from.b4; to._43 = from.c4; to._44 = from.d4;
    
    return to;
}

// ============================================================================
// LoadBones - Загрузка информации о костях из сцены Assimp
// ============================================================================
void AnimationComponent::LoadBones(const aiScene* scene)
{
    if (!scene || !scene->mRootNode)
        return;

    m_boneInfoMap.clear();
    int boneCounter = 0;
    m_maxBones = 0;

    // Проходим по всем мешам и собираем информацию о костях
    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[i];
        
        // Проходим по всем костям меша
        for (unsigned int j = 0; j < mesh->mNumBones; j++)
        {
            aiBone* bone = mesh->mBones[j];
            
            // Если кость ещё не добавлена - добавляем её
            if (m_boneInfoMap.find(bone->mName.C_Str()) == m_boneInfoMap.end())
            {
                BoneInfo info;
                info.name = bone->mName.C_Str();
                info.offsetMatrix = ConvertMatrix(bone->mOffsetMatrix);
                info.boneIndex = boneCounter;
                
                m_boneInfoMap[bone->mName.C_Str()] = info;
                boneCounter++;
            }
        }
    }

    m_maxBones = boneCounter;
    m_finalBoneMatrices.resize(m_maxBones);
    
    // Инициализируем матрицы единичными матрицами
    for (int i = 0; i < m_maxBones; i++)
    {
        XMStoreFloat4x4(&m_finalBoneMatrices[i], XMMatrixIdentity());
    }

    std::cout << "[AnimationComponent] Loaded " << m_maxBones << " bones" << std::endl;
}

// ============================================================================
// LoadAnimations - Загрузка анимаций из сцены Assimp
// ============================================================================
void AnimationComponent::LoadAnimations(const aiScene* scene)
{
    if (!scene)
        return;

    m_animations.clear();

    for (unsigned int i = 0; i < scene->mNumAnimations; i++)
    {
        aiAnimation* anim = scene->mAnimations[i];
        
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = static_cast<float>(anim->mDuration);
        clip.ticksPerSecond = anim->mTicksPerSecond > 0 ? 
                              static_cast<float>(anim->mTicksPerSecond) : 25.0f;
        
        // Если ticksPerSecond не задан, предполагаем 25 FPS
        if (clip.ticksPerSecond <= 0.0f)
            clip.ticksPerSecond = 25.0f;

        // Преобразуем каналы анимации
        for (unsigned int j = 0; j < anim->mNumChannels; j++)
        {
            aiNodeAnim* channel = anim->mChannels[j];
            
            BoneChannel boneChannel;
            boneChannel.boneName = channel->mNodeName.C_Str();

            // Копируем ключевые кадры позиции
            for (unsigned int k = 0; k < channel->mNumPositionKeys; k++)
            {
                KeyPosition pos;
                pos.position = XMFLOAT3(
                    channel->mPositionKeys[k].mValue.x,
                    channel->mPositionKeys[k].mValue.y,
                    channel->mPositionKeys[k].mValue.z
                );
                pos.timeStamp = static_cast<float>(channel->mPositionKeys[k].mTime);
                boneChannel.positions.push_back(pos);
            }

            // Копируем ключевые кадры вращения
            for (unsigned int k = 0; k < channel->mNumRotationKeys; k++)
            {
                KeyRotation rot;
                rot.orientation = XMFLOAT4(
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z,
                    channel->mRotationKeys[k].mValue.w
                );
                rot.timeStamp = static_cast<float>(channel->mRotationKeys[k].mTime);
                boneChannel.rotations.push_back(rot);
            }

            // Копируем ключевые кадры масштаба
            for (unsigned int k = 0; k < channel->mNumScalingKeys; k++)
            {
                KeyScale scale;
                scale.scale = XMFLOAT3(
                    channel->mScalingKeys[k].mValue.x,
                    channel->mScalingKeys[k].mValue.y,
                    channel->mScalingKeys[k].mValue.z
                );
                scale.timeStamp = static_cast<float>(channel->mScalingKeys[k].mTime);
                boneChannel.scales.push_back(scale);
            }

            clip.channels.push_back(boneChannel);
        }

        m_animations.push_back(clip);
        std::cout << "[AnimationComponent] Loaded animation: " << clip.name 
                  << " (duration: " << clip.duration << "s)" << std::endl;
    }
}

// ============================================================================
// GetPositionIndex - Поиск индекса ключевого кадра позиции
// ============================================================================
int AnimationComponent::GetPositionIndex(float animationTime, 
                                         const std::vector<KeyPosition>& positions)
{
    for (int i = 0; i < static_cast<int>(positions.size()) - 1; i++)
    {
        if (animationTime < positions[i + 1].timeStamp)
            return i;
    }
    return 0;
}

// ============================================================================
// GetRotationIndex - Поиск индекса ключевого кадра вращения
// ============================================================================
int AnimationComponent::GetRotationIndex(float animationTime, 
                                         const std::vector<KeyRotation>& rotations)
{
    for (int i = 0; i < static_cast<int>(rotations.size()) - 1; i++)
    {
        if (animationTime < rotations[i + 1].timeStamp)
            return i;
    }
    return 0;
}

// ============================================================================
// GetScaleIndex - Поиск индекса ключевого кадра масштаба
// ============================================================================
int AnimationComponent::GetScaleIndex(float animationTime, 
                                      const std::vector<KeyScale>& scales)
{
    for (int i = 0; i < static_cast<int>(scales.size()) - 1; i++)
    {
        if (animationTime < scales[i + 1].timeStamp)
            return i;
    }
    return 0;
}

// ============================================================================
// InterpolatePosition - Линейная интерполяция позиции между ключевыми кадрами
// ============================================================================
XMFLOAT3 AnimationComponent::InterpolatePosition(float animationTime, 
                                                  const std::vector<KeyPosition>& positions)
{
    if (positions.empty())
        return XMFLOAT3(0, 0, 0);
    
    if (positions.size() == 1)
        return positions[0].position;

    int p0Index = GetPositionIndex(animationTime, positions);
    int p1Index = p0Index + 1;
    
    if (p1Index >= static_cast<int>(positions.size()))
        return positions[p0Index].position;

    float scaleFactor = positions[p1Index].timeStamp - positions[p0Index].timeStamp;
    if (scaleFactor <= 0.0f)
        return positions[p0Index].position;
        
    float betweenFactor = (animationTime - positions[p0Index].timeStamp) / scaleFactor;
    
    XMVECTOR p0 = XMLoadFloat3(&positions[p0Index].position);
    XMVECTOR p1 = XMLoadFloat3(&positions[p1Index].position);
    
    XMVECTOR result = XMVectorLerp(p0, p1, betweenFactor);
    
    XMFLOAT3 out;
    XMStoreFloat3(&out, result);
    return out;
}

// ============================================================================
// InterpolateRotation - Сферическая интерполяция вращения (SLERP)
// ============================================================================
XMFLOAT4 AnimationComponent::InterpolateRotation(float animationTime, 
                                                  const std::vector<KeyRotation>& rotations)
{
    if (rotations.empty())
        return XMFLOAT4(0, 0, 0, 1);
    
    if (rotations.size() == 1)
        return rotations[0].orientation;

    int r0Index = GetRotationIndex(animationTime, rotations);
    int r1Index = r0Index + 1;
    
    if (r1Index >= static_cast<int>(rotations.size()))
        return rotations[r0Index].orientation;

    float scaleFactor = rotations[r1Index].timeStamp - rotations[r0Index].timeStamp;
    if (scaleFactor <= 0.0f)
        return rotations[r0Index].orientation;
        
    float betweenFactor = (animationTime - rotations[r0Index].timeStamp) / scaleFactor;
    
    XMVECTOR r0 = XMLoadFloat4(&rotations[r0Index].orientation);
    XMVECTOR r1 = XMLoadFloat4(&rotations[r1Index].orientation);
    
    // Нормализуем кватернионы
    r0 = XMQuaternionNormalize(r0);
    r1 = XMQuaternionNormalize(r1);
    
    XMVECTOR result = XMQuaternionSlerp(r0, r1, betweenFactor);
    result = XMQuaternionNormalize(result);
    
    XMFLOAT4 out;
    XMStoreFloat4(&out, result);
    return out;
}

// ============================================================================
// InterpolateScale - Линейная интерполяция масштаба
// ============================================================================
XMFLOAT3 AnimationComponent::InterpolateScale(float animationTime, 
                                               const std::vector<KeyScale>& scales)
{
    if (scales.empty())
        return XMFLOAT3(1, 1, 1);
    
    if (scales.size() == 1)
        return scales[0].scale;

    int s0Index = GetScaleIndex(animationTime, scales);
    int s1Index = s0Index + 1;
    
    if (s1Index >= static_cast<int>(scales.size()))
        return scales[s0Index].scale;

    float scaleFactor = scales[s1Index].timeStamp - scales[s0Index].timeStamp;
    if (scaleFactor <= 0.0f)
        return scales[s0Index].scale;
        
    float betweenFactor = (animationTime - scales[s0Index].timeStamp) / scaleFactor;
    
    XMVECTOR s0 = XMLoadFloat3(&scales[s0Index].scale);
    XMVECTOR s1 = XMLoadFloat3(&scales[s1Index].scale);
    
    XMVECTOR result = XMVectorLerp(s0, s1, betweenFactor);
    
    XMFLOAT3 out;
    XMStoreFloat3(&out, result);
    return out;
}

// ============================================================================
// ReadNodeHierarchy - Рекурсивное построение иерархии трансформаций костей
// ============================================================================
void AnimationComponent::ReadNodeHierarchy(const aiNode* node, 
                                           const XMFLOAT4X4& parentTransform,
                                           const AnimationClip& anim, 
                                           float animationTime)
{
    // Находим трансформацию узла из анимации или берём из сцены
    XMFLOAT4X4 nodeTransformation = ConvertMatrix(node->mTransformation);
    
    // Ищем канал анимации для этого узла
    for (const auto& channel : anim.channels)
    {
        if (channel.boneName == node->mName.C_Str())
        {
            // Интерполируем трансформации из ключевых кадров
            XMFLOAT3 scaledPos = InterpolatePosition(animationTime, channel.positions);
            XMFLOAT4 scaledRot = InterpolateRotation(animationTime, channel.rotations);
            XMFLOAT3 scaledScale = InterpolateScale(animationTime, channel.scales);
            
            // Строим матрицу трансформации
            XMMATRIX translation = XMMatrixTranslationFromVector(XMLoadFloat3(&scaledPos));
            XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&scaledRot));
            XMMATRIX scaling = XMMatrixScalingFromVector(XMLoadFloat3(&scaledScale));
            
            XMMATRIX nodeTransform = scaling * rotation * translation;
            XMStoreFloat4x4(&nodeTransformation, nodeTransform);
            break;
        }
    }
    
    // Глобальная трансформация = родительская * локальная
    XMMATRIX globalTransformation = XMLoadFloat4x4(&parentTransform) * 
                                    XMLoadFloat4x4(&nodeTransformation);
    
    // Если этот узел - кость, вычисляем финальную матрицу
    std::string nodeName = node->mName.C_Str();
    if (m_boneInfoMap.find(nodeName) != m_boneInfoMap.end())
    {
        BoneInfo& boneInfo = m_boneInfoMap[nodeName];
        
        // Финальная матрица = GlobalTransform * OffsetMatrix
        XMMATRIX finalTransform = globalTransformation * 
                                  XMLoadFloat4x4(&boneInfo.offsetMatrix);
        
        XMStoreFloat4x4(&m_finalBoneMatrices[boneInfo.boneIndex], finalTransform);
    }
    
    // Рекурсивно обрабатываем дочерние узлы
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        XMFLOAT4X4 currentTransform;
        XMStoreFloat4x4(&currentTransform, globalTransformation);
        ReadNodeHierarchy(node->mChildren[i], currentTransform, anim, animationTime);
    }
}

// ============================================================================
// UpdateAnimation - Обновление состояния анимации
// ============================================================================
void AnimationComponent::UpdateAnimation(float deltaTime, int animIndex)
{
    if (m_animations.empty() || animIndex >= static_cast<int>(m_animations.size()))
        return;
    
    const AnimationClip& currentAnim = m_animations[animIndex];
    
    // Обновляем время анимации
    m_animationTime += deltaTime * currentAnim.ticksPerSecond;
    
    // Зацикливаем анимацию
    if (m_animationTime >= currentAnim.duration)
    {
        m_animationTime = fmod(m_animationTime, currentAnim.duration);
    }
    
    // Сбрасываем матрицы в единичные
    for (int i = 0; i < m_maxBones; i++)
    {
        XMStoreFloat4x4(&m_finalBoneMatrices[i], XMMatrixIdentity());
    }
    
    // Начинаем обход иерархии с корневого узла
    // Для простоты используем единичную матрицу как корневую трансформацию
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    
    // Примечание: здесь нужен доступ к корневому узлу сцены
    // В полной реализации нужно передать aiScene* или сохранить ссылку на корень
    // Для базовой версии предполагаем что вызывается из AssimpModelClass
}

// ============================================================================
// SetAnimationTime - Установка времени анимации вручную
// ============================================================================
void AnimationComponent::SetAnimationTime(float time)
{
    if (!m_animations.empty())
    {
        const AnimationClip& currentAnim = m_animations[0];
        m_animationTime = fmod(time, currentAnim.duration);
    }
    else
    {
        m_animationTime = time;
    }
}
