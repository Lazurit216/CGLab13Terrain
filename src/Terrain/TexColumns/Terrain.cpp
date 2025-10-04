#include "Terrain.h"

void Terrain::Initialize(ID3D12Device* device, float worldSize, int maxLOD, XMFLOAT3 terrainOffset)
{
    mWorldSize = worldSize;
    mMaxLOD = maxLOD;
    mHeightScale = 50.0f;
    mTerrainOffset = terrainOffset;
    tileIndex = 0;
    BuildTree();

}


void Terrain::Update(const XMFLOAT3& cameraPos, BoundingFrustum& frustum)
{
    mVisibleTiles.clear();

    std::vector<Tile*> visibleTiles;
    CollectVisibleTiles(mRoot.get(), frustum, visibleTiles);
    mVisibleTiles = std::move(visibleTiles);

    float lodTransitionDistance = mWorldSize/10; // Настройте под свои нужды
    UpdateLOD(cameraPos, lodTransitionDistance);
}

std::vector<std::shared_ptr<Tile>>& Terrain::GetAllTiles()
{
    return mAllTiles;
}

std::vector<Tile*>& Terrain::GetVisibleTiles()
{
   
    return mVisibleTiles;
}


void Terrain::BuildTree()
{
    //float halfSize = mWorldSize * 0.5f;
    
    mRoot = std::make_unique<QuadTreeNode>();
    mRoot->boundingBox= CalculateAABB(mTerrainOffset, mWorldSize, -5.0f, 400.0f);
    mRoot->size = mWorldSize;
    mRoot->depth = 0;
    mRoot->isLeaf = false;

    BuildNode(mRoot.get(), mTerrainOffset.x, mTerrainOffset.z, 0);
}

void Terrain::BuildNode(QuadTreeNode* node, float x, float y, int depth)
{
    CreateTileForNode(node, depth);

    // Если достигли максимальной глубины - останавливаемся
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
        float childY = y + (i / 2) * childSize;

        child->boundingBox= CalculateAABB(XMFLOAT3(childX, mTerrainOffset.y, childY), childSize, -5.0f, 400.0f);
        child->size = childSize;
        child->depth = depth + 1;
        child->isLeaf = false;

        // Рекурсивно строим дочерний узел
        BuildNode(child.get(), childX, childY, depth + 1);
    }
}
BoundingBox Terrain::CalculateAABB(const XMFLOAT3& pos, float size, float minHeight, float maxHeight)
{
    BoundingBox aabb;
    auto minPoint = XMFLOAT3(pos.x, 0, pos.z);
    auto maxPoint = XMFLOAT3(pos.x + size, 100, pos.z + size);
    XMVECTOR pt1 = XMLoadFloat3(&minPoint);
    XMVECTOR pt2 = XMLoadFloat3(&maxPoint);
    BoundingBox::CreateFromPoints(aabb, pt1, pt2);
    return aabb;
}

// Создание тайла для узла
void Terrain::CreateTileForNode(QuadTreeNode* node, int depth)
{
    auto tile = std::make_shared<Tile>();
    XMFLOAT3 center = node->boundingBox.Center;

    tile->worldPos = center;

    tile->lodLevel = depth;
    tile->tileSize = node->size;
    tile->tileIndex = tileIndex++;
    tile->boundingBox = node->boundingBox;
    tile->isVisible = true;
    mAllTiles.push_back(std::move(tile));
    node->tile = mAllTiles.back().get();
}

// Рекурсивный сбор видимых тайлов
void Terrain::CollectVisibleTiles(QuadTreeNode* node, const BoundingFrustum& frustum, std::vector<Tile*>& visibleTiles)
{
    if (!node) return;

    // Проверяем видимость bounding box'а
    if (node->tile && node->tile->isVisible) {
        if (frustum.Intersects(node->tile->boundingBox)) {
            visibleTiles.push_back(node->tile);
        }
    }

    // Рекурсивно проверяем дочерние узлы
    if (!node->isLeaf) {
        for (int i = 0; i < 4; ++i) {
            CollectVisibleTiles(node->children[i].get(), frustum, visibleTiles);
        }
    }
}
void Terrain::UpdateLOD(const XMFLOAT3& cameraPos, float lodTransitionDistance) 
{
    UpdateNodeLOD(mRoot.get(), cameraPos, lodTransitionDistance);
}
// Обновление LOD на основе расстояния до камеры
void Terrain::UpdateNodeLOD(QuadTreeNode* node, const XMFLOAT3& cameraPos, float lodTransitionDistance)
{
    if (!node || !node->tile) return;

    // Вычисляем расстояние от камеры до центра тайла
    XMFLOAT3 tileCenter = node->tile->worldPos;
    float distance = XMVectorGetX(XMVector3Length(XMLoadFloat3(&tileCenter) - XMLoadFloat3(&cameraPos)));


    // Определяем должен ли узел быть разбит дальше
    bool shouldSplit = (distance < lodTransitionDistance) && (node->depth < mMaxLOD);

    if (shouldSplit && node->isLeaf) {
        // Нужно разбить узел - создаем дочерние узлы
        node->isLeaf = false;
        float childSize = node->size * 0.5f;

        //for (int i = 0; i < 4; ++i) {
        //    if (!node->children[i]) {
        //        node->children[i] = std::make_unique<QuadTreeNode>();
        //        // ... инициализация дочернего узла аналогично BuildNode
        //    }
        //}

        // Скрываем родительский тайл
        node->tile->isVisible = false;
    }
    else if (!shouldSplit && !node->isLeaf) {
        // Нужно свернуть узел - удаляем дочерние узлы
        //node->isLeaf = true;
        //for (int i = 0; i < 4; ++i) {
        //    //node->children[i]//.reset();
        //}

        // Показываем родительский тайл
        node->tile->isVisible = true;
    }

    // Рекурсивно обновляем дочерние узлы
    if (!node->isLeaf) {
        for (int i = 0; i < 4; ++i) {
            UpdateNodeLOD(node->children[i].get(), cameraPos, lodTransitionDistance);
        }
    }
}