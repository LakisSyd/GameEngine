#pragma once

#include <windows.h>
#include <vector>
#include <memory>
#include "D3DClass.h"
#include "cameraclass.h"
#include "InputClass.h"
#include "Textureshaderclass.h"
#include "lightclass.h"
#include "LightShaderClass.h"
#include "assimpmodelclass.h"
#include "gameobject.h"
#include "SceneManager.h"
#include "WorldSpriteClass.h"
#include <EditorUI.h>
#include "ImGuizmo.h"
#include "GameSettings.h"
#include "PickingSystem.h"
#include "InputManager.h"
#include "Multitextureshaderclass.h"
#include "LightMapShaderClass.h"
#include "NormalMapShaderClass.h"
#include "RenderTextureClass.h"
#include <BitmapClass.h>
#include "ShaderManager.h"
#include "FogShaderClass.h"
#include "FileSystem.h"
#include <clipplaneshaderclass.h>
#include "ReflectionShaderClass.h"
#include <colorshaderclass.h>
#include "WaterShaderClass.h"
#include "GlassShaderClass.h"
#include "ProjectionShaderClass.h"
#include "PBRShaderClass.h"
#include "DebugPhysicsVisualizer.h"
#include "ShadowMap.h"
#include "ShadowShaderClass.h"
#include "CompartmentDebugRenderer.h"
#include "../../Renderer.h"
#include "../../CameraController.h"
#include "../../DragDropSystem.h"

const bool  FULL_SCREEN = false;
const bool  VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.3f;

const float LIGHT_SPHERE_RADIUS = 0.3f;
const float OBJECT_SPHERE_RADIUS = 1.5f;
const float SPRITE_BASE_RADIUS_FACTOR = 0.5f;

class ApplicationClass
{
public:
    ApplicationClass() = default; // Или просто пустой конструктор {}

    ~ApplicationClass() = default;

    bool Initialize(int screenWidth, int screenHeight, HWND hwnd);
    void CreatePhysicsTestScene();
    void Shutdown();
    bool Frame(float dt);
    void SyncCompartmentPositions();
    void UpdatePhysics(float dt);
    void UpdateWaterAnimation(float dt);
    void HandleCompartmentGizmo();
    void ProcessEditorRequests();
    void SpawnObjectFromRequest(const EditorUI::SpawnRequest& req);
    void SetInput(InputClass* input) { m_input = input; }

private:
    // ── Методы рендеринга ────────────────────────────────────────────────────
  

    // ── Вспомогательные методы ───────────────────────────────────────────────
  

    // ── Обработка ввода ──────────────────────────────────────────────────────
   

    // ── Вспомогательные методы drag-and-drop ─────────────────────────────────
  
    void StartDraggingSprite(int index, const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos);
    void StartDraggingObject(int index, const CameraClass::Ray& pickRay, const XMFLOAT3& objPos);
    void UpdateDraggedPosition(const CameraClass::Ray& ray);
    void StopDragging();

    XMMATRIX BuildReflectionMatrix(CameraClass* camera, float reflectY);

    void CreateTestSubmarine();

   
private:

    

    void RenderCompartmentCornerHandles();

    void RenderCompoundCornerHandles();

    void RefreshRigidBodyExternal(ECSRegistry& registry, Entity entity, RigidBodyComponent* rb);

    bool RayIntersectsGroundPlane(const CameraClass::Ray& ray, XMFLOAT3& outHit, float groundY);


    ShadowMap* m_shadowMap;
    ShadowShaderClass* m_shadowShader;  // ← НОВОЕ

    // ── Ядро Direct3D ────────────────────────────────────────────────────────
    std::unique_ptr<D3DClass> m_direct3D;

    // ── Камера и ввод ───────────────────────────────────────────────────────
    std::unique_ptr<CameraClass> m_camera;
    InputClass* m_input = nullptr;


    //--- RenderSystem----

    

    // ── Render-to-Texture ───────────────────────────────────────────────────
    RenderTextureClass* m_renderTexture = nullptr;
    RenderTextureClass* m_reflectionTexture = nullptr;
    RenderTextureClass* m_refractionTexture = nullptr;
    BitmapClass* m_renderQuad = nullptr;

    // ── Базовые модели ─────────────────────────────────────────────────────
    AssimpModelClass* m_mainModel = nullptr;
    AssimpModelClass* m_lightSphereModel = nullptr;

	AssimpModelClass* m_boxModel = nullptr;
	AssimpModelClass* m_capsuleModel = nullptr;
    // ── Сцена ────────────────────────────────────────────────────────────────
    SceneManager            m_sceneManager;
    std::vector<GameObject> m_lightSpheres;

    // ── Шейдеры (указатели принадлежат ShaderManager) ───────────────────────
    TextureShaderClass* m_textureShader = nullptr;
    LightShaderClass* m_lightShader = nullptr;
    MultiTextureShaderClass* m_multiTextureShader = nullptr;
    LightMapShaderClass* m_lightMapShader = nullptr;
    NormalMapShaderClass* m_normalMapShader = nullptr;
    FogShaderClass* m_fogShader = nullptr;
    ClipPlaneShaderClass* m_ClipPlaneShader = nullptr;
    ColorShaderClass* m_ColorShader = nullptr;
    WaterShaderClass* m_WaterShader = nullptr;
    GlassShaderClass* m_glassShader = nullptr;
    ReflectionShaderClass* m_reflectionShader = nullptr;
    projectionShaderClass* m_projectionShader = nullptr;
    PBRShaderClass* m_pbrShader = nullptr;

    // ── Размеры экрана ───────────────────────────────────────────────────────
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // ── Матрицы кадра ────────────────────────────────────────────────────────
    XMMATRIX m_viewMatrix;
    XMMATRIX m_projMatrix;

    // ── Состояние активного перетаскивания ───────────────────────────────────
    bool     m_isDragging = false;
    int      m_draggedObjectIndex = -1;
    int      m_draggedLightIndex = -1;
    int      m_draggedSpriteIndex = -1;
    float    m_dragDistance = 0.0f;
    XMFLOAT3 m_dragOffset = { 0, 0, 0 };

    PickingSystem::PickResult m_pendingPickResult;

    // ── Подсистемы ───────────────────────────────────────────────────────────
    PickingSystem m_pickingSystem;
    InputManager  m_inputManager;

    // ── Редактор ─────────────────────────────────────────────────────────────
    EditorUI m_editorUI;
    ColorShaderClass* m_colorShader = nullptr;
    // ── Материал по умолчанию ────────────────────────────────────────────────
    MaterialClass* m_defaultMaterial = nullptr;

    // Фоллбек-текстура для defaultMaterial (stone.tga)
    // MaterialClass не владеет текстурами, поэтому освобождаем отдельно.
    TextureClass* m_defaultTexture = nullptr;

    // ── Файловая система ─────────────────────────────────────────────────────
    std::unique_ptr<FileSystem> m_gameFs;

    ECSRegistry m_registry;
    DebugPhysicsVisualizer* m_debugVisualizer = nullptr;
    private:
        PhysicsDebugRenderer* m_physicsDebugRenderer = nullptr;
        PhysicsDebugSettings m_physicsDebugSettings;  // Настройки

    
        CompartmentDebugRenderer* m_compartmentRenderer = nullptr;

    ID3D11RasterizerState* m_wireframeRS = nullptr;

    Renderer* m_renderer = nullptr; // <-- Теперь это указатель

    std::unique_ptr<CameraController> m_cameraController;

    std::unique_ptr <DragDropSystem> m_dragDropSystem;
};

bool RayIntersectsSphere(const CameraClass::Ray& ray, const XMFLOAT3& center, float radius);