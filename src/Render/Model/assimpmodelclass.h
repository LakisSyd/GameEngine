#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Подключаем твой класс текстуры, чтобы хранить её внутри модели
#include "textureclass.h" 
#include <LightShaderClass.h>

using namespace DirectX;
using namespace std;

struct VertexType
{
    XMFLOAT3 position;
    XMFLOAT2 texture;
    XMFLOAT3 normal;
};

struct MeshPart
{
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    int indexCount;
};

class AssimpModelClass
{
private:
    vector<MeshPart> m_meshParts;
    Assimp::Importer m_importer;
    const aiScene* m_scene;

    // 👇 ДОБАВЛЕНО: Храним текстуру внутри класса, как в старом ObjModelClass
    TextureClass* m_Texture;
    int m_totalIndexCount; 

    //
   

public:
    AssimpModelClass();
    ~AssimpModelClass();

    // 👇 ИЗМЕНЕНО: Добавили textureFilename как во втором аргументе
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const char* modelPath, const char* textureFilename);

    void Shutdown();

    // 👇 ПОВЕДЕНИЕ КАК РАНЬШЕ: Только устанавливает буферы в контекст
    void Render(ID3D11DeviceContext* deviceContext);

    // 👇 ВОЗВРАЩАЕТ ОБЩЕЕ КОЛИЧЕСТВО ИНДЕКСОВ
    int GetIndexCount();

    // 👇 ВОЗВРАЩАЕТ ТЕКСТУРУ (как в старом классе)
    ID3D11ShaderResourceView* GetTexture();
};