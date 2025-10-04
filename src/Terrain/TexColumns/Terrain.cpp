#include "Terrain.h"

void Terrain::Initialize(ID3D12Device* device, float worldSize, int maxLOD, XMFLOAT3 terrainOffset)
{
    mWorldSize = worldSize;
    mMaxLOD = maxLOD;
    mHeightScale = 500;
    mTerrainOffset = terrainOffset;
    tileIndex = 0;
    BuildTree();

}


void Terrain::Update(const XMFLOAT3& cameraPos, BoundingFrustum& frustum)
{
    mVisibleTiles.clear();
    mRoot->UpdateVisibility(frustum, cameraPos, mVisibleTiles, mHeightScale, (int)mWorldSize);
}

bool QuadTreeNode::ShouldSplit(const XMFLOAT3& cameraPos, float heightscale, int mapsize) const
{
    auto camPos = cameraPos;
    camPos.y = 0;
    XMVECTOR camPosVec = XMLoadFloat3(&camPos);
    float lodneeddist = (mapsize / 2.0f - depth * mapsize / 16);
    BoundingSphere sphere;
    sphere.Center = cameraPos;
    sphere.Radius = lodneeddist;
    if (sphere.Intersects(boundingBox))
    {
        return true;
    }
    return false;
}

// 4. ���������� ��������� � ������������
void QuadTreeNode::UpdateVisibility(BoundingFrustum& frustum, const XMFLOAT3& cameraPos, std::vector<Tile*>& visibleTiles, float heightscale, int mapsize)
{
    if (frustum.Contains(boundingBox) == DISJOINT)
    {
        return; // ���� ��������� �� �����
    }

    // ���� ���� �������� "������" (��� �������� �����) ��� �� ����� ��� ���������
    if (!children[0] || !ShouldSplit(cameraPos, heightscale, mapsize))
    {
        // �������� ������� ���� (����)
        if (tile)
        {
            visibleTiles.push_back(tile);
        }
    }
    else // ���� ����� ���������
    {
        // ���������� ��������� ��������� �������� �����
        for (int i = 0; i < 4; i++)
        {
            if (children[i])
            {
                children[i]->UpdateVisibility(frustum, cameraPos, visibleTiles, heightscale, mapsize);
            }
        }
    }
}

std::vector<std::shared_ptr<Tile>>& Terrain::GetAllTiles()
{
    return mAllTiles;
}

std::vector<Tile*>& Terrain::GetVisibleTiles()
{
   
    return mVisibleTiles;
}

float minHeight=-5;
float maxHeight = 400;
void Terrain::BuildTree()
{
    //float halfSize = mWorldSize * 0.5f;
    
    mRoot = std::make_unique<QuadTreeNode>();
    mRoot->boundingBox= CalculateAABB(mTerrainOffset, mWorldSize, minHeight, maxHeight);
    mRoot->size = mWorldSize;
    mRoot->depth = 0;
    mRoot->isLeaf = false;

    BuildNode(mRoot.get(), mTerrainOffset.x, mTerrainOffset.z, 0);
}

void Terrain::BuildNode(QuadTreeNode* node, float x, float z, int depth)
{
    CreateTileForNode(node, x, z, depth);

    // ���� �������� ������������ ������� - ���������������
    if (depth >= mMaxLOD) 
    {
        node->isLeaf = true;
        return;
    }

    float childSize = node->size * 0.5f;

    for (int i = 0; i < 4; ++i) {
        node->children[i] = std::make_unique<QuadTreeNode>();
        auto& child = node->children[i];

        float childX = x + (i % 2) * childSize;
        float childZ = z + (i / 2) * childSize;

        child->boundingBox= CalculateAABB(XMFLOAT3(childX, mTerrainOffset.y, childZ), childSize, minHeight, maxHeight);
        child->size = childSize;
        child->depth = depth + 1;
        child->isLeaf = false;

        // ���������� ������ �������� ����
        BuildNode(child.get(), childX, childZ, depth +1);
    }
}
BoundingBox Terrain::CalculateAABB(const XMFLOAT3& pos, float size, float minHeight, float maxHeight)
{
    BoundingBox aabb;
    auto minPoint = XMFLOAT3(pos.x, minHeight, pos.z);
    auto maxPoint = XMFLOAT3(pos.x + size, maxHeight, pos.z + size);
    XMVECTOR pt1 = XMLoadFloat3(&minPoint);
    XMVECTOR pt2 = XMLoadFloat3(&maxPoint);
    BoundingBox::CreateFromPoints(aabb, pt1, pt2);
    return aabb;
}

// �������� ����� ��� ����
void Terrain::CreateTileForNode(QuadTreeNode* node, float x, float z, int depth)
{
    auto tile = std::make_shared<Tile>();

    tile->worldPos = XMFLOAT3(x, mTerrainOffset.y, z);

    tile->lodLevel = depth;
    tile->tileSize = mWorldSize / (1 << depth);
    tile->tileIndex = tileIndex++;
    tile->boundingBox = node->boundingBox;
    tile->isVisible = true;
    mAllTiles.push_back(std::move(tile));
    node->tile = mAllTiles.back().get();
}

void Terrain::UpdateLOD(const XMFLOAT3& cameraPos, float lodTransitionDistance)
{
    UpdateNodeLOD(mRoot.get(), cameraPos, lodTransitionDistance);
}

void Terrain::UpdateNodeLOD(QuadTreeNode* node, const XMFLOAT3& cameraPos, float lodTransitionDistance)
{
    if (!node || !node->tile) return;

    // ��������� ���������� �� ������ �� ��������� ����� bounding box
    XMVECTOR cameraPosVec = XMLoadFloat3(&cameraPos);
    XMVECTOR boxCenter = XMLoadFloat3(&node->boundingBox.Center);

    // ����� ������ ���������� �� bounding box
    float distance = 0.0f;
    node->boundingBox.Contains(cameraPosVec);

    // ���� ������ ������ �����, ���������� ����������� ����������
    if (distance > 0.0f) {
        distance = 0.0f;
    }
    else {
        distance = std::abs(distance);
    }

    // ���������� ������ �� ���� ���� ������ ������
    bool shouldSplit = (distance < lodTransitionDistance * (node->depth + 1)) &&
        (node->depth < mMaxLOD);

    if (shouldSplit && node->isLeaf) {
        // ���������� �������� ���� � �������� ������������
        node->isLeaf = false;
        node->tile->isVisible = false;

        // ���������� ��� �������� �����
        for (int i = 0; i < 4; ++i) {
            if (node->children[i] && node->children[i]->tile) {
                node->children[i]->tile->isVisible = true;
            }
        }
    }
    else if (!shouldSplit && !node->isLeaf) {
        // �������� ��� �������� ����� � ���������� ������������
        HideChildrenTiles(node);
        node->isLeaf = true;
        node->tile->isVisible = true;
    }

    // ���������� ��������� �������� ���� (������ ���� ���� ������)
    if (!node->isLeaf) {
        for (int i = 0; i < 4; ++i) {
            UpdateNodeLOD(node->children[i].get(), cameraPos, lodTransitionDistance);
        }
    }
}

// ��������������� ������� ��� ������� ���� �������� ������
void Terrain::HideChildrenTiles(QuadTreeNode* node)
{
    if (!node) return;

    for (int i = 0; i < 4; ++i) {
        if (node->children[i]) {
            if (node->children[i]->tile) {
                node->children[i]->tile->isVisible = false;
            }
            // ���������� �������� ���� ��������
            HideChildrenTiles(node->children[i].get());
        }
    }
}

// ����������� ������� ����� ������� ������
void Terrain::CollectVisibleTiles(QuadTreeNode* node, const BoundingFrustum& frustum, std::vector<Tile*>& visibleTiles)
{
    if (!node) return;

    // ���� ���� ����� � �������� �������-����
    if (node->tile && node->tile->isVisible) {
        if (frustum.Intersects(node->boundingBox)) {
            visibleTiles.push_back(node->tile);
        }
    }

    // ���������� ��������� �������� ���� (������ ���� ���� ������� � �� �������� ������)
    if (!node->isLeaf) {
        for (int i = 0; i < 4; ++i) {
            CollectVisibleTiles(node->children[i].get(), frustum, visibleTiles);
        }
    }
}