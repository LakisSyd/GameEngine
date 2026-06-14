#include "Textureshaderclass.h"
#include "helper.h"


const wchar_t* TexturePsPath = L"src/Render/Shader/Ps/texture.ps";
const wchar_t* TextureVsPath = L"src/Render/Shader/Vs/texture.vs";


TextureShaderClass::TextureShaderClass()
{
	m_vertexShader = nullptr;
	m_pixelShader = nullptr;
	m_layout = nullptr;
	m_matrixBuffer = nullptr;

	// Новая переменная сэмплера устанавливается в null в конструкторе класса.
	m_sampleState = nullptr;


}

TextureShaderClass::TextureShaderClass(const TextureShaderClass& other)
{
}


TextureShaderClass::~TextureShaderClass()
{
}


bool TextureShaderClass::Initialize(ID3D11Device* device, HWND hwnd) {
	bool result;
	wchar_t vsFilename[128];
	wchar_t psFilename[128];
	int error;

	// 👈 1. Исправляем пути к шейдерам
	// Если шейдеры лежат в папке Engine рядом с проектом:


	error = wcscpy_s(vsFilename, 128, TextureVsPath);
	if (error != 0) {
		MessageBox(hwnd, L"texture.vs not found", L"ERROR", MB_OK);
	}

	error = wcscpy_s(psFilename, 128, TexturePsPath);
	if (error != 0) {
		MessageBox(hwnd, L"texture.ps not found", L"ERROR", MB_OK);
	}

	// 👈 2. Чистая инициализация (без вставок из ApplicationClass)
	result = InitializeShader(device, hwnd, vsFilename, psFilename);
	if (!result)
	{
		return false;
	}

	return true;

	
}


void TextureShaderClass::Shutdown()
{
	// Выключаем вершинный и пиксельный шейдеры, а также связанные объекты.
	ShutdownShader();

	return;
}


bool TextureShaderClass::Render(ID3D11DeviceContext* deviceContext, int indexCount, XMMATRIX worldMatrix, XMMATRIX viewMatrix,
	XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture)
{


	// Устанавливаем параметры шейдера, которые он будет использовать для рендеринга.
	DX_CHECK(SetShaderParameters(deviceContext, worldMatrix, viewMatrix, projectionMatrix, texture));

	// Теперь рендерим подготовленные буферы с помощью шейдера.
	RenderShader(deviceContext, indexCount);

	return true;
}

bool TextureShaderClass::InitializeShader(ID3D11Device* device, HWND hwnd, WCHAR* vsFilename, WCHAR* psFilename)
{
	ID3D10Blob* errorMessage;
	ID3D10Blob* vertexShaderBuffer;
	ID3D10Blob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	unsigned int numElements;
	D3D11_BUFFER_DESC matrixBufferDesc;

	// У нас есть новая переменная для хранения описания сэмплера текстуры, 
   // который будет настроен в этой функции.
	D3D11_SAMPLER_DESC samplerDesc;

	errorMessage = nullptr;
	vertexShaderBuffer = nullptr;
	pixelShaderBuffer = nullptr;

	DX_CHECK(D3DCompileFromFile(vsFilename, NULL, NULL, "TextureVertexShader", "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&vertexShaderBuffer, &errorMessage));

	DX_CHECK(D3DCompileFromFile(psFilename, NULL, NULL, "TexturePixelShader", "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0,
		&pixelShaderBuffer, &errorMessage));


	DX_CHECK(device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), 
		vertexShaderBuffer->GetBufferSize(), NULL, &m_vertexShader));

	DX_CHECK(device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pixelShader));

	// Макет ввода (input layout) изменился, так как теперь у нас есть элемент текстуры вместо цвета. 
	// Первый элемент позиции остается без изменений, но SemanticName и Format второго элемента 
	// были изменены на TEXCOORD и DXGI_FORMAT_R32G32_FLOAT. 
	// Эти два изменения теперь согласуют этот макет с нашим новым VertexType 
	// как в определении ModelClass, так и в typedefs в файлах шейдеров.

	// Создаем описание макета ввода вершин.
	// Эта настройка должна соответствовать структуре VertexType в ModelClass и в шейдере.
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	// Получаем количество элементов в макете.
	numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// Создаем макет ввода вершин.
	DX_CHECK(device->CreateInputLayout(polygonLayout, numElements,
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_layout));

	vertexShaderBuffer->Release();
	vertexShaderBuffer = nullptr;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = nullptr;

	// Настраиваем описание динамического константного буфера матрицы, который находится в вершинном шейдере.
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBufferType);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;



	HRESULT hr = device->CreateBuffer(&matrixBufferDesc, nullptr, &m_matrixBuffer);


	if (FAILED(hr)) {
		OutputDebugStringA("ERROR: CreateBuffer for m_matrixBuffer FAILED!\n");
		char msg[256];
		sprintf_s(msg, "HRESULT: 0x%08X\n", hr);
		OutputDebugStringA(msg);
		return false;  // ⭐ Критично: возвращаем false при ошибке!
	}

	// Описание состояния сэмплера текстуры настраивается здесь, 
	// а затем может быть передано пиксельному шейдеру после. 
	// Наиболее важным элементом описания сэмплера текстуры является Filter. 
	// Filter определит, как он решает, какие пиксели будут использованы или объединены 
	// для создания окончательного вида текстуры на грани полигона. 
	// В примере здесь я использую D3D11_FILTER_MIN_MAG_MIP_LINEAR, 
	// что дороже с точки зрения обработки, но дает лучший визуальный результат. 
	// Это говорит сэмплеру использовать линейную интерполяцию для уменьшения, увеличения и сэмплирования mip-уровней.
	//
	// AddressU и AddressV установлены в Wrap, что гарантирует, 
	// что координаты остаются между 0.0f и 1.0f. 
	// Все, что выходит за пределы этого диапазона, оборачивается и помещается между 0.0f и 1.0f. 
	// Все остальные настройки для описания состояния сэмплера являются значениями по умолчанию.

	// Создаем описание состояния сэмплера текстуры.
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	DX_CHECK(device->CreateSamplerState(&samplerDesc, &m_sampleState));

	return true;

}

void TextureShaderClass::ShutdownShader()
{
	// Функция ShutdownShader теперь освобождает новое состояние сэмплера, 
	// которое было создано во время инициализации.

	// Освобождаем состояние сэмплера.
	if (m_sampleState)
	{
		m_sampleState->Release();
		m_sampleState = 0;
	}

	// Освобождаем константный буфер матрицы.
	if (m_matrixBuffer)
	{
		m_matrixBuffer->Release();
		m_matrixBuffer = 0;
	}

	// Освобождаем макет (layout).
	if (m_layout)
	{
		m_layout->Release();
		m_layout = 0;
	}

	// Освобождаем пиксельный шейдер.
	if (m_pixelShader)
	{
		m_pixelShader->Release();
		m_pixelShader = 0;
	}

	// Освобождаем вершинный шейдер.
	if (m_vertexShader)
	{
		m_vertexShader->Release();
		m_vertexShader = 0;
	}

	return;
}

void TextureShaderClass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{
	char* compileErrors;
	unsigned long long bufferSize, i;
	ofstream fout;


	// Получаем указатель на текстовый буфер сообщения об ошибке.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Получаем длину сообщения.
	bufferSize = errorMessage->GetBufferSize();

	// Открываем файл для записи сообщения об ошибке.
	fout.open("shader-error.txt");

	// Записываем сообщение об ошибке.
	for (i = 0; i < bufferSize; i++)
	{
		fout << compileErrors[i];
	}

	// Закрываем файл.
	fout.close();

	// Освобождаем сообщение об ошибке.
	errorMessage->Release();
	errorMessage = 0;

	// Выводим сообщение на экран, чтобы уведомить пользователя проверить текстовый файл на ошибки компиляции.
	MessageBox(hwnd, L"Error compiling shader.  Check shader-error.txt for message.", shaderFilename, MB_OK);

	return;
}
bool TextureShaderClass::SetShaderParameters(ID3D11DeviceContext* deviceContext, XMMATRIX worldMatrix, XMMATRIX viewMatrix,
	XMMATRIX projectionMatrix, ID3D11ShaderResourceView* texture)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	MatrixBufferType* dataPtr;
	unsigned int bufferNumber;


	// Транспонируем матрицы, чтобы подготовить их для шейдера.
	worldMatrix = XMMatrixTranspose(worldMatrix);
	viewMatrix = XMMatrixTranspose(viewMatrix);
	projectionMatrix = XMMatrixTranspose(projectionMatrix);

	if (!deviceContext) {
		OutputDebugStringA("ERROR: deviceContext is nullptr!\n");
		return false;
	}

	// 🔴 ПРОВЕРКА 2: m_matrixBuffer не null?
	if (!m_matrixBuffer) {
		OutputDebugStringA("ERROR: m_matrixBuffer is nullptr! Did you call Initialize()?\n");
		return false;
	}

	// 🔴 ПРОВЕРКА 3: this не null? (если внутри метода класса)
	// Это проверяется автоматически, но можно добавить для ясности:
	assert(this != nullptr);

	// В SetShaderParameters, после транспонирования, но до Map



	// Блокируем константный буфер, чтобы в него можно было записать данные.
	result = deviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		return false;
	}

	// Получаем указатель на данные в константном буфере.
	dataPtr = (MatrixBufferType*)mappedResource.pData;

	// Копируем матрицы в константный буфер.
	dataPtr->world = worldMatrix;
	dataPtr->view = viewMatrix;
	dataPtr->projection = projectionMatrix;

	// Разблокируем константный буфер.
	deviceContext->Unmap(m_matrixBuffer, 0);

	// Устанавливаем позицию константного буфера в вершинном шейдере.
	bufferNumber = 0;

	// Наконец, устанавливаем константный буфер в вершинном шейдере с обновленными значениями.
	deviceContext->VSSetConstantBuffers(bufferNumber, 1, &m_matrixBuffer);

	// Функция SetShaderParameters была изменена по сравнению с предыдущим уроком, 
	// чтобы включить установку текстуры в пиксельный шейдер теперь.

	// Устанавливаем ресурс текстуры шейдера в пиксельном шейдере.
	deviceContext->PSSetShaderResources(0, 1, &texture);

	return true;
}
void TextureShaderClass::RenderShader(ID3D11DeviceContext* deviceContext, int indexCount)
{
	// Устанавливаем макет ввода вершин.
	deviceContext->IASetInputLayout(m_layout);

	// Устанавливаем вершинный и пиксельный шейдеры, 
	// которые будут использоваться для рендеринга этого треугольника.
	deviceContext->VSSetShader(m_vertexShader, NULL, 0);
	deviceContext->PSSetShader(m_pixelShader, NULL, 0);

	// Функция RenderShader была изменена, чтобы включать установку состояния сэмплера 
	// в пиксельном шейдере перед рендерингом.

	// Устанавливаем состояние сэмплера в пиксельном шейдере.
	deviceContext->PSSetSamplers(0, 1, &m_sampleState);

	// Рендерим треугольник.
	deviceContext->DrawIndexed(indexCount, 0, 0);

	return;
}
