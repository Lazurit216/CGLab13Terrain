//***************************************************************************************ObjectCB
// TexColumnsApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

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

	float g_Scale = 1.f;
	float g_TessellationFactor = 1.f;

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;

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
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void CreateMaterial(std::string _name, int _CBIndex, int _SRVDiffIndex, int _SRVNMapIndex, int _SRVDispIndex, XMFLOAT4 _DiffuseAlbedo, XMFLOAT3 _FresnelR0, float _Roughness);
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	bool mDecalVisible = false; // Флаг видимости декали

	// Метод для ray casting
	bool ScreenToWorld(int screenX, int screenY, XMFLOAT3& worldPos);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;
	//
	std::unordered_map<std::string, int>TexOffsets;
	//
	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;

	bool isFillModeSolid = true;
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


	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void TexColumnsApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void TexColumnsApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

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

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
	UpdateMaterialCBs(gt);
}

void TexColumnsApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (isFillModeSolid)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}
	else
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));


	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(8, passCB->GetGPUVirtualAddress());


	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

}




bool TexColumnsApp::ScreenToWorld(int screenX, int screenY, XMFLOAT3& worldPos)
{
	// Детальный отладочный вывод
	OutputDebugStringA("=== ScreenToWorld Debug ===\n");

	std::string debugMsg = "Input: screenX=" + std::to_string(screenX) +
		", screenY=" + std::to_string(screenY) +
		", mClientWidth=" + std::to_string(mClientWidth) +
		", mClientHeight=" + std::to_string(mClientHeight) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	// 1. Нормализованные координаты экрана [-1, 1]
	float nx = (2.0f * static_cast<float>(screenX)) / mClientWidth - 1.0f;
	float ny = 1.0f - (2.0f * static_cast<float>(screenY)) / mClientHeight;

	debugMsg = "Normalized: nx=" + std::to_string(nx) + ", ny=" + std::to_string(ny) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	// 2. Создаем луч в clip space
	XMVECTOR rayStart = XMVectorSet(nx, ny, 0.0f, 1.0f);
	XMVECTOR rayEnd = XMVectorSet(nx, ny, 1.0f, 1.0f);

	// 3. Получаем обратную матрицу ViewProj
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	// Отладочный вывод матриц
	XMFLOAT4X4 viewProjMatrix;
	XMStoreFloat4x4(&viewProjMatrix, viewProj);

	debugMsg = "ViewProj Matrix:\n";
	for (int i = 0; i < 4; i++) {
		debugMsg += "[" + std::to_string(viewProjMatrix.m[i][0]) + ", " +
			std::to_string(viewProjMatrix.m[i][1]) + ", " +
			std::to_string(viewProjMatrix.m[i][2]) + ", " +
			std::to_string(viewProjMatrix.m[i][3]) + "]\n";
	}
	OutputDebugStringA(debugMsg.c_str());

	// 4. Преобразуем в мировые координаты
	rayStart = XMVector3TransformCoord(rayStart, invViewProj);
	rayEnd = XMVector3TransformCoord(rayEnd, invViewProj);

	XMFLOAT3 startPos, endPos;
	XMStoreFloat3(&startPos, rayStart);
	XMStoreFloat3(&endPos, rayEnd);

	debugMsg = "World ray start: X=" + std::to_string(startPos.x) +
		", Y=" + std::to_string(startPos.y) +
		", Z=" + std::to_string(startPos.z) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	debugMsg = "World ray end: X=" + std::to_string(endPos.x) +
		", Y=" + std::to_string(endPos.y) +
		", Z=" + std::to_string(endPos.z) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	// 5. Направление луча
	XMVECTOR rayDir = XMVectorSubtract(rayEnd, rayStart);
	rayDir = XMVector3Normalize(rayDir);

	// 6. Intersection с плоскостью Y=0
	XMVECTOR planeNormal = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR planePoint = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	// Формула: t = dot(planePoint - rayStart, planeNormal) / dot(rayDir, planeNormal)
	XMVECTOR diff = XMVectorSubtract(planePoint, rayStart);
	float numerator = XMVectorGetX(XMVector3Dot(diff, planeNormal));
	float denominator = XMVectorGetX(XMVector3Dot(rayDir, planeNormal));

	debugMsg = "Intersection calc: numerator=" + std::to_string(numerator) +
		", denominator=" + std::to_string(denominator) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	if (fabs(denominator) < 1e-6f) {
		OutputDebugStringA("MISS: Ray parallel to plane\n");
		return false;
	}

	float t = numerator / denominator;
	debugMsg = "Intersection t=" + std::to_string(t) + "\n";
	OutputDebugStringA(debugMsg.c_str());

	if (t >= 0.0f) {
		XMVECTOR intersection = XMVectorAdd(rayStart, XMVectorScale(rayDir, t));
		XMFLOAT3 intersectPos;
		XMStoreFloat3(&intersectPos, intersection);

		debugMsg = "Intersection point: X=" + std::to_string(intersectPos.x) +
			", Y=" + std::to_string(intersectPos.y) +
			", Z=" + std::to_string(intersectPos.z) + "\n";
		OutputDebugStringA(debugMsg.c_str());

		// Проверяем границы grid (10x10)
		if (intersectPos.x >= -5.0f && intersectPos.x <= 5.0f &&
			intersectPos.z >= -5.0f && intersectPos.z <= 5.0f) {
			worldPos = intersectPos;
			OutputDebugStringA("HIT: Valid intersection found\n");
			return true;
		}
		else {
			debugMsg = "MISS: Outside grid bounds. X=" + std::to_string(intersectPos.x) +
				", Z=" + std::to_string(intersectPos.z) + "\n";
			OutputDebugStringA(debugMsg.c_str());
		}
	}
	else {
		OutputDebugStringA("MISS: Intersection behind camera\n");
	}

	return false;
}
void TexColumnsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	// Левый клик - установка декали
	if ((btnState & MK_LBUTTON) != 0)
	{
		XMFLOAT3 worldPos;
		if (ScreenToWorld(x, y, worldPos))
		{
			// Успешное попадание на plane
			mMainPassCB.decalPosition = worldPos;
			mDecalVisible = true;

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
			// Клик мимо plane - скрываем декаль
			mDecalVisible = false;
			OutputDebugStringA("Decal hidden: click missed the plane\n");
		}
	}

	SetCapture(mhMainWnd);
}

void TexColumnsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void TexColumnsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);

		//const float minRadius = 5.0f;
		//const float maxRadius = 150.0f;
		//const float minTess = 1.0f;
		//const float maxTess = 10.0f;

		//// Ступенчатая функция для оптимизации
		//float tessellationFactor;
		//if (mRadius < 20.0f) tessellationFactor = 32.0f;
		//else if (mRadius < 30.0f) tessellationFactor = 10;
		//else if (mRadius < 60.0f) tessellationFactor = 5.0f;
		//else tessellationFactor = 1.0f;

		//// Устанавливаем tessellation factor для всех render items
		//for (auto& item : mAllRitems)
		//{
		//	item->g_TessellationFactor = tessellationFactor;
		//}


		//

		//// Debug output
		//OutputDebugStringA(("Camera Radius: " + std::to_string(mRadius) +
		//	", Tessellation Factor: " + std::to_string(tessellationFactor) + "\n").c_str());
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void TexColumnsApp::OnKeyboardInput(const GameTimer& gt)
{
}

void TexColumnsApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	// Debug output
	/*OutputDebugStringA(("Camera Pos: " +
		std::to_string(mEyePos.x) + ", " +
		std::to_string(mEyePos.y) + ", " +
		std::to_string(mEyePos.z) + "\n").c_str());*/
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
			//float* g_Scale = &e->g_Scale;
			//float* g_TessellationFactor = &e->g_TessellationFactor;



			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.g_Scale = e->g_Scale;
			objConstants.g_TessellationFactor = e->g_TessellationFactor;


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

	// Проверьте что матрицы не нулевые
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	if (XMMatrixIsIdentity(view) || XMMatrixIsIdentity(proj)) {
		OutputDebugStringA("WARNING: View or projection matrix is identity!\n");
	}



	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	/*mMainPassCB.DecalRadius = 1.0f;
	mMainPassCB.DecalFalloffRadius = 2.3f;
	mMainPassCB.decalPosition = { .0f, .0f, .0f }; // Позиция декали*/

	// Устанавливаем радиусы декали только если она видима

	mMainPassCB.isDecalVisible = mDecalVisible? 1: 0;
	//OutputDebugStringA(("Decal visible: " + std::string(mDecalVisible ? "true" : "false") + "\n").c_str());
	
	mMainPassCB.DecalRadius = 1.0f;
	mMainPassCB.DecalFalloffRadius = 1.7f;


	XMVECTOR decalPos = XMLoadFloat3(&mMainPassCB.decalPosition);
	float scale = 3.5;
	XMVECTOR decalCamPos = decalPos + XMVectorSet(0.0f, 1.5f, 0.0f, 0.0f); // Смещаем вверх
	XMVECTOR target = decalPos;
	XMVECTOR up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Направление "вверх" для проектора (ось Z)

	XMMATRIX decalView = XMMatrixLookAtLH(decalCamPos, target, up);
	// Ортографическая проекция размером DecalSize x DecalSize, глубина DecalSize
	XMMATRIX decalProj = XMMatrixOrthographicLH(scale, scale, 0.0f, 1); // Near=0, Far=DecalSize

	// Транспонируем перед отправкой в константный буфер
	XMStoreFloat4x4(&mMainPassCB.DecalViewProj, XMMatrixTranspose(decalView * decalProj));
	// --- Конец расчета матрицы ---
	XMMATRIX decalTexTransform = XMMatrixScaling(0.5f, -0.5f, 1.0f) *
		XMMatrixTranslation(0.5f, 0.5f, 0.0f);
	XMStoreFloat4x4(&mMainPassCB.DecalTexTransform, XMMatrixTranspose(decalTexTransform));

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void TexColumnsApp::LoadTextures()
{
	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"../../Textures/terrain_diff.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTex->Filename.c_str(),
		stoneTex->Resource, stoneTex->UploadHeap));

	auto stoneTexD = std::make_unique<Texture>();
	stoneTexD->Name = "stonetDisp";
	stoneTexD->Filename = L"../../Textures/terrain_disp.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTexD->Filename.c_str(),
		stoneTexD->Resource, stoneTexD->UploadHeap));

	auto stoneTexN = std::make_unique<Texture>();
	stoneTexN->Name = "stoneNorm";
	stoneTexN->Filename = L"../../Textures/terrain_norm.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTexN->Filename.c_str(),
		stoneTexN->Resource, stoneTexN->UploadHeap));
	auto decalTex = std::make_unique<Texture>();
	decalTex->Name = "decalTex";
	decalTex->Filename = L"../../Textures/DecalDiff.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), decalTex->Filename.c_str(),
		decalTex->Resource, decalTex->UploadHeap));

	auto decalTexD = std::make_unique<Texture>();
	decalTexD->Name = "decalDisp";
	decalTexD->Filename = L"../../Textures/DecalDisp.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), decalTexD->Filename.c_str(),
		decalTexD->Resource, decalTexD->UploadHeap));

	auto decalTexN = std::make_unique<Texture>();
	decalTexN->Name = "decalNorm";
	decalTexN->Filename = L"../../Textures/DecalNorm.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), decalTexN->Filename.c_str(),
		decalTexN->Resource, decalTexN->UploadHeap));

	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[stoneTexD->Name] = std::move(stoneTexD);
	mTextures[stoneTexN->Name] = std::move(stoneTexN);

	mTextures[decalTex->Name] = std::move(decalTex);
	mTextures[decalTexD->Name] = std::move(decalTexD);
	mTextures[decalTexN->Name] = std::move(decalTexN);
}

void TexColumnsApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE diffuseRange;
	diffuseRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // Диффузная текстура в регистре t0

	CD3DX12_DESCRIPTOR_RANGE normalRange;
	normalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);  // Нормальная карта в регистре t1

	CD3DX12_DESCRIPTOR_RANGE dispMap;
	dispMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);  // Dispmap в регистре t2

	CD3DX12_DESCRIPTOR_RANGE decalDiffuseRange;
	decalDiffuseRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);  // Диффузная текстура декали в t3

	CD3DX12_DESCRIPTOR_RANGE decalNormalRange;
	decalNormalRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);  // Нормальная карта декали в t4

	CD3DX12_DESCRIPTOR_RANGE decalDispMap;
	decalDispMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5);  // Dispmap декали в t5

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[9];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &diffuseRange, D3D12_SHADER_VISIBILITY_ALL);      // t0
	slotRootParameter[1].InitAsDescriptorTable(1, &normalRange, D3D12_SHADER_VISIBILITY_ALL);       // t1  
	slotRootParameter[2].InitAsDescriptorTable(1, &dispMap, D3D12_SHADER_VISIBILITY_ALL);           // t2
	slotRootParameter[3].InitAsDescriptorTable(1, &decalDiffuseRange, D3D12_SHADER_VISIBILITY_ALL); // t3
	slotRootParameter[4].InitAsDescriptorTable(1, &decalNormalRange, D3D12_SHADER_VISIBILITY_ALL);  // t4
	slotRootParameter[5].InitAsDescriptorTable(1, &decalDispMap, D3D12_SHADER_VISIBILITY_ALL);      // t5

	slotRootParameter[6].InitAsConstantBufferView(0); // b0 - per object
	slotRootParameter[7].InitAsConstantBufferView(1); // b1 - per material
	slotRootParameter[8].InitAsConstantBufferView(2); // b2 - per pass

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(9, slotRootParameter,
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

void TexColumnsApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = mTextures.size(); // Основные текстуры + 2 дополнительные для декали
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	int offset = 0;

	// Сначала создаем дескрипторы для основных текстур
	for (const auto& tex : mTextures) {
		if (!tex.second || !tex.second->Resource) {
			OutputDebugStringA(("Missing texture: " + tex.first + "\n").c_str());
			continue;
		}

		auto text = tex.second->Resource;
		srvDesc.Format = text->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = text->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(text.Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvDescriptorSize);
		TexOffsets[tex.first] = offset;
		offset++;
	}

}

void TexColumnsApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaqueHS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "HS", "hs_5_1");
	mShaders["opaqueDS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "DS", "ds_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void TexColumnsApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(10.0f, 10.0f, 60, 40);
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
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
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
}

void TexColumnsApp::BuildFrameResources()
{
	/*for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}*/

	FlushCommandQueue();
	mFrameResources.clear();
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
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
}


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
	/*auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;*/

	/*auto forest0 = std::make_unique<Material>();
	forest0->Name = "forest0";
	forest0->MatCBIndex = 3;
	forest0->DiffuseSrvHeapIndex = 3;
	forest0->DisplacementSrvHeapIndex = 4;
	forest0->NormalSrvHeapIndex = 5;
	forest0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	forest0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	forest0->Roughness = 0.3f;*/

	/*mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);*/
	//mMaterials["forest0"] = std::move(forest0);

	CreateMaterial("forest0", 0, TexOffsets["forestTex"], TexOffsets["forestNorm"], TexOffsets["forestDisp"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	CreateMaterial("stone0", 0, TexOffsets["stoneTex"], TexOffsets["stoneNorm"], TexOffsets["stonetDisp"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
	CreateMaterial("decal", 0, TexOffsets["decalTex"], TexOffsets["decalNorm"], TexOffsets["decalDisp"], XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT3(0.05f, 0.05f, 0.05f), 0.3f);
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

		// Получаем базовый дескриптор кучи
		CD3DX12_GPU_DESCRIPTOR_HANDLE baseHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		// Устанавливаем дескрипторные таблицы для текстур
		CD3DX12_GPU_DESCRIPTOR_HANDLE diffuseHandle = baseHandle;
		diffuseHandle.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(0, diffuseHandle);  // t0 - diffuse

		CD3DX12_GPU_DESCRIPTOR_HANDLE normalHandle = baseHandle;
		normalHandle.Offset(ri->Mat->NormalSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(1, normalHandle);   // t1 - normal

		CD3DX12_GPU_DESCRIPTOR_HANDLE dispHandle = baseHandle;
		dispHandle.Offset(ri->Mat->DisplacementSrvHeapIndex, mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(2, dispHandle);     // t2 - displacement

		// Текстуры декали
		CD3DX12_GPU_DESCRIPTOR_HANDLE decalDiffuseHandle = baseHandle;
		decalDiffuseHandle.Offset(TexOffsets["decalTex"], mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(3, decalDiffuseHandle);  // t3 - decal diffuse

		CD3DX12_GPU_DESCRIPTOR_HANDLE decalNormalHandle = baseHandle;
		decalNormalHandle.Offset(TexOffsets["decalNorm"], mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(4, decalNormalHandle);   // t4 - decal normal

		CD3DX12_GPU_DESCRIPTOR_HANDLE decalDispHandle = baseHandle;
		decalDispHandle.Offset(TexOffsets["decalDisp"], mCbvSrvDescriptorSize);
		cmdList->SetGraphicsRootDescriptorTable(5, decalDispHandle);     // t5 - decal displacement

		// Constant buffers
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(6, objCBAddress);  // b0 - per object
		cmdList->SetGraphicsRootConstantBufferView(7, matCBAddress);  // b1 - per material

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
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

