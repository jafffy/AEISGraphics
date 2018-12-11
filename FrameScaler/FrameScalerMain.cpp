#include "pch.h"
#include "FrameScalerMain.h"
#include "Common\DirectXHelper.h"
#include "FramerateController.h"

#include <string>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Collection.h>
#include <cfloat>


using namespace FrameScaler;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Holographic;
using namespace Windows::Perception::Spatial;
using namespace Windows::UI::Input::Spatial;
using namespace std::placeholders;
using namespace DirectX;

ID3D11Texture2D* GetReadableTexture2D(
	int &width, int &height,
	ID3D11Texture2D* targetTex,
	ID3D11Device* device,
	ID3D11DeviceContext* context)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	targetTex->GetDesc(&texture2DDesc);

	width = texture2DDesc.Width;
	height = texture2DDesc.Height;

	texture2DDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	texture2DDesc.Usage = D3D11_USAGE_STAGING;
	texture2DDesc.BindFlags = 0;

	ID3D11Texture2D* copiedBackBufferTex;
	device->CreateTexture2D(&texture2DDesc, nullptr, &copiedBackBufferTex);
	context->CopyResource(copiedBackBufferTex, targetTex);

	return copiedBackBufferTex;
}

void GetTexture2DDataFromGPU(unsigned char** dest,
	ID3D11Texture2D* tex,
	ID3D11DeviceContext* context,
	int width, int height)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	context->Map(tex, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mappedResource);

	if (mappedResource.pData)
	{
		unsigned char* data = reinterpret_cast<unsigned char*>(mappedResource.pData);

		memcpy(*dest, data, sizeof(unsigned int) * width* height);

		context->Unmap(tex, 0);
	}
}

float GetDynamicScoreBasedOnMSE(
	const unsigned char* previousFrame,
	const unsigned char* currentFrame,
	int width, int height)
{
	float mse = 0;

	for (int i = 0; i < width * height; ++i) {
		float lR = float(previousFrame[4 * i + 0] / 255);
		float lG = float(previousFrame[4 * i + 1] / 255);
		float lB = float(previousFrame[4 * i + 2] / 255);

		float cR = float(currentFrame[4 * i + 0] / 255);
		float cG = float(currentFrame[4 * i + 1] / 255);
		float cB = float(currentFrame[4 * i + 2] / 255);

		float sqsum = (lR - cR) * (lR - cR) + (lG - cG) * (lG - cG) + (lB - cB) * (lB - cB);
		mse += sqsum;
	}

	mse = mse / (float)(width * height);

	return mse;
}

float GetDynamicScoreBasedOnMSEGrayscaled(
    const unsigned char* previousFrame,
    const unsigned char* currentFrame,
    int width, int height)
{
    float mse = 0;

    for (int i = 0; i < width * height; ++i) {
        float lR = float(previousFrame[4 * i + 0] / 255);
        float lG = float(previousFrame[4 * i + 1] / 255);
        float lB = float(previousFrame[4 * i + 2] / 255);
        float lY = 0.2125f * lR + 0.7152f * lG + 0.0722f * lB;

        float cR = float(currentFrame[4 * i + 0] / 255);
        float cG = float(currentFrame[4 * i + 1] / 255);
        float cB = float(currentFrame[4 * i + 2] / 255);
        float cY = 0.2125f * cR + 0.7152f * cG + 0.0722f * cB;

        float sqsum = (lY - cY) * (lY - cY);
        mse += sqsum;
    }

    mse = mse / (float)(width * height);

    return mse;
}

float GetDynamicScoreBasedOnSSIM(
	const unsigned char* previousFrame,
	const unsigned char* currentFrame,
	int width, int height)
{
	float meanX = 0, meanY = 0, meanXY = 0.0f;
	float stddevX = 0, stddevY = 0, covXY = 0.0f;

	for (int i = 0; i < width * height; ++i) {
		float lR = previousFrame[4 * i + 0] / 255.0f;
		float lG = previousFrame[4 * i + 1] / 255.0f;
		float lB = previousFrame[4 * i + 2] / 255.0f;
		float lY = 0.2125f * lR + 0.7152f * lG + 0.0722f * lB;
		meanX += lY;
		stddevX += lY * lY;

		float cR = currentFrame[4 * i + 0] / 255.0f;
		float cG = currentFrame[4 * i + 1] / 255.0f;
		float cB = currentFrame[4 * i + 2] / 255.0f;
		float cY = 0.2125f * cR + 0.7152f * cG + 0.0722f * cB;
		meanY += cY;
		stddevY += cY * cY;

		meanXY += lY * cY;
	}

	meanX /= width * height;
	meanY /= width * height;
	stddevX /= width * height;
	stddevY /= width * height;
	meanXY /= width * height;

	stddevX = sqrtf(stddevX - meanX * meanX);
	stddevY = sqrtf(stddevY - meanY * meanY);

	covXY = meanXY - meanX * meanY;

	float k1 = 0.01f, k2 = 0.03f, L = 1.0f;
	float c1 = k1 * k1, c2 = k2 * k2;

	float ssim = (2 * meanX * meanY + c1) * (2 * covXY + c2);
	ssim = ssim / ((meanX*meanX + meanY * meanY + c1) * (stddevX*stddevX + stddevY * stddevY + c2));
	
	return ssim;
}

struct QuadTree
{
    struct Node
    {
        Node* parent = nullptr;
        Node* children[4] = { nullptr, nullptr, nullptr, nullptr };

        bool isFull = false;
        int depth;
    };

    Node* rootNode = nullptr;

    static QuadTree* Create(const std::vector<const BoundingBox2D*>& geometries, const int kMaxDepth);

    ~QuadTree()
    {
        DeleteSubtree(rootNode);

        if (rootNode) {
            delete rootNode;
            rootNode = nullptr;
        }
    }

    void DeleteSubtree(Node* node)
    {
        if (!node)
            return;

        for (auto* child : node->children) {
            DeleteSubtree(child);

            if (child) {
                delete child;
                child = nullptr;
            }
        }
    }
};

QuadTree* QuadTree::Create(const std::vector<const BoundingBox2D*>& geometries, const int kMaxDepth)
{
    QuadTree* res = new QuadTree();

    std::function<void (const BoundingBox2D&, const std::vector<const BoundingBox2D*>&, int, QuadTree::Node*)> addDepth
    = [&](const BoundingBox2D& area, const std::vector<const BoundingBox2D*>& geometries, int depth, QuadTree::Node* parent) {
        auto& Min = area.Min;
        auto& Max = area.Max;
        float halfWidth = area.Width() * 0.5f;
        float halfHeight = area.Height() * 0.5f;
        std::vector<BoundingBox2D> subareas;
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x, Min.y), XMFLOAT2(Min.x + halfWidth, Min.y + halfHeight)));
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x + halfWidth, Min.y), XMFLOAT2(Max.x, Min.y + halfHeight)));
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x, Min.y + halfHeight), XMFLOAT2(Min.x + halfWidth, Max.y)));
        subareas.push_back(BoundingBox2D(XMFLOAT2(Min.x + halfWidth, Min.y + halfHeight), XMFLOAT2(Max.x, Max.y)));

        for (size_t i = 0; i < subareas.size(); ++i) {
            auto& subarea = subareas[i];

            for (auto* geometry : geometries) {
                if (subarea.Intersect(*geometry)) {
                    QuadTree::Node* pNode = new QuadTree::Node();
                    parent->children[i] = pNode;
                    pNode->parent = parent;
                    pNode->isFull = false;
                    pNode->depth = depth;

                    if (depth + 1 < kMaxDepth) {
                        addDepth(subarea, geometries, depth + 1, pNode);
                    }

                    break;
                }
            }
        }

        parent->isFull = parent->children[0] && parent->children[1] && parent->children[2] && parent->children[3];
    };

    BoundingBox2D viewArea(XMFLOAT2(-1, -1), XMFLOAT2(1, 1));
    QuadTree::Node* pRootNode = new QuadTree::Node();
    pRootNode->parent = nullptr;
    pRootNode->isFull = true;
    pRootNode->depth = 0;

    res->rootNode = pRootNode;

    addDepth(viewArea, geometries, 1, res->rootNode);

    return res;
}

static QuadTree* lastQuadTree = nullptr;

float GetDynamicScoreBasedOnQuadtree(const QuadTree::Node* prev, const QuadTree::Node* current)
{
    float sum = 0.0f;

    for (int i = 0; i < 4; ++i) {
        bool prevExist = prev->children[i];
        bool currentExist = current->children[i];

        if (prevExist && currentExist) {
            sum += GetDynamicScoreBasedOnQuadtree(prev->children[i], current->children[i]);
        }
        else if (prevExist != currentExist) {
            assert(prev->depth == current->depth);

            if (prevExist) {
                sum += 1.0f / (4 * prev->children[i]->depth);
            }
            else if (currentExist) {
                sum += 1.0f / (4 * current->children[i]->depth);
            }
        }
    }

	return sum;
}

FrameScalerMain::FrameScalerMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
    m_deviceResources(deviceResources)
{
    m_deviceResources->RegisterDeviceNotify(this);
}

void FrameScalerMain::SetHolographicSpace(HolographicSpace^ holographicSpace)
{
    UnregisterHolographicEventHandlers();

    m_holographicSpace = holographicSpace;

	int N = 8;

	for (int i = 0; i < N; ++i) {
		m_spinningCubeRenderers.push_back(std::make_unique<SpinningCubeRenderer>(m_deviceResources));

		m_spinningCubeRenderers[i]->SetPosition(float3((i - N / 2) / (float)N, 0.0f, -5.0f));
	}

    m_spatialInputHandler = std::make_unique<SpatialInputHandler>();

    m_locator = SpatialLocator::GetDefault();

    m_locatabilityChangedToken =
        m_locator->LocatabilityChanged +=
            ref new Windows::Foundation::TypedEventHandler<SpatialLocator^, Object^>(
                std::bind(&FrameScalerMain::OnLocatabilityChanged, this, _1, _2)
                );

    m_cameraAddedToken =
        m_holographicSpace->CameraAdded +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraAddedEventArgs^>(
                std::bind(&FrameScalerMain::OnCameraAdded, this, _1, _2)
                );

    m_cameraRemovedToken =
        m_holographicSpace->CameraRemoved +=
            ref new Windows::Foundation::TypedEventHandler<HolographicSpace^, HolographicSpaceCameraRemovedEventArgs^>(
                std::bind(&FrameScalerMain::OnCameraRemoved, this, _1, _2)
                );

    m_referenceFrame = m_locator->CreateStationaryFrameOfReferenceAtCurrentLocation();
}

void FrameScalerMain::UnregisterHolographicEventHandlers()
{
    if (m_holographicSpace != nullptr)
    {
        if (m_cameraAddedToken.Value != 0)
        {
            m_holographicSpace->CameraAdded -= m_cameraAddedToken;
            m_cameraAddedToken.Value = 0;
        }

        if (m_cameraRemovedToken.Value != 0)
        {
            m_holographicSpace->CameraRemoved -= m_cameraRemovedToken;
            m_cameraRemovedToken.Value = 0;
        }
    }

    if (m_locator != nullptr)
    {
        m_locator->LocatabilityChanged -= m_locatabilityChangedToken;
    }
}

FrameScalerMain::~FrameScalerMain()
{
    m_deviceResources->RegisterDeviceNotify(nullptr);

    UnregisterHolographicEventHandlers();
}

HolographicFrame^ FrameScalerMain::Update()
{
    HolographicFrame^ holographicFrame = m_holographicSpace->CreateNextFrame();

    HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

    m_deviceResources->EnsureCameraResources(holographicFrame, prediction);

    SpatialCoordinateSystem^ currentCoordinateSystem = m_referenceFrame->CoordinateSystem;

    SpatialInteractionSourceState^ pointerState = m_spatialInputHandler->CheckForInput();
	
	static float timer = 0.0f;

    m_timer.Tick([&] ()
	{
		timer += m_timer.GetElapsedSeconds();

		for (int i = 0; i < m_spinningCubeRenderers.size(); ++i) {
			auto& v = m_spinningCubeRenderers[i];

			auto p = v->GetPosition();

			if (i % 2 == 0) {
				v->SetPosition(float3(p.x, 0.5f * sinf(timer), p.z));
			}
			else {
				v->SetPosition(float3(p.x, 0.5f * -sinf(timer), p.z));
			}

			v->Update(m_timer);
		}
	});

	for (auto cameraPose : prediction->CameraPoses)
	{
		HolographicCameraRenderingParameters^ renderingParameters = holographicFrame->GetRenderingParameters(cameraPose);

		renderingParameters->SetFocusPoint(
			currentCoordinateSystem,
			m_spinningCubeRenderers[0]->GetPosition()
		);
	}

	return holographicFrame;
}

bool FrameScalerMain::Render(Windows::Graphics::Holographic::HolographicFrame^ holographicFrame)
{
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	return m_deviceResources->UseHolographicCameraResources<bool>(
		[this, holographicFrame](std::map<UINT32, std::unique_ptr<DX::CameraResources>>& cameraResourceMap)
	{
		holographicFrame->UpdateCurrentPrediction();
		HolographicFramePrediction^ prediction = holographicFrame->CurrentPrediction;

		bool atLeastOneCameraRendered = false;
		for (auto cameraPose : prediction->CameraPoses)
		{
			DX::CameraResources* pCameraResources = cameraResourceMap[cameraPose->HolographicCamera->Id].get();

			const auto context = m_deviceResources->GetD3DDeviceContext();
			const auto depthStencilView = pCameraResources->GetDepthStencilView();

			ID3D11RenderTargetView *const targets[1] = { pCameraResources->GetBackBufferRenderTargetView() };
			context->OMSetRenderTargets(1, targets, depthStencilView);

			context->ClearRenderTargetView(targets[0], DirectX::Colors::Transparent);
			context->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			pCameraResources->UpdateViewProjectionBuffer(m_deviceResources, cameraPose, m_referenceFrame->CoordinateSystem);

			bool cameraActive = pCameraResources->AttachViewProjectionBuffer(m_deviceResources);

			if (cameraActive)
			{
				for (auto& v : m_spinningCubeRenderers) {
					v->Render();
				}
			}

			float max_distance = -FLT_MAX;

			auto coordinateSystem = m_referenceFrame->CoordinateSystem;
			Platform::IBox<HolographicStereoTransform>^ viewTransformContainer = cameraPose->TryGetViewTransform(coordinateSystem);
			if (!viewTransformContainer)
				return false;
			HolographicStereoTransform viewCoordinateSystemTransform = viewTransformContainer->Value;
			auto viewMatrix = DirectX::XMLoadFloat4x4(&viewCoordinateSystemTransform.Left);

			HolographicStereoTransform cameraProjectionTransform = cameraPose->ProjectionTransform;
			auto projectionMatrix = DirectX::XMLoadFloat4x4(&cameraProjectionTransform.Left);

			for (auto& v : m_spinningCubeRenderers) {
				auto modelMatrix = DirectX::XMLoadFloat4x4(&v->modelMatrix);
				auto boundingBox = v->boundingBox;

				BoundingBox2D boundingBox2D;

				auto vertices = boundingBox.vertices;
				for (int i = 0; i < 8; ++i) {
					XMVECTOR vertex = XMLoadFloat3(&vertices[i]);

					XMFLOAT4 result;
					XMStoreFloat4(&result,
						XMVector3TransformCoord(
							XMVector3TransformCoord(
								XMVector3TransformCoord(vertex, modelMatrix), viewMatrix), projectionMatrix));
					boundingBox2D.AddPoint(result.x, result.y);
				}

				if (boundingBox2D.Min.y > 1 || boundingBox2D.Max.y < -1
					|| boundingBox2D.Min.x > 1 || boundingBox2D.Max.x < -1) {
					v->SetCulled(true);
				}
				else {
					v->SetCulled(false);

					float kReductionLevel2 = 10.0f / 80.0f;
					float kReductionLevel1 = 30.0f / 80.0f;

					if (boundingBox2D.Min.y > kReductionLevel2 || boundingBox2D.Max.x < -kReductionLevel2
						|| boundingBox2D.Min.x > kReductionLevel2 || boundingBox2D.Max.x < -kReductionLevel2) {
						v->SetReductionLevel(2);
					}
					else if (boundingBox2D.Min.y > kReductionLevel1 || boundingBox2D.Max.x < -kReductionLevel1
						|| boundingBox2D.Min.x > kReductionLevel1 || boundingBox2D.Max.x < -kReductionLevel1) {
						v->SetReductionLevel(1);
					}
					else {
						v->SetReductionLevel(0);
					}

					auto prev = XMLoadFloat2(&v->lastProjectedPosition);
					auto current = XMLoadFloat2(&boundingBox2D.mid());

					XMVECTOR vD = XMVector2LengthSq(prev - current);
					float d;
					XMStoreFloat(&d, vD);

					if (max_distance < d) {
						max_distance = d;
					}

				}
			}

			atLeastOneCameraRendered = true;

			constexpr float level1 = 0.01f / 120.0f;
			constexpr float level2 = 0.005f / 120.0f;

			auto* frameController = FramerateController::get();

			int currentFramerate = frameController->GetFramerate();
			if (currentFramerate > 59) {
				if (max_distance < level1) {
					frameController->SetFramerate(30);
				}
			}
			else if (currentFramerate > 29) {
				if (max_distance > level1) {
					frameController->SetFramerate(60);
				}
				else if (max_distance < level2) {
					frameController->SetFramerate(15);
				}
			}
			else {
				if (max_distance > level2) {
					frameController->SetFramerate(30);
				}
			}
		}

		return atLeastOneCameraRendered;
	});
}

void FrameScalerMain::SaveAppState()
{
}

void FrameScalerMain::LoadAppState()
{
}

void FrameScalerMain::OnDeviceLost()
{
	for (auto& v : m_spinningCubeRenderers) {
		v->ReleaseDeviceDependentResources();
	}
}

void FrameScalerMain::OnDeviceRestored()
{
	for (auto& v : m_spinningCubeRenderers) {
		v->CreateDeviceDependentResources();
	}
}

void FrameScalerMain::OnLocatabilityChanged(SpatialLocator^ sender, Object^ args)
{
	switch (sender->Locatability)
	{
	case SpatialLocatability::Unavailable:
	{
		String^ message = L"Warning! Positional tracking is " +
			sender->Locatability.ToString() + L".\n";
		OutputDebugStringW(message->Data());
	}
	break;

	case SpatialLocatability::PositionalTrackingActivating:

	case SpatialLocatability::OrientationOnly:

	case SpatialLocatability::PositionalTrackingInhibited:
		break;

	case SpatialLocatability::PositionalTrackingActive:
		break;
	}
}

void FrameScalerMain::OnCameraAdded(
	HolographicSpace^ sender,
	HolographicSpaceCameraAddedEventArgs^ args
)
{
	Deferral^ deferral = args->GetDeferral();
	HolographicCamera^ holographicCamera = args->Camera;
	create_task([this, deferral, holographicCamera]()
	{
		m_deviceResources->AddHolographicCamera(holographicCamera);

		deferral->Complete();
	});
}

void FrameScalerMain::OnCameraRemoved(
	HolographicSpace^ sender,
	HolographicSpaceCameraRemovedEventArgs^ args
)
{
	create_task([this]()
	{
	});

	m_deviceResources->RemoveHolographicCamera(args->Camera);
}
