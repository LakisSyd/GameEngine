#pragma once

#include <windows.h>
#include <vector>
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
#include "../../RenderTextureClass.h"
#include <BitmapClass.h>
#include "../../ShaderManager .h"

class EditorUI;
class MaterialClass;
class NormalMapShaderClass;

// Global graphics settings
const bool FULL_SCREEN = false;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.0f;
const float SCREEN_NEAR = 0.3f;

// Picking radii
const float LIGHT_SPHERE_RADIUS = 0.3f;
const float OBJECT_SPHERE_RADIUS = 1.5f;
const float SPRITE_BASE_RADIUS_FACTOR = 0.5f;

class ApplicationClass
{
public:
    ApplicationClass();
    ~ApplicationClass() = default;

    bool Initialize(int screenWidth, int screenHeight, HWND hwnd);
    void Shutdown();
    bool Frame();

    // Теперь принимает не‑владеющий указатель — Application не владеет InputClass
    void SetInput(InputClass* input) { m_input = input; }

private:
    // Rendering groups
    void Render3DObjects(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj);
    void RenderLightSources(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj);
    void RenderWorldSprites(ID3D11DeviceContext* context, const XMMATRIX& view, const XMMATRIX& proj);
    void RenderImGuizmo(const XMMATRIX& viewMatrix, const XMMATRIX& projMatrix);  // 🔥 НОВЫЙ

    // Helpers
    void SetupLights(XMFLOAT4 colors[], XMFLOAT4 positions[]);
    XMMATRIX BuildSpriteMatrix(float x, float y, float z, float w, float h) const;

    // Input handling
    void UpdateInputState();
    void HandleCameraMovement();
    void HandleDragDrop();


    // Drag-and-drop helpers
    void StartDraggingLight(int index, const CameraClass::Ray& pickRay);
    void StartDraggingSprite(int index, const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos);
    void StartDraggingObject(int index, const CameraClass::Ray& pickRay, const XMFLOAT3& objPos);
    void UpdateDraggedPosition(const CameraClass::Ray& ray);
    void StopDragging();

private:
    // Core subsystems
    std::unique_ptr<D3DClass> m_direct3D;

    std::unique_ptr<CameraClass> m_camera;
    // НЕ владеющий указатель на InputClass — SystemClass остаётся владельцем
    InputClass* m_input = nullptr;


    ///TEST
    RenderTextureClass* m_renderTexture = nullptr; // Целевая текстура
    BitmapClass* m_renderQuad = nullptr; 
    

    // Models
    AssimpModelClass* m_mainModel;
    AssimpModelClass* m_lightSphereModel;
    AssimpModelClass* m_floorModel;
    AssimpModelClass* m_wallModel;

    // Scene management
    SceneManager m_sceneManager;
    std::vector<GameObject> m_lightSpheres;

    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // Input state (cached each frame)
    bool m_isLeftDown;
    bool m_isRightDown;
    bool m_isMouseCaptured;
    int m_mouseX, m_mouseY;
    XMMATRIX m_viewMatrix;
    XMMATRIX m_projMatrix;
    int m_clickStartX, m_clickStartY;
    bool m_potentialDrag;
    const int m_dragThreshold = 5;

    // Drag-and-drop state
    bool m_isDragging;
    int m_draggedObjectIndex;
    int m_draggedLightIndex;
    int m_draggedSpriteIndex;
    float m_dragDistance;
    XMFLOAT3 m_dragOffset;



    PickingSystem m_pickingSystem;    // ← Выбор объектов
    InputManager m_inputManager;      // ← Обработка ввода

    // UI
    EditorUI m_editorUI;

    //Material

    MaterialClass* m_defaultMaterial;

    // Shaders — теперь НЕ хранятся здесь, только кэшированные указатели для удобства
    TextureShaderClass*       m_textureShader   = nullptr; // ← просто кэш из ShaderManager
    LightShaderClass*         m_lightShader     = nullptr;
    MultiTextureShaderClass*  m_multiTextureShader = nullptr;
    LightMapShaderClass*      m_lightMapShader  = nullptr;
    NormalMapShaderClass*     m_normalMapShader = nullptr;
   
};

// Utility function for ray-sphere intersection
bool RayIntersectsSphere(const CameraClass::Ray& ray, const XMFLOAT3& center, float radius);