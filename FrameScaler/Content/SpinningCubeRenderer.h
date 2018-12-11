#pragma once

#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"
#include "ShaderStructures.h"

#include <float.h>

using namespace DirectX;

namespace FrameScaler
{
	struct BoundingBox2D
	{
		XMFLOAT2 Min;
		XMFLOAT2 Max;

		BoundingBox2D()
			: Min(FLT_MAX, FLT_MAX),
			Max(-FLT_MAX, -FLT_MAX)
		{}

        BoundingBox2D(const XMFLOAT2& Min, const XMFLOAT2& Max)
            : Min(Min), Max(Max) {}

        float Width() const { return Max.x - Min.x; }
        float Height() const { return Max.y - Min.y; }

		void AddPoint(float x, float y)
		{
			Min.x = x < Min.x ? x : Min.x;
			Min.y = y < Min.y ? y : Min.y;
			Max.x = x > Max.x ? x : Max.x;
			Max.y = y > Max.y ? y : Max.y;
		}

        bool IncludePoint(const XMFLOAT2& v) const
        {
            return Min.x < v.x && Min.y < v.y && Max.x > v.x && Max.y > v.y;
        }

        bool Intersect(const BoundingBox2D& bb) const
        {
            return IncludePoint(bb.Min) || IncludePoint(bb.Max);
        }

		XMFLOAT2 mid() const
		{
			XMFLOAT2 m;
			m.x = (Min.x + Max.x) * 0.5f;
			m.y = (Min.y + Max.y) * 0.5f;

			return m;
		}
	};

	struct BoundingBox3D
	{
		XMFLOAT3 Min;
		XMFLOAT3 Max;

		XMFLOAT3 vertices[8];

		BoundingBox3D()
			: Min(FLT_MAX, FLT_MAX, FLT_MAX),
			Max(-FLT_MAX, -FLT_MAX, -FLT_MAX)
		{}

		void AddPoint(const XMFLOAT3 &p)
		{
			float* m = &Min.x;
			float* M = &Max.x;
			const float* pPoint = &p.x;

			for (int i = 0; i < 3; ++i) {
				if (m[i] > pPoint[i]) {
					m[i] = pPoint[i];
				}
				if (M[i] < pPoint[i]) {
					M[i] = pPoint[i];
				}
			}
		}

		void BuildGeometry()
		{
			vertices[0] = Min;
			vertices[1] = Max;
			vertices[2] = XMFLOAT3(Min.x, Max.y, Max.z);
			vertices[3] = XMFLOAT3(Min.x, Max.y, Min.z);
			vertices[4] = XMFLOAT3(Max.x, Max.y, Min.z);
			vertices[5] = XMFLOAT3(Min.x, Min.y, Max.z);
			vertices[6] = XMFLOAT3(Max.x, Min.y, Max.z);
			vertices[7] = XMFLOAT3(Max.x, Min.y, Min.z);
		}
	};

    // This sample renderer instantiates a basic rendering pipeline.
    class SpinningCubeRenderer
    {
    public:
        SpinningCubeRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
        void CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Update(const DX::StepTimer& timer);
        void Render();

        // Repositions the sample hologram.
        void PositionHologram(Windows::UI::Input::Spatial::SpatialPointerPose^ pointerPose);

        // Property accessors.
        void SetPosition(Windows::Foundation::Numerics::float3 pos) { m_position = pos;  }
        Windows::Foundation::Numerics::float3 GetPosition()         { return m_position; }

		BoundingBox3D boundingBox;
		XMFLOAT4X4 modelMatrix;
		XMFLOAT2 lastProjectedPosition;

		void SetCulled(bool isCulled);
		void SetReductionLevel(int reductionLevel);

		bool isCulled = false;
		int reductionLevel = 0;

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>            m_deviceResources;

        // Direct3D resources for cube geometry.
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer_2;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer_2;

        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer_1;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer_1;

        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_modelConstantBuffer;

        // System resources for cube geometry.
        ModelConstantBuffer                             m_modelConstantBufferData;
        uint32                                          m_indexCount = 0;
        uint32                                          m_indexCount_1 = 0;
        uint32                                          m_indexCount_2 = 0;

        // Variables used with the rendering loop.
        bool                                            m_loadingComplete = false;
        float                                           m_degreesPerSecond = 45.f;
        Windows::Foundation::Numerics::float3           m_position = { 0.f, 0.f, -2.f };

        // If the current D3D Device supports VPRT, we can avoid using a geometry
        // shader just to set the render target array index.
        bool                                            m_usingVprtShaders = false;
    };
}
