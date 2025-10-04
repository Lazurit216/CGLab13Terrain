#pragma once
#include "../../Common/d3dApp.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
struct Tile
{
	XMFLOAT3 worldPos;
	int lodLevel;
	float tileSize;
	BoundingBox boundingBox;
	bool isVisible = true;
	int tileIndex;
	int renderItemIndex;
	int NumFramesDirty;
};

struct QuadTreeNode
{
	BoundingBox boundingBox;
	float size;                  // Размер узла
	int depth;                   // Глубина узла
	bool isLeaf;                 // Является ли листом
	std::unique_ptr<QuadTreeNode> children[4]; // Дочерние узлы
	Tile* tile;  // Связанный тайл
};

class Terrain
{
public:
	Terrain() {};

	void Initialize(ID3D12Device* device, float worldSize, int maxLOD, XMFLOAT3 terrainOffset);
	void Update(const XMFLOAT3& cameraPos, BoundingFrustum& frustum);
	std::vector<std::shared_ptr<Tile>>& GetAllTiles();
	std::vector<Tile*>& GetVisibleTiles();
	void BuildTree();
	void UpdateLOD(const XMFLOAT3& cameraPos, float lodTransitionDistance);

private:
	void CreateTileForNode(QuadTreeNode* node, int depth);
	void BuildNode(QuadTreeNode* node, float x, float y, int depth);
	void CollectVisibleTiles(QuadTreeNode* node, const DirectX::BoundingFrustum& frustum, std::vector<Tile*>& visibleTiles);
	void UpdateNodeLOD(QuadTreeNode* node, const XMFLOAT3& cameraPos, float lodTransitionDistance);
	BoundingBox CalculateAABB(const XMFLOAT3& pos, float size, float minHeight, float maxHeight);

public:
	float mWorldSize;
	float mHeightScale;
	int renderlodlevel = 0;
	int tileRenderIndex = 0;
	XMFLOAT3 mTerrainOffset;

private:
	std::unique_ptr<QuadTreeNode> mRoot;
	ComPtr<ID3D12Resource> mHeightmapTexture;
	std::vector<std::shared_ptr<Tile>> mAllTiles;
	std::vector<Tile*> mVisibleTiles;
	int mMaxLOD;
	int tileIndex = 0;
};

