//***************************************************************************************ObjectCB
// TexColumnsApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"

#include "../../Common/imgui.h"
#include "../../Common/imgui_impl_dx12.h"
#include "../../Common/imgui_impl_win32.h"

#include "../../Common/Camera.h"
#include "FrameResource.h"
#include "Terrain.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 6;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();


	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;
	MeshGeometry* mDebugGeo = nullptr;
	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};


class TexColumnsApp : public D3DApp
{
public:
	TexColumnsApp(HINSTANCE hInstance);
	TexColumnsApp(const TexColumnsApp& rhs) = delete;
	TexColumnsApp& operator=(const TexColumnsApp& rhs) = delete;
	~TexColumnsApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateBrushCB(const GameTimer& gt);

	void LoadDDSTexture(std::string name, std::wstring filename);
	void LoadTextures();
	void CreateBrushTexture(CD3DX12_CPU_DESCRIPTOR_HANDLE baseDescriptorHandle, int baseOffset);
	void BuildRootSignature();

	void BuildCsRootSignature();
	void BuildTerrainRootSignature();
	void BuildDebugRootSignature();


	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildDebugGeometry();

	//void CreateBoundingBoxMesh(const BoundingBox& bbox, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices);
	//void BuildDebugGeometry();
	//void RenderBoundingBoxes();

	void GenerateTileGeometry(const XMFLOAT3& worldPos, float tileSize, int lodLevel, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices);
	void BuildTerrainGeometry();
	void UpdateTerrain(const GameTimer& gt);
	void InitTerrain();
	void UpdateTerrainCBs(const GameTimer& gt);

	void BuildPSOs();
	void BuildFrameResources();
	void CreateMaterial(std::string _name, int _CBIndex, int _SRVDiffIndex, int _SRVNMapIndex, int _SRVDispIndex, XMFLOAT4 _DiffuseAlbedo, XMFLOAT3 _FresnelR0, float _Roughness);
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawTilesRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<Tile*>& tiles);

	void InitImGui();
	void SetupImGui();

	bool ScreenToWorld(int screenX, int screenY, XMFLOAT3& worldPos);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	XMMATRIX mInvViewProj;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;
	//
	std::unordered_map<std::string, int>TexOffsets;
	//
	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mTerrainRootSignature = nullptr;

	// Compute Shader PSO
	//Microsoft::WRL::ComPtr<ID3D12PipelineState> mBrushComputePSO;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mBrushComputeRootSignature;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mDebugRootSignature;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBrushTexture;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBrushTextureUpload;
	D3D12_CPU_DESCRIPTOR_HANDLE mBrushTextureSRV;
	D3D12_CPU_DESCRIPTOR_HANDLE mBrushTextureUAV;

	int mBrushTextureSRVIndex = 0;
	int mBrushTextureUAVIndex = 0;
	UINT mBrushTextureWidth = 1024;
	UINT mBrushTextureHeight = 1024;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mImGuiSrvDescriptorHeap;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mDebugGeometries;
	
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mDebugInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;


	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	PassConstants mMainPassCB;
	BrushConstants mBrushCB;

	POINT mLastMousePos;

	Camera mCamera;

	bool isFillModeSolid = true;
	bool showTilesBoundingBox=false;

	float mScale = 1.f;
	float mTessellationFactor = 1.f;

	std::unique_ptr<Terrain> mTerrain;
	int mMaxLOD = 5;
	XMFLOAT3 terrainPos = XMFLOAT3(0.f, -100, 0.f);
	XMFLOAT3 terrainOffset = XMFLOAT3(0.f, 0.f, 0.f);
	float mTerrainSize = 1024;
	std::vector<Tile*> mVisibleTiles;

	float mCameraVertSpeed = 400;
	float mCameraHorSpeed = 400;

	int controlMode = 0; //0 - camera, 1 - terrain brush 

	//XMFLOAT3 BrushWPos;
	XMFLOAT4 BrushColor = {1.f, 1.f, 1.f, 1.f};
	float BrushRadius=30;
	float BrushFalloffRadius=40;
	int mIsPainting = 0;
	//bool mShowDebugTexture = true;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		TexColumnsApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

TexColumnsApp::TexColumnsApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

TexColumnsApp::~TexColumnsApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool TexColumnsApp::Initialize()
{

	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.SetPosition(0.0f, 300.0f, 0.f);

	LoadTextures();
	//CreateBrushTexture();
	BuildRootSignature();
	BuildTerrainRootSignature();
	BuildCsRootSignature();
	BuildDescriptorHeaps();
	//BuildDebugRootSignature();

	InitTerrain();

	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	//BuildDebugGeometry();

	BuildTerrainGeometry();

	BuildMaterials();
	BuildPSOs();
	BuildRenderItems();
	BuildFrameResources();


	InitImGui();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}
void TexColumnsApp::InitTerrain()
{
	mTerrain = std::make_unique<Terrain>();
	mTerrain->Initialize(md3dDevice.Get(), mTerrainSize, mMaxLOD, terrainPos);
}

void TexColumnsApp::InitImGui()
{
	D3D12_DESCRIPTOR_HEAP_DESC imGuiHeapDesc = {};
	imGuiHeapDesc.NumDescriptors = 1;
	imGuiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imGuiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	imGuiHeapDesc.NodeMask = 0; // Or the appropriate node mask if you have multiple GPUs
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&imGuiHeapDesc, IID_PPV_ARGS(&mImGuiSrvDescriptorHeap)));
	// INITIALIZE IMGUI ////////////////////
		IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = md3dDevice.Get();
	init_info.CommandQueue = mCommandQueue.Get();
	init_info.NumFramesInFlight = gNumFrameResources;
	init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // Or your render target format.
	init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
	init_info.SrvDescriptorHeap = mImGuiSrvDescriptorHeap.Get();
	init_info.LegacySingleSrvCpuDescriptor = mImGuiSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	init_info.LegacySingleSrvGpuDescriptor = mImGuiSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	ImGui_ImplWin32_Init(mhMainWnd);
	ImGui_ImplDX12_Init(&init_info);
	////////////////////////////////////////
}

void TexColumnsApp::OnResize()
{
	D3DApp::OnResize();


	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, mCamera.cameraFarZ);

}

void TexColumnsApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// Cycle through the circular frame resource array.

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	SetupImGui();

	UpdateTerrain(gt);

	AnimateMaterials(gt);

	UpdateObjectCBs(gt);
	UpdateTerrainCBs(gt);

	UpdateMainPassCB(gt);
	UpdateBrushCB(gt);
	UpdateMaterialCBs(gt);
}

void TexColumnsApp::SetupImGui()
{
	// === ImGui Setup ===
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Settings");

	ImGui::Text("Polygon Fill Mode:");

	if (ImGui::RadioButton("Solid", isFillModeSolid)) isFillModeSolid = true;
	ImGui::SameLine();
	if (ImGui::RadioButton("Wireframe", !isFillModeSolid)) isFillModeSolid = false;

	ImGui::Separator();

	ImGui::Text("Camera speed:");
	XMFLOAT2 speed = XMFLOAT2(mCameraHorSpeed, mCameraVertSpeed);
	ImGui::DragFloat2("##Speed:", &speed.x, 1.f, 10.f, 9999.f, "%.1f");
	mCameraHorSpeed = speed.x;
	mCameraVertSpeed = speed.y;

	ImGui::Separator();

	ImGui::Text("Tesselation:");
	ImGui::Text("Scale:");
	ImGui::DragFloat("##Scale", &mScale, 0.1f, 0.0f, 10, "%.3f");
	ImGui::Text("Tessellation Factor:");
	ImGui::DragFloat("##TessFactor", &mTessellationFactor, 1.f, 1.f, 32, "%.1f");

	ImGui::Separator();

	ImGui::Text("Terrain:");
	ImGui::Text("Visible Tiles: %d", mTerrain->GetVisibleTiles().size());
	ImGui::Text("Total Tiles: %d", mTerrain->GetAllTiles().size());
	//ImGui::DragFloat3("Terrain offset", &terrainOffset.x, 1.0f, -1000.0f, 1000.0f);
	ImGui::Checkbox("Show Bounding Box", &showTilesBoundingBox);
	//ImGui::Checkbox("Show Debug Texture", &mShowDebugTexture);
	
	ImGui::SetWindowSize(ImVec2(300, 500)); // ������, ������
	ImGui::SetWindowPos(ImVec2(5, 5));   // X, Y �������

	ImGui::Separator();

	ImGui::Text("Control mode:");

	ImGui::RadioButton("Camera", &controlMode, 0); ImGui::SameLine();
	ImGui::RadioButton("Terrain Brush", &controlMode, 1); //ImGui::SameLine();
	//ImGui::RadioButton("radio c", &e, 2);

	ImGui::Separator();

	ImGui::Text("Brush settings:");
	ImGui::SliderFloat("Radius", &BrushRadius, 1.f, 500.f, "%.1f");
	ImGui::SliderFloat("Falloff radius", &BrushFalloffRadius, 0.f, 500.f, "%.1f");
	//ImGui::DragFloat("Radius", &BrushRadius, 1.f, 1.f, 500.f, "%.1f");
	//ImGui::DragFloat("Falloff radius", &BrushFalloffRadius, 1.f, 0.f, 500.f, "%.1f");
	static float col[4] = { 1.f, 1.f, 1.f, 1.f };
	ImGui::ColorEdit4("Color", col);
	BrushColor = {col[0], col[1], col[2], col[3]};

	ImGui::End();


}

void TexColumnsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	if ((btnState & MK_LBUTTON) != 0)
	{
		//do left click
	}

	SetCapture(mhMainWnd);
}

void TexColumnsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void TexColumnsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (!ImGui::GetIO().WantCaptureMouse)
	{
		if ((btnState & MK_RBUTTON) != 0)
		{
			// Make each pixel correspond to a quarter of a degree.
			float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

			mCamera.Pitch(dy);
			mCamera.RotateY(dx);
		}
		//Control mode HERE
		switch (controlMode)
		{
		case 0:

			break;
		case 1:
			if ((btnState & MK_RBUTTON) == 0)
			{
				mIsPainting = (btnState & MK_LBUTTON) == 0 ? 0 : 1;
				XMFLOAT3 worldPos;
				if (ScreenToWorld(x, y, worldPos))
				{
					mBrushCB.BrushWPos = worldPos;
					// Отладочный вывод выбранной позиции
					std::string debugMsg = "Decal placed at: X=" +
						std::to_string(worldPos.x) +
						", Y=" + std::to_string(worldPos.y) +
						", Z=" + std::to_string(worldPos.z) +
						"\n";
					OutputDebugStringA(debugMsg.c_str());
				}
				else
				{
					OutputDebugStringA("Decal hidden: click missed the plane\n");
				}


			}
			break;
		default:
			break;
		}
		

		mLastMousePos.x = x;
		mLastMousePos.y = y;
	
	}
}

void TexColumnsApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

	if (GetAsyncKeyState('W') & 0x8000)
	{
		if (shiftPressed) mCamera.MoveUp(mCameraVertSpeed * dt);
		else mCamera.Walk(mCameraHorSpeed * dt);
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		if (shiftPressed) mCamera.MoveUp(-mCameraVertSpeed * dt);
		else mCamera.Walk(-mCameraHorSpeed * dt);
	}

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-mCameraHorSpeed * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(mCameraHorSpeed * dt);

	mCamera.UpdateViewMatrix();
}


bool TexColumnsApp::ScreenToWorld(int screenX, int screenY, XMFLOAT3& worldPos)
{
	// 1. Нормализованные координаты экрана [-1, 1]
	float nx = (2.0f * static_cast<float>(screenX)) / mClientWidth - 1.0f;
	float ny = 1.0f - (2.0f * static_cast<float>(screenY)) / mClientHeight;

	// 2. Создаем луч в clip space
	XMVECTOR rayStart = XMVectorSet(nx, ny, 0.0f, 1.0f);
	XMVECTOR rayEnd = XMVectorSet(nx, ny, 1.0f, 1.0f);

	// 3. Получаем обратную матрицу ViewProj
	//XMMATRIX viewProj = XMMatrixMultiply(mCamera.GetView(), mCamera.GetProj());
	XMMATRIX invViewProj = mInvViewProj;//XMMatrixInverse(nullptr, viewProj);

	// 4. Преобразуем в мировые координаты
	rayStart = XMVector3TransformCoord(rayStart, invViewProj);
	rayEnd = XMVector3TransformCoord(rayEnd, invViewProj);

	XMFLOAT3 startPos, endPos;
	XMStoreFloat3(&startPos, rayStart);
	XMStoreFloat3(&endPos, rayEnd);

	// 5. Направление луча
	XMVECTOR rayDir = XMVectorSubtract(rayEnd, rayStart);
	rayDir = XMVector3Normalize(rayDir);
	
	// Проверяем пересечение луча с AABB террейна
	float distance;

	std::string debugMsg;;

	XMVECTOR planeNormal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR planePoint = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	XMVECTOR diff = XMVectorSubtract(planePoint, rayStart);
	float numerator = XMVectorGetX(XMVector3Dot(diff, planeNormal));
	float denominator = XMVectorGetX(XMVector3Dot(rayDir, planeNormal));

	debugMsg = "Plane intersection calc: numerator=" + std::to_string(numerator) +
		", denominator=" + std::to_string(denominator) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	if (fabs(denominator) > 1e-6f)
	{
		float t = numerator / denominator;

		if (t >= 0.0f)
		{
			XMVECTOR intersection = XMVectorAdd(rayStart, XMVectorScale(rayDir, t));
			XMFLOAT3 intersectPos;
			XMStoreFloat3(&intersectPos, intersection);

			// Проверяем границы террейна в плоскости XZ
			if (intersectPos.x >= terrainPos.x && intersectPos.x <= terrainPos.x + mTerrainSize &&
				intersectPos.z >= terrainPos.z && intersectPos.z <= terrainPos.z + mTerrainSize)
			{
				worldPos = intersectPos;
				debugMsg = "HIT: Projected to terrain plane at Y=" + std::to_string(intersectPos.y) + "\n";
				OutputDebugStringA(debugMsg.c_str());
				return true;
			}
		}
	}

	OutputDebugStringA("MISS: No intersection with terrain bounds\n");
	return false;
}

void TexColumnsApp::AnimateMaterials(const GameTimer& gt)
{

}

void TexColumnsApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);


			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));


			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void TexColumnsApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void TexColumnsApp::UpdateMainPassCB(const GameTimer& gt)
{

	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	if (XMMatrixIsIdentity(view) || XMMatrixIsIdentity(proj)) {
		OutputDebugStringA("WARNING: View or projection matrix is identity!\n");
	}



	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    mInvViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(mInvViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = mCamera.cameraFarZ;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	mMainPassCB.gScale = mScale;
	mMainPassCB.gTessellationFactor = mTessellationFactor;



	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}
void TexColumnsApp::UpdateBrushCB(const GameTimer& gt)
{
	mBrushCB.BrushRadius = BrushRadius;
	mBrushCB.BrushFalofRadius = BrushFalloffRadius;
	mBrushCB.BrushColors = BrushColor;
	mBrushCB.isBrushMode = controlMode == 1 ? 1 : 0;
	mBrushCB.isPainting = mIsPainting == 1 ? 1 : 0;

	auto currBrushCB = mCurrFrameResource->BrushCB.get();
	currBrushCB->CopyData(0, mBrushCB);
}

void TexColumnsApp::UpdateTerrainCBs(const GameTimer& gt)
{
	auto currTileCB = mCurrFrameResource->TerrainCB.get();
	for (auto& tile : mTerrain->GetAllTiles())
	{
	    TileConstants tileConstants;

		tileConstants.TilePosition = tile->worldPos;
		tileConstants.gTerrainOffset = terrainOffset;
		tileConstants.TileSize = tile->tileSize;
		tileConstants.mapSize = mTerrain->mWorldSize;
		tileConstants.hScale = mTerrain->mHeightScale;

		tileConstants.showBoundingBox = showTilesBoundingBox ? 1 : 0;

		currTileCB->CopyData(tile->tileIndex, tileConstants);

		tile->NumFramesDirty--;
	}
}

void TexColumnsApp::UpdateTerrain(const GameTimer& gt)
{
	if (!mTerrain)
		return;


	mTerrain->Update(mCamera.GetPosition3f(), mCamera.GetFrustum());
	//mTerrain->UpdateBoundainBoxes(terrainOffset);
	//auto& visibleTiles = mTerrain->GetVisibleTiles();
	//for (auto& tile : visibleTiles)
	//{
	//	auto& rItem = mAllRitems.at(tile->renderItemIndex);

	//}
}

void TexColumnsApp::LoadDDSTexture(std::string name, std::wstring filename)
{
	auto tex = std::make_unique<Texture>();
	tex->Filename = filename;
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tex->Filename.c_str(),
		tex->Resource, tex->UploadHeap));
	mTextures[name] = std::move(tex);
}

void TexColumnsApp::LoadTextures()
{
	LoadDDSTexture("stoneTex", L"../../Textures/stone.dds");
	LoadDDSTexture("stoneNorm", L"../../Textures/stone_nmap.dds");
	LoadDDSTexture("stonetDisp", L"../../Textures/stone_disp.dds");

	LoadDDSTexture("terrainDiff", L"../../Textures/terrain_diff.dds");
	LoadDDSTexture("terrainNorm", L"../../Textures/terrain_norm.dds");
	LoadDDSTexture("terrainDisp", L"../../Textures/terrain_disp.dds");
}



void TexColumnsApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE diffuseRange;
	diffuseRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // ��������� �������� � �������� t0

	CD3DX12_DESCRIPTOR_RANGE normalRange;
	normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);  // ���������� ����� � �������� t1

	CD3DX12_DESCRIPTOR_RANGE dispMap;
	dispMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);  // Dispmap � �������� t2

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[6];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &diffuseRange, D3D12_SHADER_VISIBILITY_ALL);      // t0
	slotRootParameter[1].InitAsDescriptorTable(1, &normalRange, D3D12_SHADER_VISIBILITY_ALL);       // t1  
	slotRootParameter[2].InitAsDescriptorTable(1, &dispMap, D3D12_SHADER_VISIBILITY_ALL);           // t2

	slotRootParameter[3].InitAsConstantBufferView(0); // b0 
	slotRootParameter[4].InitAsConstantBufferView(1); // b1 
	slotRootParameter[5].InitAsConstantBufferView(2); // b2 

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

//TERREAIN ROOT SIGNATURE
void TexColumnsApp::BuildTerrainRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE TerrainDiffuseRange;
	TerrainDiffuseRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // Terrain diff t0

	CD3DX12_DESCRIPTOR_RANGE TerrainNormalRange;
	TerrainNormalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);  // Terrain norm t1

	CD3DX12_DESCRIPTOR_RANGE TerrainDispMap;
	TerrainDispMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);  // Terrain Disp (HeightMap) t2

	// ===== ДОБАВЛЕНО: SRV для текстуры кисти =====
	CD3DX12_DESCRIPTOR_RANGE BrushTextureRange;
	BrushTextureRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);  // Brush texture t3

	// УВЕЛИЧИТЬ массив до 9 параметров (было 8)
	CD3DX12_ROOT_PARAMETER slotRootParameter[9];

	// SRV текстуры
	slotRootParameter[0].InitAsDescriptorTable(1, &TerrainDiffuseRange, D3D12_SHADER_VISIBILITY_PIXEL);  // t0
	slotRootParameter[1].InitAsDescriptorTable(1, &TerrainNormalRange, D3D12_SHADER_VISIBILITY_PIXEL);   // t1
	slotRootParameter[2].InitAsDescriptorTable(1, &TerrainDispMap, D3D12_SHADER_VISIBILITY_ALL);         // t2
	slotRootParameter[3].InitAsDescriptorTable(1, &BrushTextureRange, D3D12_SHADER_VISIBILITY_PIXEL);    // t3 <-- НОВОЕ

	// CBV
	slotRootParameter[4].InitAsConstantBufferView(0); // b0 - cbPerObject
	slotRootParameter[5].InitAsConstantBufferView(1); // b1 - cbPass
	slotRootParameter[6].InitAsConstantBufferView(2); // b2 - cbMaterial
	slotRootParameter[7].InitAsConstantBufferView(3); // b3 - cbTerrainTile
	slotRootParameter[8].InitAsConstantBufferView(4); // b4 - cbBrush

	auto staticSamplers = GetStaticSamplers();

	// Увеличить количество параметров до 9
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(9, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mTerrainRootSignature.GetAddressOf())));

	OutputDebugStringA("Terrain root signature created with brush texture slot (t3)\n");
}

void TexColumnsApp::BuildCsRootSignature()
{
	// 1. SRV для карты высот (t0)
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

	// 2. UAV для текстуры кисти (u0)
	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

	// 3. Root параметры
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// b0: BrushCB
	slotRootParameter[0].InitAsConstantBufferView(0);

	// b1: TerrainCB  
	slotRootParameter[1].InitAsConstantBufferView(1);

	// t0: Карта высот (SRV)
	slotRootParameter[2].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_ALL);

	// u0: Текстура кисти (UAV)
	slotRootParameter[3].InitAsDescriptorTable(1, &uavTable, D3D12_SHADER_VISIBILITY_ALL);

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig, &errorBlob);

	if (errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());

	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mBrushComputeRootSignature.GetAddressOf())));
}

/*void TexColumnsApp::BuildDebugRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

	// Сэмплер
	CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // register 0
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter,
		1, &pointClamp,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSig, &errorBlob);

	if (errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());

	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mDebugRootSignature.GetAddressOf())));
}*/

void TexColumnsApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mTextures.size() + 2; // Все текстуры + SRV кисти + UAV кисти
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	char msg[256];
	sprintf_s(msg, "Building descriptor heap with %d descriptors\n", srvHeapDesc.NumDescriptors);
	OutputDebugStringA(msg);

	// Вывод информации о текстурах перед созданием
	sprintf_s(msg, "Total textures in mTextures: %zu\n", mTextures.size());
	OutputDebugStringA(msg);

	int texIndex = 0;
	for (const auto& tex : mTextures) {
		sprintf_s(msg, "Texture %d: %s (has resource: %s)\n",
			texIndex++,
			tex.first.c_str(),
			(tex.second && tex.second->Resource) ? "YES" : "NO");
		OutputDebugStringA(msg);
	}

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	int offset = 0;

	// Создаем дескрипторы для всех обычных текстур
	OutputDebugStringA("\nCreating SRV descriptors:\n");
	for (const auto& tex : mTextures) {
		if (!tex.second || !tex.second->Resource) {
			sprintf_s(msg, "Skipping texture %s: missing resource\n", tex.first.c_str());
			OutputDebugStringA(msg);
			continue;
		}

		auto text = tex.second->Resource;
		srvDesc.Format = text->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = text->GetDesc().MipLevels;

		// Сохраняем информацию о создаваемом дескрипторе
		sprintf_s(msg, "  Offset %d: %s (Format: %d, Mips: %d)\n",
			offset, tex.first.c_str(), srvDesc.Format, srvDesc.Texture2D.MipLevels);
		OutputDebugStringA(msg);

		md3dDevice->CreateShaderResourceView(text.Get(), &srvDesc, hDescriptor);

		TexOffsets[tex.first] = offset;
		offset++;

		// Смещаем дескриптор только после создания
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	}

	// Создаем текстуру кисти после всех обычных текстур
	OutputDebugStringA("\nCreating brush texture descriptors:\n");
	CreateBrushTexture(hDescriptor, offset);

	// === ВЫВОД ИНФОРМАЦИИ О СОЗДАННОЙ КУЧЕ ===
	OutputDebugStringA("\n=== DESCRIPTOR HEAP SUMMARY ===\n");

	// 1. Вывод всех TexOffsets
	sprintf_s(msg, "TexOffsets map contains %zu entries:\n", TexOffsets.size());
	OutputDebugStringA(msg);

	for (const auto& offsetEntry : TexOffsets) {
		sprintf_s(msg, "  %s -> index %d\n", offsetEntry.first.c_str(), offsetEntry.second);
		OutputDebugStringA(msg);
	}

	// 2. Проверка наличия критических текстур террейна
	OutputDebugStringA("\nCritical terrain textures check:\n");

	const char* terrainTextures[] = { "terrainDiff", "terrainNorm", "terrainDisp" };
	for (const char* texName : terrainTextures) {
		if (TexOffsets.find(texName) == TexOffsets.end()) {
			sprintf_s(msg, "  WARNING: %s NOT FOUND in TexOffsets!\n", texName);
			OutputDebugStringA(msg);
		}
		else {
			sprintf_s(msg, "  OK: %s found at index %d\n", texName, TexOffsets[texName]);
			OutputDebugStringA(msg);
		}
	}

	// 3. Проверка индексов кисти
	OutputDebugStringA("\nBrush texture indices:\n");
	if (mBrushTextureSRVIndex >= 0) {
		sprintf_s(msg, "  Brush SRV index: %d\n", mBrushTextureSRVIndex);
		OutputDebugStringA(msg);
	}
	else {
		OutputDebugStringA("  WARNING: Brush SRV index not set!\n");
	}

	if (mBrushTextureUAVIndex >= 0) {
		sprintf_s(msg, "  Brush UAV index: %d\n", mBrushTextureUAVIndex);
		OutputDebugStringA(msg);
	}
	else {
		OutputDebugStringA("  WARNING: Brush UAV index not set!\n");
	}

	// 4. Вывод дескрипторного размера
	sprintf_s(msg, "\nDescriptor size: %u bytes\n", mCbvSrvDescriptorSize);
	OutputDebugStringA(msg);

	// 5. Проверка связности индексов
	OutputDebugStringA("\nIndex continuity check:\n");
	if (offset != mTextures.size()) {
		sprintf_s(msg, "  WARNING: offset (%d) != mTextures.size() (%zu)\n",
			offset, mTextures.size());
		OutputDebugStringA(msg);
	}
	else {
		sprintf_s(msg, "  OK: offset matches texture count\n");
		OutputDebugStringA(msg);
	}

	sprintf_s(msg, "Total descriptors created: %d\n", offset + 2); // +2 для SRV и UAV кисти
	OutputDebugStringA(msg);

	OutputDebugStringA("=== END DESCRIPTOR HEAP SUMMARY ===\n\n");
}

void TexColumnsApp::CreateBrushTexture(CD3DX12_CPU_DESCRIPTOR_HANDLE baseDescriptorHandle, int baseOffset)
{
	// 1. Создаем текстуру с UAV флагом
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mBrushTextureWidth;
	texDesc.Height = mBrushTextureHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mBrushTexture)));

	// 2. Создаем SRV для текстуры кисти
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	// SRV идет сразу после всех обычных текстур
	mBrushTextureSRV = baseDescriptorHandle;
	md3dDevice->CreateShaderResourceView(mBrushTexture.Get(), &srvDesc, mBrushTextureSRV);

	// 3. Смещаем дескриптор для UAV
	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle = baseDescriptorHandle;
	uavHandle.Offset(1, mCbvSrvDescriptorSize);

	// 4. Создаем UAV для текстуры кисти
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = texDesc.Format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;

	mBrushTextureUAV = uavHandle;
	md3dDevice->CreateUnorderedAccessView(mBrushTexture.Get(), nullptr, &uavDesc, mBrushTextureUAV);

	// 5. Добавляем текстуру в mTextures для управления ресурсами
	auto brushTex = std::make_unique<Texture>();
	brushTex->Name = "brushCanvas";
	brushTex->Resource = mBrushTexture;
	brushTex->Filename = L"";

	// Сохраняем индексы для быстрого доступа
	mBrushTextureSRVIndex = baseOffset;
	mBrushTextureUAVIndex = baseOffset + 1;

	mTextures["brushCanvas"] = std::move(brushTex);

	// 6. Инициализируем текстуру черным цветом
	//InitializeBrushTexture();
}


void TexColumnsApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["opaqueVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaqueHS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "HS", "hs_5_1");
	mShaders["opaqueDS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "DS", "ds_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["wirePS"] = d3dUtil::CompileShader(L"Shaders\\Wireframe.hlsl", nullptr, "WirePS", "ps_5_1");

	mShaders["terrainVS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["terrainPS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["wireTerrPS"] = d3dUtil::CompileShader(L"Shaders\\Terrain.hlsl", nullptr, "WirePS", "ps_5_1"); 

	mShaders["brushCS"] = d3dUtil::CompileShader(L"Shaders\\Brush.hlsl", nullptr, "BrushCS", "cs_5_1");

	//mShaders["debugVS"] = d3dUtil::CompileShader(L"Shaders\\Debug.hlsl", nullptr, "VS", "vs_5_1");
	//mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\Debug.hlsl", nullptr, "PS", "ps_5_1");

	//mShaders["debugVS"] = d3dUtil::CompileShader(L"Shaders\\Debug.hlsl", nullptr, "VS", "vs_5_1");
	//mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\Debug.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	/*mDebugInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	}*/;

	//debugInputLayout =
	//{
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//};
}

void TexColumnsApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaqueVS"]->GetBufferPointer()),
		mShaders["opaqueVS"]->GetBufferSize()
	};
	opaquePsoDesc.HS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaqueHS"]->GetBufferPointer()),
		mShaders["opaqueHS"]->GetBufferSize()
	};
	opaquePsoDesc.DS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaqueDS"]->GetBufferPointer()),
		mShaders["opaqueDS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;//D3D12_FILL_MODE_SOLID; //D3D12_FILL_MODE_WIREFRAME; //wire solid

	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	//PSO for WIREFRAME

	D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePsoDesc = opaquePsoDesc;
	wireframePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["wirePS"]->GetBufferPointer()),
		mShaders["wirePS"]->GetBufferSize()
	};
	wireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&wireframePsoDesc, IID_PPV_ARGS(&mPSOs["wireframe"])));

	OutputDebugStringA("=== Building PSOs ===\n");

	OutputDebugStringA("Creating Compute PSO...\n");
	// 2. Compute PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc;
	ZeroMemory(&computePsoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));

	computePsoDesc.pRootSignature = mBrushComputeRootSignature.Get();

	computePsoDesc.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["brushCS"]->GetBufferPointer()),
		mShaders["brushCS"]->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	computePsoDesc.NodeMask = 0;

	ThrowIfFailed(md3dDevice->CreateComputePipelineState(
		&computePsoDesc, IID_PPV_ARGS(&mPSOs["brushCompute"]) ));

	//
	// PSO for Terrain.
	//
	OutputDebugStringA("Creating Terrain PSO...\n");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC terrainPsoDesc;

	ZeroMemory(&terrainPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	terrainPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	terrainPsoDesc.pRootSignature = mTerrainRootSignature.Get();
	terrainPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["terrainVS"]->GetBufferPointer()),
		mShaders["terrainVS"]->GetBufferSize()
	};
	terrainPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["terrainPS"]->GetBufferPointer()),
		mShaders["terrainPS"]->GetBufferSize()
	};
	terrainPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	terrainPsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	terrainPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	terrainPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	terrainPsoDesc.SampleMask = UINT_MAX;
	terrainPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	terrainPsoDesc.NumRenderTargets = 1;
	terrainPsoDesc.RTVFormats[0] = mBackBufferFormat;
	terrainPsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	terrainPsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	terrainPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&terrainPsoDesc, IID_PPV_ARGS(&mPSOs["terrain"])));

	//PSO for wireframe terrain
	OutputDebugStringA("Creating Wire Terrain PSO...\n");
	D3D12_GRAPHICS_PIPELINE_STATE_DESC terrWirePsoDesc = terrainPsoDesc;
	terrWirePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["wireTerrPS"]->GetBufferPointer()),
		mShaders["wireTerrPS"]->GetBufferSize()
	};
	terrWirePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&terrWirePsoDesc, IID_PPV_ARGS(&mPSOs["wireTerrain"])));

	//
	// Дебаг PSO
	//
	/*OutputDebugStringA("Creating Debug PSO...\n");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = {};
	debugPsoDesc.InputLayout = { mDebugInputLayout.data(), (UINT)mDebugInputLayout.size() };
	debugPsoDesc.pRootSignature = mDebugRootSignature.Get();
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	debugPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	debugPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	debugPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	debugPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	debugPsoDesc.DepthStencilState.DepthEnable = FALSE; // Без глубины для дебага
	debugPsoDesc.DepthStencilState.StencilEnable = FALSE;
	debugPsoDesc.SampleMask = UINT_MAX;
	debugPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	debugPsoDesc.NumRenderTargets = 1;
	debugPsoDesc.RTVFormats[0] = mBackBufferFormat;
	debugPsoDesc.SampleDesc.Count = 1;
	debugPsoDesc.SampleDesc.Quality = 0;
	debugPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debugQuad"])));*/
	//PSO for debug
	/*D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPSODesc = {};
	debugPSODesc.pRootSignature = mTerrainRootSignature.Get(); // �� ������ root signature

	// Shaders
	debugPSODesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPSODesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};

	// Input Layout (������ �������)
	debugPSODesc.InputLayout = { debugInputLayout.data(), (UINT)debugInputLayout.size() };
	debugPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	// Rasterizer - ������ ����� �����������
	debugPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	debugPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	debugPSODesc.RasterizerState.DepthClipEnable = TRUE;

	// Blend - ��������� ������������
	debugPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	// Depth - ������ �� �� �����
	debugPSODesc.DepthStencilState.DepthEnable = TRUE;
	debugPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	debugPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// Formats
	debugPSODesc.SampleMask = UINT_MAX;
	debugPSODesc.SampleDesc.Count = 1;
	debugPSODesc.NumRenderTargets = 1;
	debugPSODesc.RTVFormats[0] = mBackBufferFormat;
	debugPSODesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPSODesc, IID_PPV_ARGS(&mPSOs["debug"])));*/
}

void TexColumnsApp::BuildFrameResources()
{
	FlushCommandQueue();
	mFrameResources.clear();
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), (UINT)mTerrain->GetAllTiles().size(), 1
		));
	}
	mCurrFrameResourceIndex = 0;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	for (auto& ri : mAllRitems)
	{
		ri->NumFramesDirty = gNumFrameResources;
	}
	for (auto& kv : mMaterials)
	{
		kv.second->NumFramesDirty = gNumFrameResources;
	}
	for (auto& t : mTerrain->GetAllTiles())
	{
		t->NumFramesDirty = gNumFrameResources;
	}
}

/// <summary>
/// It's works, but now useless
/// </summary>
/*void TexColumnsApp::BuildDebugGeometry()
{
	GeometryGenerator::MeshData quad;

	// Создаем полноэкранный квад в NDC пространстве (-1 до 1)
	quad.Vertices.resize(4);
	quad.Indices32.resize(6);

	// Вершины
	quad.Vertices[0].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
	quad.Vertices[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	quad.Vertices[0].TexC = XMFLOAT2(0.0f, 1.0f);

	quad.Vertices[1].Position = XMFLOAT3(-1.0f, 1.0f, 0.0f);
	quad.Vertices[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	quad.Vertices[1].TexC = XMFLOAT2(0.0f, 0.0f);

	quad.Vertices[2].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
	quad.Vertices[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	quad.Vertices[2].TexC = XMFLOAT2(1.0f, 1.0f);

	quad.Vertices[3].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
	quad.Vertices[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	quad.Vertices[3].TexC = XMFLOAT2(1.0f, 0.0f);

	// Индексы
	quad.Indices32 = { 0, 1, 2, 1, 3, 2 };

	// Submesh
	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = 0;
	quadSubmesh.BaseVertexLocation = 0;

	// Вершины
	std::vector<Vertex> vertices(quad.Vertices.size());
	for (size_t i = 0; i < quad.Vertices.size(); ++i)
	{
		vertices[i].Pos = quad.Vertices[i].Position;
		vertices[i].Normal = quad.Vertices[i].Normal;
		vertices[i].TexC = quad.Vertices[i].TexC;
		vertices[i].Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
	}

	// Индексы
	auto indices = quad.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	// Создаем геометрию
	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "debugQuadGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["debugQuad"] = quadSubmesh;

	// Сохраняем в mDebugGeo и в общую коллекцию
	mDebugGeometries[geo->Name] = std::move(geo);
	//mGeometries[mDebugGeo->Name] = std::move(mDebugGeo);
}*/

void TexColumnsApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(10.0f, 10.0f, 2, 2);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void TexColumnsApp::GenerateTileGeometry(const XMFLOAT3& worldPos, float tileSize, int lodLevel, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices)
{
	int minResolution = 8;    

	int resolution = (minResolution << lodLevel);
	resolution = min(resolution, minResolution);
	vertices.clear();
	indices.clear();

	//float scale = 1;

	float stepSize = (tileSize) / (resolution - 1);
	float curtainDepth = 50;

	//main vertices
	for (int z = 0; z < resolution; z++)
	{
		for (int x = 0; x < resolution; x++)
		{
			Vertex vertex;
			vertex.Pos = XMFLOAT3(worldPos.x + x * stepSize, worldPos.y, worldPos.z + z * stepSize);
			vertex.TexC = XMFLOAT2((float)x / (resolution - 1), (float)z / (resolution - 1));
			vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vertex.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);
			vertices.push_back(vertex);
		}
	}

	int mainVertexCount = static_cast<int>(vertices.size());

	//curtain vertices
	// Left (x = 0)
	for (int z = 0; z < resolution; z++)
	{
		Vertex vertex = vertices[z * resolution];
		vertex.Pos.y = worldPos.y -curtainDepth;
		vertices.push_back(vertex);
	}

	// right (x = resolution-1)  
	for (int z = 0; z < resolution; z++)
	{
		Vertex vertex = vertices[z * resolution + (resolution - 1)];
		vertex.Pos.y = worldPos.y -curtainDepth;
		vertices.push_back(vertex);
	}

	// bottom (z = 0)
	for (int x = 0; x < resolution ; x++)
	{
		Vertex vertex = vertices[0 * resolution + x];
		vertex.Pos.y = worldPos.y -curtainDepth;
		vertices.push_back(vertex);
	}

	// top(z = resolution-1)
	for (int x = 0; x < resolution ; x++)
	{
		Vertex vertex = vertices[(resolution - 1) * resolution + x];
		vertex.Pos.y = worldPos.y -curtainDepth;
		vertices.push_back(vertex);
	}

	//Indices
	for (int z = 0; z < resolution - 1; z++)
	{
		for (int x = 0; x < resolution - 1; x++)
		{
			UINT topLeft = z * resolution + x;
			UINT topRight = topLeft + 1;
			UINT bottomLeft = (z + 1) * resolution + x;
			UINT bottomRight = bottomLeft + 1;

			indices.push_back(topLeft);
			indices.push_back(bottomLeft);
			indices.push_back(topRight);

			indices.push_back(topRight);
			indices.push_back(bottomLeft);
			indices.push_back(bottomRight);
		}
	}

	// curtain indices
	int leftCurtainStart = mainVertexCount;
	int rightCurtainStart = leftCurtainStart + resolution;
	int bottomCurtainStart = rightCurtainStart + resolution;
	int topCurtainStart = bottomCurtainStart + resolution;

	//left
	for (int z = 0; z < resolution - 1; z++)
	{
		UINT edge1 = z * resolution;
		UINT edge2 = (z + 1) * resolution;
		UINT curtain1 = leftCurtainStart + z;
		UINT curtain2 = leftCurtainStart + z + 1;

		indices.push_back(edge1);
		indices.push_back(curtain1);
		indices.push_back(edge2);

		indices.push_back(edge2);
		indices.push_back(curtain1);
		indices.push_back(curtain2);
	}

	// right 
	for (int z = 0; z < resolution - 1; z++)
	{
		UINT edge1 = z * resolution + (resolution - 1);
		UINT edge2 = (z + 1) * resolution + (resolution - 1);
		UINT curtain1 = rightCurtainStart + z;
		UINT curtain2 = rightCurtainStart + z + 1;

		indices.push_back(edge1);
		indices.push_back(edge2);
		indices.push_back(curtain1);

		indices.push_back(edge2);
		indices.push_back(curtain2);
		indices.push_back(curtain1);
	}

	// bottom
	for (int x = 0; x < resolution - 1; x++)
	{
		UINT edge1 = x;
		UINT edge2 = x + 1;
		UINT curtain1 = bottomCurtainStart + x;
		UINT curtain2 = bottomCurtainStart + x+1;

		indices.push_back(edge1);
		indices.push_back(edge2);
		indices.push_back(curtain1);

		indices.push_back(edge2);
		indices.push_back(curtain2);
		indices.push_back(curtain1);
	}

	// top
	for (int x = 0; x < resolution - 1; x++)
	{
		UINT edge1 = (resolution - 1) * resolution + x;
		UINT edge2 = (resolution - 1) * resolution + x + 1;
		UINT curtain1 = topCurtainStart + x;
		UINT curtain2 = topCurtainStart + x+1;

		indices.push_back(edge1);
		indices.push_back(curtain1);
		indices.push_back(edge2);

		indices.push_back(edge2);
		indices.push_back(curtain1);
		indices.push_back(curtain2);
	}
}
void TexColumnsApp::BuildTerrainGeometry()
{
	auto terrainGeo = std::make_unique<MeshGeometry>();
	terrainGeo->Name = "terrainGeo";

	auto& allTiles = mTerrain->GetAllTiles();

	std::vector<Vertex> allVertices;
	std::vector<std::uint32_t> allIndices;

	for (int tileIdx = 0; tileIdx < allTiles.size(); tileIdx++)
	{
		auto& tile = allTiles[tileIdx];

		std::vector<Vertex> tileVertices;
		std::vector<std::uint32_t> tileIndices;

		GenerateTileGeometry(tile->worldPos, tile->tileSize, tile->lodLevel, tileVertices, tileIndices);

		// ������� ������� �� ���������� ��� ����������� ������
		UINT baseVertex = static_cast<int>(allVertices.size());
		for (auto& index : tileIndices)
		{
			index += baseVertex;
		}

		// ��������� submesh
		SubmeshGeometry submesh;
		submesh.IndexCount = (UINT)tileIndices.size();
		submesh.StartIndexLocation = (UINT)allIndices.size();
		submesh.BaseVertexLocation = 0;

		std::string submeshName = "tile_" + std::to_string(tileIdx) + "_LOD_" + std::to_string(tile->lodLevel);
		terrainGeo->DrawArgs[submeshName] = submesh;

		// ��������� � ����� �������
		allVertices.insert(allVertices.end(), tileVertices.begin(), tileVertices.end());
		allIndices.insert(allIndices.end(), tileIndices.begin(), tileIndices.end());

	}

	const UINT vbByteSize = (UINT)allVertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)allIndices.size() * sizeof(std::uint32_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &terrainGeo->VertexBufferCPU));
	CopyMemory(terrainGeo->VertexBufferCPU->GetBufferPointer(), allVertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &terrainGeo->IndexBufferCPU));
	CopyMemory(terrainGeo->IndexBufferCPU->GetBufferPointer(), allIndices.data(), ibByteSize);

	terrainGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		allVertices.data(), vbByteSize,
		terrainGeo->VertexBufferUploader);

	terrainGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		allIndices.data(), ibByteSize,
		terrainGeo->IndexBufferUploader);

	terrainGeo->VertexByteStride = sizeof(Vertex);
	terrainGeo->VertexBufferByteSize = vbByteSize;
	terrainGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
	terrainGeo->IndexBufferByteSize = ibByteSize;

	mGeometries[terrainGeo->Name] = std::move(terrainGeo);
}

//void TexColumnsApp::CreateBoundingBoxMesh(const BoundingBox& bbox, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices)
//{
////	vertices.clear();
////	indices.clear();
////
////	// �������� 8 ����� bounding box
////	XMFLOAT3 corners[8];
////	bbox.GetCorners(corners);
////
////	// ������� �������
////	for (int i = 0; i < 8; i++) {
////		Vertex vertex;
////		vertex.Pos = corners[i];
////		vertex.Normal = XMFLOAT3(0, 1, 0);
////		vertex.TexC = XMFLOAT2(0, 0);
////		vertex.Tangent = XMFLOAT3(1, 0, 0);
////		vertices.push_back(vertex);
////	}
////
////	// ������� ��� 12 ����� (24 �������)
////	// ������ ����� = 2 �������
////	std::uint32_t lineIndices[] = {
////		// ������ �����
////		0, 1, 1, 2, 2, 3, 3, 0,
////		// ������� �����
////		4, 5, 5, 6, 6, 7, 7, 4,
////		// ������������ �����
////		0, 4, 1, 5, 2, 6, 3, 7
////	};
////
////	indices.insert(indices.end(), std::begin(lineIndices), std::end(lineIndices));
////}
////
////void TexColumnsApp::BuildDebugGeometry()
////{
////	std::vector<Vertex> vertices;
////	std::vector<std::uint32_t> indices;
////
////	// ������� ��������� ��� ���������� bounding box
////	BoundingBox unitBox;
////	BoundingBox::CreateFromPoints(unitBox,
////		XMVectorSet(-0.5f, -0.5f, -0.5f, 0.0f),
////		XMVectorSet(0.5f, 0.5f, 0.5f, 0.0f)
////	);
////
////	CreateBoundingBoxMesh(unitBox, vertices, indices);
////
////	// ������� mesh geometry
////	auto geo = std::make_unique<MeshGeometry>();
////	geo->Name = "debugBoxGeo";
////
////	// Vertex buffer
////	UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
////	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
////		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
////
////	// Index buffer
////	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);
////	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
////		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);
////
////	geo->VertexByteStride = sizeof(Vertex);
////	geo->VertexBufferByteSize = vbByteSize;
////	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
////	geo->IndexBufferByteSize = ibByteSize;
////
////	// Submesh
////	SubmeshGeometry submesh;
////	submesh.IndexCount = (UINT)indices.size();
////	submesh.StartIndexLocation = 0;
////	submesh.BaseVertexLocation = 0;
////	geo->DrawArgs["boundainBox"] = submesh;
////
////	mGeometries[geo->Name] = std::move(geo);
//}

//void  TexColumnsApp::RenderBoundingBoxes()
//{
//	//if (!showTilesBoundingBox) return;
//
//	//mCommandList->SetPipelineState(mPSOs["debug"].Get());
//	//mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
//
//	//auto debugGeo = mGeometries["debugBoxGeo"].get();
//	//mCommandList->IASetVertexBuffers(0, 1, &debugGeo->VertexBufferView());
//	//mCommandList->IASetIndexBuffer(&debugGeo->IndexBufferView());
//	//auto& tiles = mTerrain->GetAllTiles();
//	//for (const auto& tile : tiles) 
//	//{
//	//	BoundingBox debugBox = tile->boundingBox;
//	//	// ��������� ������� �������������� �� unit box � ������ bounding box
//	//	XMFLOAT3 center, extents;
//	//	center = debugBox.Center;
//	//	extents = debugBox.Extents;
//
//	//	// ������������ unit box �� �������� bounding box
//	//	XMMATRIX scale = XMMatrixScaling(extents.x * 2, extents.y * 2, extents.z * 2);
//	//	XMMATRIX translation = XMMatrixTranslation(center.x, center.y, center.z);
//	//	XMMATRIX world = scale * translation;
//
//	//	auto submesh = debugGeo->DrawArgs["box"];
//	//	mCommandList->DrawIndexedInstanced(submesh.IndexCount, 1,
//	//		submesh.StartIndexLocation,
//	//		submesh.BaseVertexLocation, 0);
//	//}
//}

void TexColumnsApp::CreateMaterial(std::string _name, int _CBIndex, int _SRVDiffIndex, int _SRVNMapIndex, int _SRVDispIndex, XMFLOAT4 _DiffuseAlbedo, XMFLOAT3 _FresnelR0, float _Roughness)
{

	auto material = std::make_unique<Material>();
	material->Name = _name;
	material->MatCBIndex = _CBIndex;
	material->DiffuseSrvHeapIndex = _SRVDiffIndex;
	material->NormalSrvHeapIndex = _SRVNMapIndex;
	material->DisplacementSrvHeapIndex = _SRVDispIndex;
	material->DiffuseAlbedo = _DiffuseAlbedo;
	material->FresnelR0 = _FresnelR0;
	material->Roughness = _Roughness;
	mMaterials[_name] = std::move(material);
}

void TexColumnsApp::BuildMaterials()
{
	CreateMaterial("stone0", 0, TexOffsets["stoneTex"], TexOffsets["stoneNorm"], TexOffsets["stonetDisp"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	CreateMaterial("terrain", 0, TexOffsets["terrainDiff"], TexOffsets["terrainNorm"], TexOffsets["terrainDisp"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
}

void TexColumnsApp::BuildRenderItems()
{
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	gridRitem->ObjCBIndex = 0;
	gridRitem->Mat = mMaterials["stone0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	// All the render items are opaque.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());

	//Terrain tiles
	std::vector<std::shared_ptr<Tile>>& allTiles = mTerrain->GetAllTiles();

	for (auto& tile : allTiles)
	{
		auto renderItem = std::make_unique<RenderItem>();
		renderItem->World = MathHelper::Identity4x4();

		renderItem->TexTransform = MathHelper::Identity4x4();
		renderItem->ObjCBIndex = static_cast<int>(mAllRitems.size());
		renderItem->Mat = mMaterials["terrain"].get();
		renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		int lodIndex = tile->lodLevel;
		std::string lodName = "tile_" + std::to_string(tile->tileIndex) + "_LOD_" + std::to_string(lodIndex);
		renderItem->Geo = mGeometries["terrainGeo"].get();
		renderItem->IndexCount = renderItem->Geo->DrawArgs[lodName].IndexCount;
		renderItem->StartIndexLocation = renderItem->Geo->DrawArgs[lodName].StartIndexLocation;
		renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs[lodName].BaseVertexLocation;

		tile->renderItemIndex = static_cast<int>(mAllRitems.size());
		mAllRitems.push_back(std::move(renderItem));
	}
}

void TexColumnsApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		if (ri->IndexCount == 0) {
			OutputDebugStringA("WARNING: IndexCount is zero!\n");
			continue;
		}

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// �������� ������� ���������� ����
		CD3DX12_GPU_DESCRIPTOR_HANDLE baseHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		// ������������� ������������� ������� ��� �������
		CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseHandle = baseHandle;
		diffuseHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, diffuseHandle);  // t0 - diffuse

		CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle = baseHandle;
		normalHandle.Offset(ri->Mat->NormalSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(1, normalHandle);   // t1 - normal

		CD3DX12_GPU_DESCRIPTOR_HANDLE dispHandle = baseHandle;
		dispHandle.Offset(ri->Mat->DisplacementSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(2, dispHandle);     // t2 - displacement

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(3, objCBAddress);  // b0 - per object
		cmdList->SetGraphicsRootConstantBufferView(5, matCBAddress);  // b1 - per material

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void TexColumnsApp::DrawTilesRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<Tile*>& tiles)
{

	char debugMsg[256];

	sprintf_s(debugMsg, "TexOffsets: diff=%d, norm=%d, disp=%d\n",
		TexOffsets["terrainDiff"],
		TexOffsets["terrainNorm"],
		TexOffsets["terrainDisp"]);
	OutputDebugStringA(debugMsg);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT terrCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(TileConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();


	for (auto& tile : tiles)
	{
		auto ri = mAllRitems[tile->renderItemIndex].get();
		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE baseHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		// t0 - Terrain diffuse
		CD3DX12_GPU_DESCRIPTOR_HANDLE terrainDiffHandle = baseHandle;
		terrainDiffHandle.Offset(TexOffsets["terrainDiff"], mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, terrainDiffHandle);

		// t1 - Terrain normal
		CD3DX12_GPU_DESCRIPTOR_HANDLE terrainNormHandle = baseHandle;
		terrainNormHandle.Offset(TexOffsets["terrainNorm"], mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(1, terrainNormHandle);

		// t2 - Terrain displacement
		CD3DX12_GPU_DESCRIPTOR_HANDLE terrainDispHandle = baseHandle;
		terrainDispHandle.Offset(TexOffsets["terrainDisp"], mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(2, terrainDispHandle);

		// === t3 - Brush texture (НОВОЕ) ===
		CD3DX12_GPU_DESCRIPTOR_HANDLE brushTextureHandle = baseHandle;
		brushTextureHandle.Offset(mBrushTextureSRVIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(3, brushTextureHandle);

		// CBV b0 (корневой индекс 4) - cbPerObject
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(4, objCBAddress);

		// CBV b1 (корневой индекс 5) - cbPass - устанавливается в Draw()
		// CBV b2 (корневой индекс 6) - cbMaterial
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(6, matCBAddress);

		// CBV b3 (корневой индекс 7) - cbTerrainTile
		auto terrCB = mCurrFrameResource->TerrainCB->Resource();
		cmdList->SetGraphicsRootConstantBufferView(7, terrCB->GetGPUVirtualAddress() + tile->tileIndex * terrCBByteSize);

		// CBV b4 (корневой индекс 8) - cbBrush
		auto brushCB = mCurrFrameResource->BrushCB->Resource();
		cmdList->SetGraphicsRootConstantBufferView(8, brushCB->GetGPUVirtualAddress());

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void TexColumnsApp::Draw(const GameTimer& gt)
{
	static int frameCount = 0;
	frameCount++;

	char debugMsg[256];

	try
	{
		auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
		HRESULT hr = cmdListAlloc->Reset();
		if (FAILED(hr)) ThrowIfFailed(hr);

		if (isFillModeSolid)
		{
			hr = mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get());
		}
		else
		{
			hr = mCommandList->Reset(cmdListAlloc.Get(), mPSOs["wireframe"].Get());
		}
		if (FAILED(hr)) ThrowIfFailed(hr);

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::DarkGoldenrod, 0, nullptr);
		mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		// ============ ВЫПОЛНЕНИЕ COMPUTE SHADER ============
		// ДОЛЖНО БЫТЬ ДО основного рендеринга!
		if (mIsPainting)
		{
			OutputDebugStringA("=== EXECUTING COMPUTE SHADER ===\n");

			// Устанавливаем Compute PSO и root signature
			mCommandList->SetPipelineState(mPSOs["brushCompute"].Get());
			mCommandList->SetComputeRootSignature(mBrushComputeRootSignature.Get());

			// Барьер для перевода текстуры
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				mBrushTexture.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			mCommandList->ResourceBarrier(1, &barrier);

			// Устанавливаем корневые параметры
			auto brushCB = mCurrFrameResource->BrushCB->Resource();
			mCommandList->SetComputeRootConstantBufferView(0, brushCB->GetGPUVirtualAddress()); // b0

			if (!mVisibleTiles.empty())
			{
				auto tile = mVisibleTiles[0];
				auto terrCB = mCurrFrameResource->TerrainCB->Resource();
				UINT terrCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(TileConstants));
				D3D12_GPU_VIRTUAL_ADDRESS terrainCBAddress = terrCB->GetGPUVirtualAddress() + tile->tileIndex * terrCBByteSize;
				mCommandList->SetComputeRootConstantBufferView(1, terrainCBAddress); // b1
			}

			// SRV для карты высот (t0) - индекс 2
			CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(
				mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
				TexOffsets["terrainDisp"],
				mCbvSrvDescriptorSize);
			mCommandList->SetComputeRootDescriptorTable(2, srvHandle);

			// UAV для текстуры кисти (u0) - индекс 3
			CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(
				mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
				mBrushTextureUAVIndex,
				mCbvSrvDescriptorSize);
			mCommandList->SetComputeRootDescriptorTable(3, uavHandle);

			// Диспатчим Compute Shader
			UINT threadGroupsX = (UINT)ceil(mBrushTextureWidth / 16.0f);
			UINT threadGroupsY = (UINT)ceil(mBrushTextureHeight / 16.0f);

			sprintf_s(debugMsg, "Dispatch: %dx%d thread groups\n", threadGroupsX, threadGroupsY);
			OutputDebugStringA(debugMsg);

			mCommandList->Dispatch(threadGroupsX, threadGroupsY, 1);

			// Возвращаем текстуру в SRV состояние
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				mBrushTexture.Get(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			mCommandList->ResourceBarrier(1, &barrier);

			// Возвращаем графические настройки
			mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
			if (isFillModeSolid)
				mCommandList->SetPipelineState(mPSOs["opaque"].Get());
			else
				mCommandList->SetPipelineState(mPSOs["wireframe"].Get());

			OutputDebugStringA("Compute shader executed successfully\n");
		}
		// =================================================

		// Устанавливаем дескрипторные хипы ДЛЯ ГРАФИКИ
		ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		// Основной рендеринг
		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
		auto passCB = mCurrFrameResource->PassCB->Resource();
		mCommandList->SetGraphicsRootConstantBufferView(4, passCB->GetGPUVirtualAddress());

		DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

		// Рендеринг террейна
		mVisibleTiles.clear();
		mVisibleTiles = mTerrain->GetVisibleTiles();

		if (!mVisibleTiles.empty())
		{
			sprintf_s(debugMsg, "Rendering %d visible tiles\n", (int)mVisibleTiles.size());
			OutputDebugStringA(debugMsg);

			if (isFillModeSolid)
				mCommandList->SetPipelineState(mPSOs["terrain"].Get());
			else
				mCommandList->SetPipelineState(mPSOs["wireTerrain"].Get());

			mCommandList->SetGraphicsRootSignature(mTerrainRootSignature.Get());
			mCommandList->SetGraphicsRootConstantBufferView(5, passCB->GetGPUVirtualAddress());

			DrawTilesRenderItems(mCommandList.Get(), mVisibleTiles);

			// Возвращаемся к основной PSO
			mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
			if (isFillModeSolid)
				mCommandList->SetPipelineState(mPSOs["opaque"].Get());
			else
				mCommandList->SetPipelineState(mPSOs["wireframe"].Get());
		}

		// ============ ДЕБАГ РЕНДЕРИНГ ТЕКСТУРЫ КИСТИ ============
		// ДОЛЖНО БЫТЬ ПОСЛЕ compute shader и ДО ImGui!
		/*if (mShowDebugTexture)
		{
			OutputDebugStringA("Debug rendering brush texture...\n");

			// Сохраняем текущий viewport/scissor
			D3D12_VIEWPORT savedViewport = mScreenViewport;
			D3D12_RECT savedScissor = mScissorRect;

			// Настройки для дебаг окна (справа вверху)
			D3D12_VIEWPORT debugViewport;
			debugViewport.TopLeftX = mScreenViewport.Width - 256.0f - 10.0f; // Правая сторона с отступом 10px
			debugViewport.TopLeftY = 10.0f;                                   // Верх с отступом 10px
			debugViewport.Width = 256.0f;                                     // Размер 256x256
			debugViewport.Height = 256.0f;
			debugViewport.MinDepth = 0.0f;
			debugViewport.MaxDepth = 1.0f;

			D3D12_RECT debugScissor;
			debugScissor.left = mScreenViewport.Width - 256.0f - 10.0f;   // Та же позиция X
			debugScissor.top = 10;                                                // Та же позиция Y
			debugScissor.right = debugScissor.left + 256;                         // Ширина 256
			debugScissor.bottom = debugScissor.top + 256;                         // Высота 256

			mCommandList->RSSetViewports(1, &debugViewport);
			mCommandList->RSSetScissorRects(1, &debugScissor);

			// Устанавливаем дебаг PSO (важно: используем debug PSO, не terrain!)
			mCommandList->SetPipelineState(mPSOs["debugQuad"].Get()); 
			mCommandList->SetGraphicsRootSignature(mDebugRootSignature.Get());

			// Устанавливаем текстуру кисти как SRV (ВАЖНО: дескрипторные хипы уже установлены выше)
			CD3DX12_GPU_DESCRIPTOR_HANDLE brushSrvHandle(
				mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
				mBrushTextureSRVIndex,
				mCbvSrvDescriptorSize);
			mCommandList->SetGraphicsRootDescriptorTable(0, brushSrvHandle);

			// Получаем геометрию (исправлено имя ключа)
			auto debugGeoIt = mDebugGeometries.find("debugQuadGeo"); // Исправлено: было "debugQuad"
			if (debugGeoIt != mDebugGeometries.end() && debugGeoIt->second)
			{
				auto& debugGeo = debugGeoIt->second;
				D3D12_VERTEX_BUFFER_VIEW vbv = debugGeo->VertexBufferView();
				D3D12_INDEX_BUFFER_VIEW ibv = debugGeo->IndexBufferView();

				mCommandList->IASetVertexBuffers(0, 1, &vbv);
				mCommandList->IASetIndexBuffer(&ibv);
				mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				// Рисуем дебаг квад
				auto submeshIt = debugGeo->DrawArgs.find("debugQuad");
				if (submeshIt != debugGeo->DrawArgs.end())
				{
					auto& submesh = submeshIt->second;
					mCommandList->DrawIndexedInstanced(submesh.IndexCount, 1,
						submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
				}
				else
				{
					OutputDebugStringA("ERROR: 'debugQuad' not found in DrawArgs!\n");
				}
			}
			else
			{
				sprintf_s(debugMsg, "ERROR: Debug geometry 'debugQuadGeo' not found! Map size: %zu\n",
					mDebugGeometries.size());
				OutputDebugStringA(debugMsg);
			}

			// Восстанавливаем viewport/scissor
			mCommandList->RSSetViewports(1, &savedViewport);
			mCommandList->RSSetScissorRects(1, &savedScissor);

			// Возвращаемся к основному рендерингу
			mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
			if (isFillModeSolid)
				mCommandList->SetPipelineState(mPSOs["opaque"].Get());
			else
				mCommandList->SetPipelineState(mPSOs["wireframe"].Get());

			OutputDebugStringA("Debug rendering complete\n");
		}*/
		// =======================================================

		// ImGui рендеринг (последний)
		ID3D12DescriptorHeap* imguiHeaps[] = { mImGuiSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(imguiHeaps), imguiHeaps);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

		// Возвращаем основной heap (если нужно дальше)
		ID3D12DescriptorHeap* mainHeaps[] = { mSrvDescriptorHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(mainHeaps), mainHeaps);

		// Завершение кадра
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

		hr = mCommandList->Close();
		if (FAILED(hr)) ThrowIfFailed(hr);

		ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
		mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		hr = mSwapChain->Present(0, 0);
		if (FAILED(hr)) ThrowIfFailed(hr);

		mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;
		mCurrFrameResource->Fence = ++mCurrentFence;
		hr = mCommandQueue->Signal(mFence.Get(), mCurrentFence);
		if (FAILED(hr)) ThrowIfFailed(hr);
	}
	catch (...)
	{
		throw;
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TexColumnsApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

