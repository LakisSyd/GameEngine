#include "ObjectDebugWindow.h"

#include <algorithm>

#include <imgui.h>

#include "SceneManager.h"

namespace EditorDebug
{
static constexpr ImVec4 COL_ACCENT = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
static constexpr ImVec4 COL_RED = ImVec4(0.75f, 0.18f, 0.18f, 1.0f);
static constexpr ImVec4 COL_TEXT_DIM = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);

static constexpr float BASE_WIDTH = 1920.0f;
static constexpr float BASE_HEIGHT = 1280.0f;

static float DebugScaleX() { return ImGui::GetIO().DisplaySize.x / BASE_WIDTH; }
static float DebugScaleY() { return ImGui::GetIO().DisplaySize.y / BASE_HEIGHT; }
static float DebugScale() { return std::min(DebugScaleX(), DebugScaleY()); }

static const char* ShaderTypeName(ShaderType type)
{
    switch (type)
    {
    case ShaderType::Color: return "Color";
    case ShaderType::Texture: return "Texture";
    case ShaderType::Light: return "Light";
    case ShaderType::LightMap: return "LightMap";
    case ShaderType::MultiTexture: return "MultiTexture";
    case ShaderType::NormalMap: return "NormalMap";
    case ShaderType::Fog: return "Fog";
    case ShaderType::ClipPlane: return "ClipPlane";
    case ShaderType::Reflection: return "Reflection";
    case ShaderType::Water: return "Water";
    case ShaderType::Glass: return "Glass";
    case ShaderType::Projection: return "Projection";
    case ShaderType::PBR: return "PBR";
    default: return "Unknown";
    }
}

static const char* RigidBodyShapeName(RigidBodyComponent::ShapeType type)
{
    switch (type)
    {
    case RigidBodyComponent::Box: return "Box";
    case RigidBodyComponent::Sphere: return "Sphere";
    case RigidBodyComponent::Cylinder: return "Cylinder";
    case RigidBodyComponent::Capsule: return "Capsule";
    case RigidBodyComponent::Plane: return "Plane";
    case RigidBodyComponent::ConvexHull: return "ConvexHull";
    case RigidBodyComponent::TriangleMesh: return "TriangleMesh";
    case RigidBodyComponent::GImpactMesh: return "GImpactMesh";
    case RigidBodyComponent::VHACD: return "VHACD";
    default: return "Unknown";
    }
}

static void DebugBoolLine(const char* label, bool value)
{
    ImGui::Text("%s: %s", label, value ? "true" : "false");
}

static void DebugVec3Line(const char* label, const XMFLOAT3& value)
{
    ImGui::Text("%s: %.3f, %.3f, %.3f", label, value.x, value.y, value.z);
}

static void DebugVec4Line(const char* label, const XMFLOAT4& value)
{
    ImGui::Text("%s: %.3f, %.3f, %.3f, %.3f", label, value.x, value.y, value.z, value.w);
}

static void RenderSelectedObjectDebugInfo(SceneManager& sceneManager, Entity entity);

void RenderObjectDebugWindow(SceneManager& sceneManager, Entity selectedEntity, bool& showWindow)
{
    if (!showWindow) return;

    ImGui::SetNextWindowSize(ImVec2(520.0f * DebugScale(), 680.0f * DebugScale()), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(24.0f * DebugScaleX(), 80.0f * DebugScaleY()), ImGuiCond_FirstUseEver);

    bool open = showWindow;
    if (!ImGui::Begin("Object Debug (F1)", &open, ImGuiWindowFlags_NoCollapse))
    {
        showWindow = open;
        ImGui::End();
        return;
    }

    showWindow = open;

    ImGui::TextColored(COL_ACCENT, "F1 toggles this window in Editor and Play Mode");
    ImGui::Separator();

    Entity debugEntity = selectedEntity;
    if (debugEntity == INVALID_ENTITY)
    {
        auto& registry = sceneManager.GetECSRegistry();
        registry.ForEach<PlayerGameTag>([&](Entity entity, PlayerGameTag&) {
            if (debugEntity == INVALID_ENTITY) debugEntity = entity;
            });
    }

    if (debugEntity == INVALID_ENTITY)
    {
        ImGui::TextColored(COL_TEXT_DIM, "No selected entity.");
        ImGui::TextWrapped("Select an object in the editor, then press F1. In Play Mode the window keeps showing the selected object; if nothing is selected it tries to show PlayerGameTag.");
        ImGui::End();
        return;
    }

    RenderSelectedObjectDebugInfo(sceneManager, debugEntity);
    ImGui::End();
}

static void RenderSelectedObjectDebugInfo(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();

    const bool hasTransform = registry.HasComponent<TransformComponent>(entity);
    const bool hasMesh = registry.HasComponent<MeshRendererComponent>(entity);
    const bool hasLight = registry.HasComponent<LightComponent>(entity);
    const bool hasActive = registry.HasComponent<ActiveComponent>(entity);

    const char* typeName = hasLight ? "Light" : (hasMesh ? "Object" : "Entity");
    ImGui::Text("Entity: %u", entity);
    ImGui::Text("Type: %s", typeName);
    DebugBoolLine("Active", hasActive);

    if (auto* name = registry.GetComponent<NameComponent>(entity))
        ImGui::Text("Name: %s", name->name.c_str());

    ImGui::Spacing();
    ImGui::SeparatorText("Components");
    ImGui::TextWrapped(
        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
        hasTransform ? "Transform " : "",
        hasMesh ? "MeshRenderer " : "",
        hasActive ? "Active " : "",
        registry.HasComponent<NameComponent>(entity) ? "Name " : "",
        hasLight ? "Light " : "",
        registry.HasComponent<RigidBodyComponent>(entity) ? "RigidBody " : "",
        registry.HasComponent<StaticColliderComponent>(entity) ? "StaticCollider " : "",
        registry.HasComponent<BuoyancyComponent>(entity) ? "Buoyancy " : "",
        registry.HasComponent<CharacterController>(entity) ? "CharacterController " : "",
        registry.HasComponent<CharacterMovement>(entity) ? "CharacterMovement " : "",
        registry.HasComponent<PlayerGameTag>(entity) ? "PlayerGameTag " : "",
        registry.HasComponent<DoorComponent>(entity) ? "Door " : "",
        registry.HasComponent<CompartmentComponent>(entity) ? "Compartment " : "",
        registry.HasComponent<SubmarineHullConfig>(entity) ? "SubmarineHullConfig " : "",
        registry.HasComponent<SubmarineComponent>(entity) ? "Submarine " : "",
        registry.HasComponent<PressureHullComponent>(entity) ? "PressureHull " : "",
        registry.HasComponent<JointConnectionComponent>(entity) ? "JointConnection " : "",
        registry.HasComponent<DamageDecalComponent>(entity) ? "DamageDecal" : "");

    if (auto* transform = registry.GetComponent<TransformComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DebugVec3Line("Position", transform->position);
            DebugVec3Line("Rotation", transform->rotation);
            DebugVec3Line("Scale", transform->scale);
        }
    }
    else
    {
        ImGui::TextColored(COL_RED, "TransformComponent is missing");
    }

    if (auto* mesh = registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (ImGui::CollapsingHeader("MeshRenderer", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Model ptr: %p", static_cast<void*>(mesh->model));
            ImGui::Text("Material ptr: %p", static_cast<void*>(mesh->material));

            if (mesh->model)
            {
                ImGui::Text("Index count: %d", mesh->model->GetIndexCount());
                ImGui::Text("Mesh parts: %d", static_cast<int>(mesh->model->GetMeshParts().size()));
                DebugBoolLine("Embedded format", mesh->model->IsEmbeddedFormat());
                DebugBoolLine("Has animations", mesh->model->HasAnimations());
                ImGui::Text("Animation clips: %d", static_cast<int>(mesh->model->GetAnimationCount()));
                ImGui::Text("Bone count: %d", static_cast<int>(mesh->model->m_boneInfoMap.size()));

                XMFLOAT3 bbMin{}, bbMax{}, sphereCenter{};
                float sphereRadius = 0.0f;
                mesh->model->GetBoundingBox(&bbMin, &bbMax);
                mesh->model->GetBoundingSphere(&sphereCenter, &sphereRadius);
                DebugVec3Line("Bounds min", bbMin);
                DebugVec3Line("Bounds max", bbMax);
                DebugVec3Line("Sphere center", sphereCenter);
                ImGui::Text("Sphere radius: %.3f", sphereRadius);
            }
            else
            {
                ImGui::TextColored(COL_RED, "Model is null; renderer will skip this entity.");
            }

            MaterialClass* material = mesh->material ? mesh->material : sceneManager.GetDefaultMaterial();
            if (material)
            {
                ImGui::SeparatorText(mesh->material ? "Entity Material" : "Default Material");
                ImGui::Text("Material name: %ls", material->GetName().c_str());
                ImGui::Text("Shader ptr: %p", static_cast<void*>(material->GetShader()));
                ImGui::Text("Shader type: %s", material->GetShader() ? ShaderTypeName(material->GetShader()->GetType()) : "None");
                DebugVec3Line("Diffuse", material->GetDiffuseColor());
                ImGui::Text("Specular power: %.3f", material->GetSpecularPower());
                DebugBoolLine("Glass", material->IsGlass());
                ImGui::Text("Glass opacity: %.3f", material->GetGlassOpacity());
                ImGui::Text("Reflectivity: %.3f", material->GetReflectivity());
                ImGui::Text("Water translation: %.3f", material->GetWaterTranslation());
                ImGui::Text("Reflect/refract scale: %.3f", material->GetReflectRefractScale());
                DebugVec4Line("Refraction tint", material->GetRefractionTint());
                for (size_t slot = 0; slot < 4; ++slot)
                {
                    auto* texture = material->GetTexture(slot);
                    ImGui::Text("Texture slot %zu: %p", slot, static_cast<void*>(texture));
                }
            }
            else
            {
                ImGui::TextColored(COL_RED, "No entity material and no default material.");
            }
        }
    }

    if (auto* anim = registry.GetComponent<AnimationComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Animation index: %d", anim->animationIndex);
            ImGui::Text("Current time: %.3f", anim->currentTime);
            DebugBoolLine("Playing", anim->isPlaying);
            ImGui::Text("Final bone matrices: %d", static_cast<int>(anim->finalBoneMatrices.size()));
        }
    }

    if (auto* light = registry.GetComponent<LightComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DebugVec4Line("Color", light->color);
            DebugVec3Line("Offset", light->offset);
            ImGui::Text("Radius: %.3f", light->radius);
            DebugBoolLine("Enabled", light->enabled);
            DebugBoolLine("Editor gizmo", light->isEditorGizmo);
            ImGui::Text("Attached to: %u", light->attachedTo);
        }
    }

    if (auto* rb = registry.GetComponent<RigidBodyComponent>(entity))
    {
        if (ImGui::CollapsingHeader("RigidBody", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Body ptr: %p", static_cast<void*>(rb->body));
            ImGui::Text("Shape ptr: %p", static_cast<void*>(rb->shape));
            ImGui::Text("Mass: %.3f", rb->mass);
            ImGui::Text("Shape type: %s", RigidBodyShapeName(rb->shapeType));
            ImGui::Text("Shape size: %.3f, %.3f, %.3f", rb->shapeSize.x(), rb->shapeSize.y(), rb->shapeSize.z());
            ImGui::Text("Shape radius: %.3f", rb->shapeRadius);
            DebugBoolLine("Dynamic", rb->isDynamic);
            DebugBoolLine("Kinematic", rb->isKinematic);
            DebugBoolLine("CCD", rb->enableCCD);
            DebugBoolLine("Needs collision rebuild", rb->needsCollisionRebuild);
            DebugBoolLine("Manual size", rb->useManualSize);
            DebugBoolLine("Compound", rb->isCompound);
            ImGui::Text("Compound elements: %d", static_cast<int>(rb->compoundElements.size()));
            ImGui::Text("Convex hulls: %d", static_cast<int>(rb->convexHulls.size()));
            ImGui::Text("Friction: %.3f", rb->friction);
            ImGui::Text("Linear damping: %.3f", rb->linearDamping);
            ImGui::Text("Angular damping: %.3f", rb->angularDamping);
            DebugVec3Line("Last synced position", rb->lastSyncedPosition);
            DebugVec3Line("Last synced rotation", rb->lastSyncedRotation);
        }
    }

    if (auto* collider = registry.GetComponent<StaticColliderComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Static Collider"))
        {
            ImGui::Text("Shape ptr: %p", static_cast<void*>(collider->shape));
            ImGui::Text("Body ptr: %p", static_cast<void*>(collider->body));
        }
    }

    if (auto* character = registry.GetComponent<CharacterController>(entity))
    {
        if (ImGui::CollapsingHeader("Character Controller"))
        {
            ImGui::Text("Ghost ptr: %p", static_cast<void*>(character->ghostObj));
            ImGui::Text("Controller ptr: %p", static_cast<void*>(character->controller));
            ImGui::Text("Capsule ptr: %p", static_cast<void*>(character->capsuleShape));
            ImGui::Text("Step height: %.3f", character->stepHeight);
            ImGui::Text("Jump speed: %.3f", character->jumpSpeed);
            ImGui::Text("Gravity: %.3f", character->gravity);
            ImGui::Text("Max slope: %.3f", character->maxSlope);
            DebugBoolLine("Grounded", character->isGrounded);
            DebugVec3Line("Last synced position", character->lastSyncedPosition);
            DebugVec3Line("Last synced rotation", character->lastSyncedRotation);
        }
    }

    if (auto* movement = registry.GetComponent<CharacterMovement>(entity))
    {
        if (ImGui::CollapsingHeader("Character Movement"))
        {
            DebugVec3Line("Walk direction", movement->walkDirection);
            DebugBoolLine("Want jump", movement->wantJump);
            ImGui::Text("Move speed: %.3f", movement->moveSpeed);
            ImGui::Text("Run speed: %.3f", movement->runSpeed);
            ImGui::Text("Rotation speed: %.3f", movement->rotationSpeed);
            DebugBoolLine("Moving", movement->isMoving);
            DebugBoolLine("Grounded", movement->isGrounded);
            ImGui::Text("Current speed: %.3f", movement->currentSpeed);
        }
    }

    if (registry.HasComponent<PlayerGameTag>(entity))
    {
        if (ImGui::CollapsingHeader("Player GameTag"))
            ImGui::TextColored(COL_ACCENT, "This entity is the current player object.");
    }

    if (auto* buoyancy = registry.GetComponent<BuoyancyComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Buoyancy"))
        {
            ImGui::Text("Fluid density: %.3f", buoyancy->fluidDensity);
            ImGui::Text("Volume: %.3f", buoyancy->volume);
            ImGui::Text("Height: %.3f", buoyancy->height);
            ImGui::Text("Multiplier: %.3f", buoyancy->buoyancyMultiplier);
            ImGui::Text("Smoothed submersion: %.3f", buoyancy->smoothedSubmersion);
            ImGui::Text("Last calculated volume: %.3f", buoyancy->lastCalculatedVolume);
        }
    }

    if (auto* door = registry.GetComponent<DoorComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Door"))
        {
            ImGui::Text("Compartment A: %u", door->compartmentA);
            ImGui::Text("Compartment B: %u", door->compartmentB);
            DebugBoolLine("Open", door->isOpen);
            DebugBoolLine("Broken", door->isBroken);
            DebugBoolLine("Requires power", door->requiresPower);
            ImGui::Text("Flow rate: %.3f", door->flowRate);
            ImGui::Text("Effective flow: %.3f", door->GetEffectiveFlowRate());
            ImGui::Text("Open progress: %.3f", door->openProgress);
            ImGui::Text("Open speed: %.3f", door->openSpeed);
        }
    }

    if (auto* comp = registry.GetComponent<CompartmentComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Compartment"))
        {
            DebugVec3Line("Local min", comp->localMin);
            DebugVec3Line("Local max", comp->localMax);
            DebugVec3Line("Local center", comp->localCenter);
            ImGui::Text("Volume: %.3f", comp->volume);
            ImGui::Text("Water volume: %.3f", comp->waterVolume);
            ImGui::Text("Water level: %.3f", comp->waterLevel);
            ImGui::Text("Breaches: %d", static_cast<int>(comp->breaches.size()));
            DebugBoolLine("Sealed", comp->isSealed);
            DebugBoolLine("Has power", comp->hasPower);
            ImGui::Text("Pump rate: %.3f", comp->pumpRate);
            ImGui::Text("Damage visual: %.3f", comp->damageVisual);
            ImGui::Text("Connected compartments: %d", static_cast<int>(comp->connectedCompartments.size()));
        }
    }

    if (auto* hull = registry.GetComponent<SubmarineHullConfig>(entity))
    {
        if (ImGui::CollapsingHeader("Submarine Hull Config"))
        {
            DebugVec3Line("Hull extents", hull->hullExtents);
            ImGui::Text("Wall thickness: %.3f", hull->wallThickness);
            DebugBoolLine("Kinematic", hull->isKinematic);
            DebugBoolLine("Show debug hull", hull->showDebugHull);
        }
    }

    if (auto* sub = registry.GetComponent<SubmarineComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Submarine"))
        {
            ImGui::Text("Root: %u", sub->submarineRoot);
            ImGui::Text("Compartment index: %d", sub->compartmentIndex);
            DebugBoolLine("Detached", sub->isDetached);
            ImGui::Text("All compartments: %d", static_cast<int>(sub->allCompartments.size()));
            ImGui::Text("Cached total mass: %.3f", sub->cachedTotalMass);
            ImGui::Text("Cached total volume: %.3f", sub->cachedTotalVolume);
            DebugBoolLine("Needs physics update", sub->needsPhysicsUpdate);
            ImGui::Text("Last compartment count: %d", sub->compartmentCountLastFrame);
            ImGui::Text("Proxy entity: %u", sub->proxyEntity);
        }
    }

    if (auto* pressure = registry.GetComponent<PressureHullComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Pressure Hull"))
        {
            ImGui::Text("Max pressure: %.3f", pressure->maxPressure);
            ImGui::Text("Current health: %.3f", pressure->currentHealth);
            ImGui::Text("Repair rate: %.3f", pressure->repairRate);
            DebugBoolLine("Leaking", pressure->isLeaking);
            ImGui::Text("Leak intensity: %.3f", pressure->leakIntensity);
            ImGui::Text("Last depth: %.3f", pressure->lastDepth);
            ImGui::Text("Last pressure: %.3f", pressure->lastPressure);
        }
    }

    if (auto* subJoint = registry.GetComponent<SubmarineJointComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Submarine Joint"))
        {
            ImGui::Text("Compartment A: %u", subJoint->compartmentA);
            ImGui::Text("Compartment B: %u", subJoint->compartmentB);
            ImGui::Text("Joint ptr: %p", static_cast<void*>(subJoint->joint));
            ImGui::Text("Break force: %.3f", subJoint->breakForce);
            ImGui::Text("Current stress: %.3f", subJoint->currentStress);
            DebugBoolLine("Broken", subJoint->isBroken);
        }
    }

    if (auto* joint = registry.GetComponent<JointConnectionComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Joint Connection"))
        {
            ImGui::Text("Target entity: %u", joint->targetEntity);
            ImGui::Text("Pivot A: %.3f, %.3f, %.3f", joint->pivotA.x(), joint->pivotA.y(), joint->pivotA.z());
            ImGui::Text("Pivot B: %.3f, %.3f, %.3f", joint->pivotB.x(), joint->pivotB.y(), joint->pivotB.z());
            ImGui::Text("Break force: %.3f", joint->breakForce);
            DebugBoolLine("Disable collisions", joint->disableCollisions);
            ImGui::Text("Joint ptr: %p", static_cast<void*>(joint->joint));
            DebugBoolLine("Broken", joint->isBroken);
            DebugBoolLine("Show gizmo", joint->showGizmo);
            DebugVec4Line("Gizmo color", joint->gizmoColor);
        }
    }

    if (auto* damage = registry.GetComponent<DamageDecalComponent>(entity))
    {
        if (ImGui::CollapsingHeader("Damage Decals"))
        {
            ImGui::Text("Cracks: %d / %d", static_cast<int>(damage->cracks.size()), DamageDecalComponent::MAX_CRACKS);
            for (size_t i = 0; i < damage->cracks.size(); ++i)
            {
                const auto& crack = damage->cracks[i];
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::TreeNode("Crack", "Crack %d", static_cast<int>(i)))
                {
                    DebugVec3Line("Local position", crack.localPosition);
                    ImGui::Text("Radius: %.3f", crack.radius);
                    ImGui::Text("Intensity: %.3f", crack.intensity);
                    ImGui::Text("Rotation: %.3f", crack.rotation);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
}




} // namespace EditorDebug
