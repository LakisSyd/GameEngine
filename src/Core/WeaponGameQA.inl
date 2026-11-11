// Opt-in end-to-end regression playback: same input, physics, animation and renderer as Play.
#include <fstream>
#include <filesystem>
#include <Json/json.hpp>
#include "Animation/WeaponIKSystem.h"

void ApplicationClass::UpdateWeaponQAInput()
{
    if (!m_input) return;
    if (m_weaponQAFrame == 0) m_input->KeyDown(VK_F5);
    if (m_weaponQAFrame == 1) m_input->KeyUp(VK_F5);
    if (!m_weaponQA) return;
    const int f = m_weaponQAFrame;
    for (unsigned int key : std::array<unsigned int,9>{
        'W', 'S', 'A', 'D', '1', '2', '3', VK_SHIFT, VK_ESCAPE })
        m_input->KeyUp(key);
    m_input->MouseLeftUp();
    // Escape must reach Play interaction input without closing the application.
    if (f == 300) m_input->KeyDown(VK_ESCAPE);
    if ((f >= 80 && f < 280) || (f >= 560 && f < 620)) m_input->KeyDown('W');
    if (f >= 180 && f < 280) m_input->KeyDown(VK_SHIFT);
    if (f == 10 || f == 380 || f == 680) m_input->KeyDown('1');
    if (f == 320 || f == 640) m_input->KeyDown('3');
    if (f == 500) m_input->KeyDown('2');
    if ((f >= 120 && f < 140) || (f >= 200 && f < 220) ||
        (f >= 440 && f < 480) ||
        (f >= 560 && f < 620)) m_input->MouseLeftDown();
}

void ApplicationClass::CaptureWeaponQAFrame()
{
    namespace fs = std::filesystem;
    using nlohmann::json;
    fs::create_directories("tools/qa/weapon_game");
    auto& registry = m_sceneManager.GetECSRegistry();
    const Entity player = m_gameplayController.GetPlayerEntity();
    const auto* animation = registry.GetComponent<AnimationComponent>(player);
    const auto* mesh = registry.GetComponent<MeshRendererComponent>(player);
    const auto* transform = registry.GetComponent<TransformComponent>(player);
    const auto* movement = registry.GetComponent<CharacterMovement>(player);
    const Entity weaponEntity = WeaponSystem::GetEquippedWeaponEntity(registry, player);
    auto* weapon = registry.GetComponent<WeaponComponent>(weaponEntity);
    if (m_weaponQAFrame == 0) std::ofstream("tools/qa/weapon_game/frames.jsonl", std::ios::trunc);
    if (animation && transform && mesh && mesh->model)
    {
        const auto* base = mesh->model->GetAnimation(animation->animationIndex);
        const auto* upper = mesh->model->GetAnimation(animation->upperBodyAnimationIndex);
        json row = {{"frame",m_weaponQAFrame}, {"base",base ? base->name : ""},
            {"upper",upper ? upper->name : ""}, {"time",animation->currentTime},
            {"upper_root",animation->upperBodyRoot},
            {"upper_channels",upper ? upper->channels.size() : 0},
            {"upper_duration",upper ? upper->duration : 0.0f},
            {"ik",animation->leftHandIKApplied}, {"ik_error",animation->leftHandIKError},
            {"position",{transform->position.x,transform->position.y,transform->position.z}},
            {"speed",movement ? movement->currentSpeed : 0},
            {"shots",weapon ? weapon->shotsFired : 0}, {"weapon",weaponEntity}};
        if (m_weaponQAFrame == 0 && weapon)
        {
            auto* camera = registry.GetComponent<PlayCameraComponent>(player);
            FirstPersonViewComponent* arms = nullptr;
            registry.ForEach<FirstPersonViewComponent>(
                [&](Entity, FirstPersonViewComponent& view)
                {
                    if (view.owner == player && view.followCamera)
                        arms = &view;
                });
            if (camera && arms)
            {
                const WeaponComponent savedWeapon = *weapon;
                const PlayCameraComponent savedCamera = *camera;
                const FirstPersonViewComponent savedArms = *arms;
                weapon->worldGrip.position = { 0.11f, 0.22f, 0.33f };
                weapon->leftHandGrip = { 0.12f, 0.23f, 0.34f };
                weapon->enableLeftHandIK = false;
                weapon->firstPersonGrip.position = { 0.44f, 0.55f, 0.66f };
                weapon->firstPersonGrip.pivot = { 0.14f, 0.15f, 0.16f };
                weapon->firstPersonLeftHandGrip = { 0.45f, 0.56f, 0.67f };
                weapon->firstPersonLeftHandRotation = { 0.17f, 0.18f, 0.19f };
                weapon->enableFirstPersonLeftHandIK = true;
                weapon->firstPersonPitchScale = 0.83f;
                weapon->firstPersonPitchOffset = 0.24f;
                camera->thirdPersonDistance = 7.25f;
                camera->thirdPersonTargetOffset = { 0.21f, 1.73f, 0.31f };
                camera->firstPersonEyeOffset = { 0.09f, 1.65f, 0.27f };
                arms->cameraOffset = { 0.03f, -1.41f, 0.19f };
                arms->lookUpLimit = 55.0f;
                arms->lookDownLimit = 61.0f;
                const std::string path =
                    "tools/qa/weapon_game/ProfileRoundtrip.json";
                const bool saved = m_sceneManager.SaveWeaponProfile(*weapon, path);
                weapon->worldGrip.position = {};
                weapon->leftHandGrip = {};
                weapon->enableLeftHandIK = true;
                weapon->firstPersonGrip.position = {};
                weapon->firstPersonGrip.pivot = {};
                weapon->firstPersonLeftHandGrip = {};
                weapon->firstPersonLeftHandRotation = {};
                weapon->enableFirstPersonLeftHandIK = false;
                weapon->firstPersonPitchScale = 0.0f;
                weapon->firstPersonPitchOffset = 0.0f;
                *camera = PlayCameraComponent{};
                arms->cameraOffset = {};
                arms->lookUpLimit = 0.0f;
                arms->lookDownLimit = 0.0f;
                const bool loaded = saved &&
                    m_sceneManager.LoadWeaponProfile(*weapon, path);
                const auto approximatelyEqual = [](float a, float b)
                {
                    return std::fabs(a - b) < 0.0001f;
                };
                row["profile_roundtrip_ok"] = loaded &&
                    approximatelyEqual(weapon->worldGrip.position.z, 0.33f) &&
                    approximatelyEqual(weapon->leftHandGrip.y, 0.23f) &&
                    !weapon->enableLeftHandIK &&
                    approximatelyEqual(weapon->firstPersonGrip.position.x, 0.44f) &&
                    approximatelyEqual(weapon->firstPersonGrip.pivot.y, 0.15f) &&
                    approximatelyEqual(weapon->firstPersonLeftHandGrip.z, 0.67f) &&
                    approximatelyEqual(weapon->firstPersonLeftHandRotation.x, 0.17f) &&
                    weapon->enableFirstPersonLeftHandIK &&
                    approximatelyEqual(weapon->firstPersonPitchScale, 0.83f) &&
                    approximatelyEqual(weapon->firstPersonPitchOffset, 0.24f) &&
                    approximatelyEqual(camera->thirdPersonDistance, 7.25f) &&
                    approximatelyEqual(camera->thirdPersonTargetOffset.y, 1.73f) &&
                    approximatelyEqual(camera->firstPersonEyeOffset.z, 0.27f) &&
                    approximatelyEqual(arms->cameraOffset.y, -1.41f) &&
                    approximatelyEqual(arms->lookUpLimit, 55.0f) &&
                    approximatelyEqual(arms->lookDownLimit, 61.0f);
                *weapon = savedWeapon;
                *camera = savedCamera;
                *arms = savedArms;
            }
        }
        registry.ForEach<DamageableComponent>([&](Entity e, DamageableComponent& health) {
            row["targets"][std::to_string(e)] = health.health;
        });
        XMMATRIX hand;
        if (m_weaponQAFrame == 0 && upper)
        {
            AnimationComponent standalone = *animation;
            standalone.animationIndex = animation->upperBodyAnimationIndex;
            standalone.upperBodyAnimationIndex = -1;
            standalone.currentTime = 0.0f;
            mesh->model->UpdateAnimation(standalone, 0.0f);
            XMMATRIX upperHand;
            if (mesh->model->TryGetBoneModelTransform(
                standalone, "mixamorig:RightHand", upperHand))
            {
                XMFLOAT4X4 upperHandMatrix;
                XMStoreFloat4x4(&upperHandMatrix, upperHand);
                row["standalone_upper_right_hand"] = {
                    upperHandMatrix._41, upperHandMatrix._42, upperHandMatrix._43};
            }
        }
        if (mesh->model->TryGetBoneModelTransform(*animation,"mixamorig:RightHand",hand)) {
            XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,hand);
            row["right_hand"] = {matrix._41,matrix._42,matrix._43};
            XMMATRIX left;
            if (weapon && mesh->model->TryGetBoneModelTransform(
                *animation, weapon->leftHandBone, left)) {
                XMFLOAT4X4 leftMatrix;XMStoreFloat4x4(&leftMatrix,left);
                row["left_hand"] = {leftMatrix._41,leftMatrix._42,leftMatrix._43};
                const XMMATRIX weaponModel =
                    WeaponIKSystem::GripMatrix(weapon->worldGrip) * hand;
                const XMMATRIX local = left * XMMatrixInverse(nullptr,weaponModel);
                XMVECTOR s,q,p;
                if (XMMatrixDecompose(&s,&q,&p,local)) {
                    XMFLOAT3 pos;XMFLOAT4 quat;
                    XMStoreFloat3(&pos,p);XMStoreFloat4(&quat,q);
                    row["left_in_weapon"] = {pos.x,pos.y,pos.z};
                    row["left_rotation_in_weapon_quat"] =
                        {quat.x,quat.y,quat.z,quat.w};
                }
            }
        }
        std::ofstream("tools/qa/weapon_game/frames.jsonl",std::ios::app) << row.dump() << '\n';
    }
    const int frame = m_weaponQAFrame;
    if (frame != 60 && frame != 140 && frame != 240 && frame != 350 && frame != 410 && frame != 460 && frame != 540 && frame != 600 && frame != 710) return;
    ID3D11Texture2D* buffer = nullptr;
    if (FAILED(m_direct3D->GetSwapChain()->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void**>(&buffer)))) return;
    D3D11_TEXTURE2D_DESC desc;buffer->GetDesc(&desc);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
    ID3D11Texture2D* staging=nullptr;
    if (FAILED(m_direct3D->GetDevice()->CreateTexture2D(&desc,nullptr,&staging))) { buffer->Release();return; }
    auto* context=m_direct3D->GetDeviceContext();context->CopyResource(staging,buffer);buffer->Release();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped))) {
        BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
        header.bfType=0x4D42;header.bfOffBits=sizeof(header)+sizeof(info);
        header.bfSize=header.bfOffBits+desc.Width*desc.Height*4;
        info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);
        info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
        std::ofstream output("tools/qa/weapon_game/frame_"+std::to_string(frame)+".bmp",std::ios::binary);
        output.write(reinterpret_cast<char*>(&header),sizeof(header));output.write(reinterpret_cast<char*>(&info),sizeof(info));
        std::vector<unsigned char> pixels(desc.Width*4);
        for (UINT y=0;y<desc.Height;++y) {
            memcpy(pixels.data(),static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,pixels.size());
            if (desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
                for (UINT x=0;x<desc.Width;++x) std::swap(pixels[x*4],pixels[x*4+2]);
            output.write(reinterpret_cast<char*>(pixels.data()),pixels.size());
        }
        context->Unmap(staging,0);
    }
    staging->Release();
}

