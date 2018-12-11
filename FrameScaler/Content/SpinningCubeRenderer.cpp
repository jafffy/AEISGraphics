#include "pch.h"
#include "SpinningCubeRenderer.h"
#include "Common\DirectXHelper.h"

#include "Common/tiny_obj_loader.h"

using namespace FrameScaler;
using namespace Concurrency;
using namespace DirectX;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI::Input::Spatial;

struct BoundingBox3D {
    XMFLOAT3 Min, Max;

    BoundingBox3D()
        : Min(FLT_MAX, FLT_MAX, FLT_MAX),
        Max(-FLT_MAX, -FLT_MAX, -FLT_MAX) {}

    BoundingBox3D(const XMFLOAT3& Min, const XMFLOAT3& Max)
        : Min(Min), Max(Max) {}

    void AddPoint(const XMFLOAT3& p) {
        Min.x = p.x < Min.x ? p.x : Min.x;
        Min.y = p.y < Min.y ? p.y : Min.y;
        Min.z = p.z < Min.z ? p.z : Min.z;

        Max.x = p.x > Max.x ? p.x : Max.x;
        Max.y = p.y > Max.y ? p.y : Max.y;
        Max.z = p.z > Max.z ? p.z : Max.z;
    }

    bool IncludePoint(const XMFLOAT3& p) {
        return Min.x < p.x && Min.y < p.y && Min.z < p.z
            && Max.x > p.x && Max.y > p.y && Max.z > p.z;
    }
};


SpinningCubeRenderer::SpinningCubeRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

void SpinningCubeRenderer::PositionHologram(SpatialPointerPose^ pointerPose)
{
    if (pointerPose != nullptr)
    {
        const float3 headPosition    = pointerPose->Head->Position;
        const float3 headDirection   = pointerPose->Head->ForwardDirection;

        constexpr float distanceFromUser    = 2.0f; // meters
        const float3 gazeAtTwoMeters        = headPosition + (distanceFromUser * headDirection);

        SetPosition(gazeAtTwoMeters);
    }
}

void SpinningCubeRenderer::Update(const DX::StepTimer& timer)
{
    const float    radiansPerSecond = XMConvertToRadians(m_degreesPerSecond) * 2;
    const double   totalRotation    = timer.GetTotalSeconds() * radiansPerSecond;

    const XMMATRIX modelTranslation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

    XMStoreFloat4x4(&m_modelConstantBufferData.model, XMMatrixTranspose(modelTranslation));

	XMStoreFloat4x4(&modelMatrix, modelTranslation);

    if (!m_loadingComplete)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    context->UpdateSubresource(
        m_modelConstantBuffer.Get(),
        0,
        nullptr,
        &m_modelConstantBufferData,
        0,
        0
        );
}

void SpinningCubeRenderer::Render()
{
    if (!m_loadingComplete)
    {
        return;
    }

	if (isCulled)
		return;

    const auto context = m_deviceResources->GetD3DDeviceContext();

    const UINT stride = sizeof(VertexPositionColor);
    const UINT offset = 0;
	if (this->reductionLevel == 0) {
		context->IASetVertexBuffers(
			0,
			1,
			m_vertexBuffer.GetAddressOf(),
			&stride,
			&offset
		);
		context->IASetIndexBuffer(
			m_indexBuffer.Get(),
			DXGI_FORMAT_R32_UINT, // Each index is one 16-bit unsigned integer (short).
			0
		);
	}
	else if (reductionLevel == 1) {
		context->IASetVertexBuffers(
			0,
			1,
			m_vertexBuffer_1.GetAddressOf(),
			&stride,
			&offset
		);
		context->IASetIndexBuffer(
			m_indexBuffer_1.Get(),
			DXGI_FORMAT_R32_UINT, // Each index is one 16-bit unsigned integer (short).
			0
		);
	}
	else if (reductionLevel == 2) {
		context->IASetVertexBuffers(
			0,
			1,
			m_vertexBuffer_2.GetAddressOf(),
			&stride,
			&offset
		);
		context->IASetIndexBuffer(
			m_indexBuffer_2.Get(),
			DXGI_FORMAT_R32_UINT, // Each index is one 16-bit unsigned integer (short).
			0
		);
	}
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(m_inputLayout.Get());

	context->VSSetShader(
		m_vertexShader.Get(),
		nullptr,
		0
	);
	context->VSSetConstantBuffers(
		0,
		1,
		m_modelConstantBuffer.GetAddressOf()
	);

	if (!m_usingVprtShaders)
	{
		context->GSSetShader(
			m_geometryShader.Get(),
			nullptr,
			0
		);
	}

	context->PSSetShader(
		m_pixelShader.Get(),
		nullptr,
		0
	);

	if (reductionLevel == 0) {
		context->DrawIndexedInstanced(
			m_indexCount,   // Index count per instance.
			2,              // Instance count.
			0,              // Start index location.
			0,              // Base vertex location.
			0               // Start instance location.
		);
	}
	else if (reductionLevel == 1) {
		context->DrawIndexedInstanced(
			m_indexCount_1,   // Index count per instance.
			2,              // Instance count.
			0,              // Start index location.
			0,              // Base vertex location.
			0               // Start instance location.
		);
	}
	else if (reductionLevel == 2) {
		context->DrawIndexedInstanced(
			m_indexCount_2,   // Index count per instance.
			2,              // Instance count.
			0,              // Start index location.
			0,              // Base vertex location.
			0               // Start instance location.
		);
	}
}

void SpinningCubeRenderer::CreateDeviceDependentResources()
{
	m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

	std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

	task<std::vector<byte>> loadVSTask = DX::ReadDataAsync(vertexShaderFileName);
	task<std::vector<byte>> loadPSTask = DX::ReadDataAsync(L"ms-appx:///PixelShader.cso");

	task<std::vector<byte>> loadGSTask;
	if (!m_usingVprtShaders)
	{
		loadGSTask = DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");
	}

	task<void> createVSTask = loadVSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateVertexShader(
				fileData.data(),
				fileData.size(),
				nullptr,
				&m_vertexShader
			)
		);

		constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 3> vertexDesc =
		{ {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		} };

		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateInputLayout(
				vertexDesc.data(),
				static_cast<UINT>(vertexDesc.size()),
				fileData.data(),
				static_cast<UINT>(fileData.size()),
				&m_inputLayout
			)
		);
	});

	// After the pixel shader file is loaded, create the shader and constant buffer.
	task<void> createPSTask = loadPSTask.then([this](const std::vector<byte>& fileData)
	{
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreatePixelShader(
				fileData.data(),
				fileData.size(),
				nullptr,
				&m_pixelShader
			)
		);

		const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
		DX::ThrowIfFailed(
			m_deviceResources->GetD3DDevice()->CreateBuffer(
				&constantBufferDesc,
				nullptr,
				&m_modelConstantBuffer
			)
		);
	});

	task<void> createGSTask;
	if (!m_usingVprtShaders)
	{
		// After the pass-through geometry shader file is loaded, create the shader.
		createGSTask = loadGSTask.then([this](const std::vector<byte>& fileData)
		{
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateGeometryShader(
					fileData.data(),
					fileData.size(),
					nullptr,
					&m_geometryShader
				)
			);
		});
	}

	static bool isVerticesFinished = false;
	static bool isVerticesFinished_1 = false;
	static bool isVerticesFinished_2 = false;
	static std::vector<VertexPositionColor> cubeVertices;
	static std::vector<unsigned int> cubeIndices;
	static std::vector<VertexPositionColor> cubeVertices_1;
	static std::vector<unsigned int> cubeIndices_1;
	static std::vector<VertexPositionColor> cubeVertices_2;
	static std::vector<unsigned int> cubeIndices_2;

	// Once all shaders are loaded, create the mesh.
	task<void> shaderTaskGroup = m_usingVprtShaders ? (createPSTask && createVSTask) : (createPSTask && createVSTask && createGSTask);
	task<void> createCubeTask = shaderTaskGroup.then([this]()
	{
		{
			if (!isVerticesFinished) {
				tinyobj::attrib_t attrib;
				std::vector<tinyobj::shape_t> shapes;
				std::vector<tinyobj::material_t> materials;
				std::string err;

				bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, ".\\Assets\\sphere.obj", ".\\Assets\\", false);

				if (!err.empty()) {
					OutputDebugStringA(err.c_str());
				}
				if (!ret) {
					OutputDebugStringA("Failed to load/parse .obj\n");
				}

				for (unsigned int i = 0; i < attrib.vertices.size() / 3; ++i) {
					XMFLOAT3 v;
					v.x = attrib.vertices[3 * i + 0] * 0.1f;
					v.y = attrib.vertices[3 * i + 1] * 0.1f;
					v.z = attrib.vertices[3 * i + 2] * 0.1f;

					XMFLOAT3 normal;
					normal.x = attrib.normals[3 * i + 0];
					normal.y = attrib.normals[3 * i + 1];
					normal.z = attrib.normals[3 * i + 2];

					cubeVertices.push_back(VertexPositionColor(v, normal, XMFLOAT3(1, 1, 1)));
				}

				for (unsigned int si = 0; si < shapes.size(); ++si) {
					for (unsigned int ii = 0; ii < shapes[si].mesh.indices.size() / 3; ++ii) {
						unsigned int a = shapes[si].mesh.indices[3 * ii + 0].vertex_index;
						unsigned int b = shapes[si].mesh.indices[3 * ii + 1].vertex_index;
						unsigned int c = shapes[si].mesh.indices[3 * ii + 2].vertex_index;

						cubeIndices.push_back(a);
						cubeIndices.push_back(b);
						cubeIndices.push_back(c);
					}
				}

				isVerticesFinished = true;
			}

			for (size_t i = 0; i < cubeVertices.size(); ++i) {
				boundingBox.AddPoint(cubeVertices[i].pos);
			}
			boundingBox.BuildGeometry();

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = cubeVertices.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(cubeVertices.size()), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&vertexBufferDesc,
					&vertexBufferData,
					&m_vertexBuffer
				)
			);

			m_indexCount = static_cast<unsigned int>(cubeIndices.size());

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = cubeIndices.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * static_cast<UINT>(cubeIndices.size()), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&indexBufferDesc,
					&indexBufferData,
					&m_indexBuffer
				)
			);
		}
		{
			if (!isVerticesFinished_1) {
				tinyobj::attrib_t attrib;
				std::vector<tinyobj::shape_t> shapes;
				std::vector<tinyobj::material_t> materials;
				std::string err;

				bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, ".\\Assets\\sphere_1.obj", ".\\Assets\\", false);

				if (!err.empty()) {
					OutputDebugStringA(err.c_str());
				}
				if (!ret) {
					OutputDebugStringA("Failed to load/parse .obj\n");
				}

				for (unsigned int i = 0; i < attrib.vertices.size() / 3; ++i) {
					XMFLOAT3 v;
					v.x = attrib.vertices[3 * i + 0] * 0.1f;
					v.y = attrib.vertices[3 * i + 1] * 0.1f;
					v.z = attrib.vertices[3 * i + 2] * 0.1f;

					XMFLOAT3 normal;
					normal.x = attrib.normals[3 * i + 0];
					normal.y = attrib.normals[3 * i + 1];
					normal.z = attrib.normals[3 * i + 2];

					cubeVertices_1.push_back(VertexPositionColor(v, normal, XMFLOAT3(1, 1, 1)));
				}

				for (unsigned int si = 0; si < shapes.size(); ++si) {
					for (unsigned int ii = 0; ii < shapes[si].mesh.indices.size() / 3; ++ii) {
						unsigned int a = shapes[si].mesh.indices[3 * ii + 0].vertex_index;
						unsigned int b = shapes[si].mesh.indices[3 * ii + 1].vertex_index;
						unsigned int c = shapes[si].mesh.indices[3 * ii + 2].vertex_index;

						cubeIndices_1.push_back(a);
						cubeIndices_1.push_back(b);
						cubeIndices_1.push_back(c);
					}
				}

				isVerticesFinished_1 = true;
			}

			for (size_t i = 0; i < cubeVertices_1.size(); ++i) {
				boundingBox.AddPoint(cubeVertices_1[i].pos);
			}
			boundingBox.BuildGeometry();

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = cubeVertices_1.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(cubeVertices_1.size()), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&vertexBufferDesc,
					&vertexBufferData,
					&m_vertexBuffer_1
				)
			);

			m_indexCount_1 = static_cast<unsigned int>(cubeIndices_1.size());

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = cubeIndices_1.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * static_cast<UINT>(cubeIndices_1.size()), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&indexBufferDesc,
					&indexBufferData,
					&m_indexBuffer_1
				)
			);
		}
		{
			if (!isVerticesFinished_2) {
				tinyobj::attrib_t attrib;
				std::vector<tinyobj::shape_t> shapes;
				std::vector<tinyobj::material_t> materials;
				std::string err;

				bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, ".\\Assets\\sphere_2.obj", ".\\Assets\\", false);

				if (!err.empty()) {
					OutputDebugStringA(err.c_str());
				}
				if (!ret) {
					OutputDebugStringA("Failed to load/parse .obj\n");
				}

				for (unsigned int i = 0; i < attrib.vertices.size() / 3; ++i) {
					XMFLOAT3 v;
					v.x = attrib.vertices[3 * i + 0] * 0.1f;
					v.y = attrib.vertices[3 * i + 1] * 0.1f;
					v.z = attrib.vertices[3 * i + 2] * 0.1f;

					XMFLOAT3 normal;
					normal.x = attrib.normals[3 * i + 0];
					normal.y = attrib.normals[3 * i + 1];
					normal.z = attrib.normals[3 * i + 2];

					cubeVertices_2.push_back(VertexPositionColor(v, normal, XMFLOAT3(1, 1, 1)));
				}

				for (unsigned int si = 0; si < shapes.size(); ++si) {
					for (unsigned int ii = 0; ii < shapes[si].mesh.indices.size() / 3; ++ii) {
						unsigned int a = shapes[si].mesh.indices[3 * ii + 0].vertex_index;
						unsigned int b = shapes[si].mesh.indices[3 * ii + 1].vertex_index;
						unsigned int c = shapes[si].mesh.indices[3 * ii + 2].vertex_index;

						cubeIndices_2.push_back(a);
						cubeIndices_2.push_back(b);
						cubeIndices_2.push_back(c);
					}
				}

				isVerticesFinished_2 = true;
			}

			for (size_t i = 0; i < cubeVertices_2.size(); ++i) {
				boundingBox.AddPoint(cubeVertices_2[i].pos);
			}
			boundingBox.BuildGeometry();

			D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
			vertexBufferData.pSysMem = cubeVertices_2.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;
			const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(cubeVertices_2.size()), D3D11_BIND_VERTEX_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&vertexBufferDesc,
					&vertexBufferData,
					&m_vertexBuffer_2
				)
			);

			m_indexCount_2 = static_cast<unsigned int>(cubeIndices_2.size());

			D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
			indexBufferData.pSysMem = cubeIndices_2.data();
			indexBufferData.SysMemPitch = 0;
			indexBufferData.SysMemSlicePitch = 0;
			CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned int) * static_cast<UINT>(cubeIndices_2.size()), D3D11_BIND_INDEX_BUFFER);
			DX::ThrowIfFailed(
				m_deviceResources->GetD3DDevice()->CreateBuffer(
					&indexBufferDesc,
					&indexBufferData,
					&m_indexBuffer_2
				)
			);
		}
	});

	// Once the cube is loaded, the object is ready to be rendered.
	createCubeTask.then([this]()
	{
		m_loadingComplete = true;
	});
}

void SpinningCubeRenderer::ReleaseDeviceDependentResources()
{
	m_loadingComplete = false;
	m_usingVprtShaders = false;
	m_vertexShader.Reset();
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_geometryShader.Reset();
	m_modelConstantBuffer.Reset();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
}

void SpinningCubeRenderer::SetCulled(bool isCulled)
{
	this->isCulled = isCulled;
}

void SpinningCubeRenderer::SetReductionLevel(int reductionLevel)
{
	this->reductionLevel = reductionLevel;
}
