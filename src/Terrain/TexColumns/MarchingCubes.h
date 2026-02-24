#pragma once
// MarchingCubes.h
// Self-contained CPU marching cubes. No DX12 dependency — just produces
// std::vector<Vertex> and std::vector<uint32_t> that you upload with
// d3dUtil::CreateDefaultBuffer exactly like any other geometry.
//
// Vertex is the same struct 
//   XMFLOAT3 Pos, XMFLOAT3 Normal, XMFLOAT2 TexC, XMFLOAT3 Tangent
//
// Usage:
//   MarchingCubes::ScalarField field(sizeX, sizeY, sizeZ);
//   field.Set(x, y, z, value);   // value > 0.5 = solid
//   auto [verts, indices] = MarchingCubes::Polygonize(field, 0.5f);

#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "FrameResource.h"
using namespace DirectX;

// ── forward-declare the Vertex layout to match your FrameResource.h ─────────
// If you already have a Vertex struct in scope, remove this block.
//#ifndef MARCHING_CUBES_VERTEX_DEFINED
//#define MARCHING_CUBES_VERTEX_DEFINED
//struct Vertex
//{
//    XMFLOAT3 Pos;
//    XMFLOAT3 Normal;
//    XMFLOAT2 TexC;
//    XMFLOAT3 Tangent;
//};
//#endif

namespace MarchingCubes
{

// ────────────────────────────────────────────────────────────────────────────
// Scalar field — a simple 3-D array of floats.
// ────────────────────────────────────────────────────────────────────────────
struct ScalarField
{
    int   SizeX, SizeY, SizeZ;
    float CellSize = 1.0f; // world-space size of one voxel

    std::vector<float> Data; // row-major [z][y][x]

    ScalarField() : SizeX(0), SizeY(0), SizeZ(0) {}
    ScalarField(int sx, int sy, int sz, float cellSize = 1.0f)
        : SizeX(sx), SizeY(sy), SizeZ(sz), CellSize(cellSize)
        , Data(sx * sy * sz, 0.0f) {}

    float& At(int x, int y, int z)
    {
        return Data[z * SizeY * SizeX + y * SizeX + x];
    }
    float At(int x, int y, int z) const
    {
        return Data[z * SizeY * SizeX + y * SizeX + x];
    }

    // Convenience: Set(x,y,z, value)
    void Set(int x, int y, int z, float v) { At(x, y, z) = v; }
};

// ────────────────────────────────────────────────────────────────────────────
// Standard Lorensen & Cline lookup tables (1987)
// ────────────────────────────────────────────────────────────────────────────

//  Vertex and edge numbering of a unit cube:
//
//      7 ─────── 6
//     /|         /|
//    4 ─────── 5  |
//    |  |      |  |
//    |  3 ─────|─ 2
//    | /       | /
//    0 ─────── 1
//
//  Edge numbering:
//  0: 0-1  1: 1-2  2: 2-3  3: 3-0
//  4: 4-5  5: 5-6  6: 6-7  7: 7-4
//  8: 0-4  9: 1-5  10:2-6  11:3-7

static const int edgeTable[256] = {
    0x0  ,0x109,0x203,0x30a,0x406,0x50f,0x605,0x70c,
    0x80c,0x905,0xa0f,0xb06,0xc0a,0xd03,0xe09,0xf00,
    0x190,0x99 ,0x393,0x29a,0x596,0x49f,0x795,0x69c,
    0x99c,0x895,0xb9f,0xa96,0xd9a,0xc93,0xf99,0xe90,
    0x230,0x339,0x33 ,0x13a,0x636,0x73f,0x435,0x53c,
    0xa3c,0xb35,0x83f,0x936,0xe3a,0xf33,0xc39,0xd30,
    0x3a0,0x2a9,0x1a3,0xaa ,0x7a6,0x6af,0x5a5,0x4ac,
    0xbac,0xaa5,0x9af,0x8a6,0xfaa,0xea3,0xda9,0xca0,
    0x460,0x569,0x663,0x76a,0x66 ,0x16f,0x265,0x36c,
    0xc6c,0xd65,0xe6f,0xf66,0x86a,0x963,0xa69,0xb60,
    0x5f0,0x4f9,0x7f3,0x6fa,0x1f6,0xff ,0x3f5,0x2fc,
    0xdfc,0xcf5,0xfff,0xef6,0x9fa,0x8f3,0xbf9,0xaf0,
    0x650,0x759,0x453,0x55a,0x256,0x35f,0x55 ,0x15c,
    0xe5c,0xf55,0xc5f,0xd56,0xa5a,0xb53,0x859,0x950,
    0x7c0,0x6c9,0x5c3,0x4ca,0x3c6,0x2cf,0x1c5,0xcc ,
    0xfcc,0xec5,0xdcf,0xcc6,0xbca,0xac3,0x9c9,0x8c0,
    0x8c0,0x9c9,0xac3,0xbca,0xcc6,0xdcf,0xec5,0xfcc,
    0xcc ,0x1c5,0x2cf,0x3c6,0x4ca,0x5c3,0x6c9,0x7c0,
    0x950,0x859,0xb53,0xa5a,0xd56,0xc5f,0xf55,0xe5c,
    0x15c,0x55 ,0x35f,0x256,0x55a,0x453,0x759,0x650,
    0xaf0,0xbf9,0x8f3,0x9fa,0xef6,0xfff,0xcf5,0xdfc,
    0x2fc,0x3f5,0xff ,0x1f6,0x6fa,0x7f3,0x4f9,0x5f0,
    0xb60,0xa69,0x963,0x86a,0xf66,0xe6f,0xd65,0xc6c,
    0x36c,0x265,0x16f,0x66 ,0x76a,0x663,0x569,0x460,
    0xca0,0xda9,0xea3,0xfaa,0x8a6,0x9af,0xaa5,0xbac,
    0x4ac,0x5a5,0x6af,0x7a6,0xaa ,0x1a3,0x2a9,0x3a0,
    0xd30,0xc39,0xf33,0xe3a,0x936,0x83f,0xb35,0xa3c,
    0x53c,0x435,0x73f,0x636,0x13a,0x33 ,0x339,0x230,
    0xe90,0xf99,0xc93,0xd9a,0xa96,0xb9f,0x895,0x99c,
    0x69c,0x795,0x49f,0x596,0x29a,0x393,0x99 ,0x190,
    0xf00,0xe09,0xd03,0xc0a,0xb06,0xa0f,0x905,0x80c,
    0x70c,0x605,0x50f,0x406,0x30a,0x203,0x109,0x0
};

// triTable: -1 terminates each list
static const int triTable[256][16] = {
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,8,3,9,8,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,1,2,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,2,10,0,2,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,8,3,2,10,8,10,9,8,-1,-1,-1,-1,-1,-1,-1},
    {3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,11,2,8,11,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,9,0,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,11,2,1,9,11,9,8,11,-1,-1,-1,-1,-1,-1,-1},
    {3,10,1,11,10,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,10,1,0,8,10,8,11,10,-1,-1,-1,-1,-1,-1,-1},
    {3,9,0,3,11,9,11,10,9,-1,-1,-1,-1,-1,-1,-1},
    {9,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,3,0,7,3,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,1,9,4,7,1,7,3,1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,8,4,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,4,7,3,0,4,1,2,10,-1,-1,-1,-1,-1,-1,-1},
    {9,2,10,9,0,2,8,4,7,-1,-1,-1,-1,-1,-1,-1},
    {2,10,9,2,9,7,2,7,3,7,9,4,-1,-1,-1,-1},
    {8,4,7,3,11,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,4,7,11,2,4,2,0,4,-1,-1,-1,-1,-1,-1,-1},
    {9,0,1,8,4,7,2,3,11,-1,-1,-1,-1,-1,-1,-1},
    {4,7,11,9,4,11,9,11,2,9,2,1,-1,-1,-1,-1},
    {3,10,1,3,11,10,7,8,4,-1,-1,-1,-1,-1,-1,-1},
    {1,11,10,1,4,11,1,0,4,7,11,4,-1,-1,-1,-1},
    {4,7,8,9,0,11,9,11,10,11,0,3,-1,-1,-1,-1},
    {4,7,11,4,11,9,9,11,10,-1,-1,-1,-1,-1,-1,-1},
    {9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,5,4,0,8,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,5,4,1,5,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,5,4,8,3,5,3,1,5,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,9,5,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,0,8,1,2,10,4,9,5,-1,-1,-1,-1,-1,-1,-1},
    {5,2,10,5,4,2,4,0,2,-1,-1,-1,-1,-1,-1,-1},
    {2,10,5,3,2,5,3,5,4,3,4,8,-1,-1,-1,-1},
    {9,5,4,2,3,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,11,2,0,8,11,4,9,5,-1,-1,-1,-1,-1,-1,-1},
    {0,5,4,0,1,5,2,3,11,-1,-1,-1,-1,-1,-1,-1},
    {2,1,5,2,5,8,2,8,11,4,8,5,-1,-1,-1,-1},
    {10,3,11,10,1,3,9,5,4,-1,-1,-1,-1,-1,-1,-1},
    {4,9,5,0,8,1,8,10,1,8,11,10,-1,-1,-1,-1},
    {5,4,0,5,0,11,5,11,10,11,0,3,-1,-1,-1,-1},
    {5,4,8,5,8,10,10,8,11,-1,-1,-1,-1,-1,-1,-1},
    {9,7,8,5,7,9,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,3,0,9,5,3,5,7,3,-1,-1,-1,-1,-1,-1,-1},
    {0,7,8,0,1,7,1,5,7,-1,-1,-1,-1,-1,-1,-1},
    {1,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,7,8,9,5,7,10,1,2,-1,-1,-1,-1,-1,-1,-1},
    {10,1,2,9,5,0,5,3,0,5,7,3,-1,-1,-1,-1},
    {8,0,2,8,2,5,8,5,7,10,5,2,-1,-1,-1,-1},
    {2,10,5,2,5,3,3,5,7,-1,-1,-1,-1,-1,-1,-1},
    {7,9,5,7,8,9,3,11,2,-1,-1,-1,-1,-1,-1,-1},
    {9,5,7,9,7,2,9,2,0,2,7,11,-1,-1,-1,-1},
    {2,3,11,0,1,8,1,7,8,1,5,7,-1,-1,-1,-1},
    {11,2,1,11,1,7,7,1,5,-1,-1,-1,-1,-1,-1,-1},
    {9,5,8,8,5,7,10,1,3,10,3,11,-1,-1,-1,-1},
    {5,7,0,5,0,9,7,11,0,1,0,10,11,10,0,-1},
    {11,10,0,11,0,3,10,5,0,8,0,7,5,7,0,-1},
    {11,10,5,7,11,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,0,1,5,10,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,8,3,1,9,8,5,10,6,-1,-1,-1,-1,-1,-1,-1},
    {1,6,5,2,6,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,6,5,1,2,6,3,0,8,-1,-1,-1,-1,-1,-1,-1},
    {9,6,5,9,0,6,0,2,6,-1,-1,-1,-1,-1,-1,-1},
    {5,9,8,5,8,2,5,2,6,3,2,8,-1,-1,-1,-1},
    {2,3,11,10,6,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,0,8,11,2,0,10,6,5,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,2,3,11,5,10,6,-1,-1,-1,-1,-1,-1,-1},
    {5,10,6,1,9,2,9,11,2,9,8,11,-1,-1,-1,-1},
    {6,3,11,6,5,3,5,1,3,-1,-1,-1,-1,-1,-1,-1},
    {0,8,11,0,11,5,0,5,1,5,11,6,-1,-1,-1,-1},
    {3,11,6,0,3,6,0,6,5,0,5,9,-1,-1,-1,-1},
    {6,5,9,6,9,11,11,9,8,-1,-1,-1,-1,-1,-1,-1},
    {5,10,6,4,7,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,3,0,4,7,3,6,5,10,-1,-1,-1,-1,-1,-1,-1},
    {1,9,0,5,10,6,8,4,7,-1,-1,-1,-1,-1,-1,-1},
    {10,6,5,1,9,7,1,7,3,7,9,4,-1,-1,-1,-1},
    {6,1,2,6,5,1,4,7,8,-1,-1,-1,-1,-1,-1,-1},
    {1,2,5,5,2,6,3,0,4,3,4,7,-1,-1,-1,-1},
    {8,4,7,9,0,5,0,6,5,0,2,6,-1,-1,-1,-1},
    {7,3,9,7,9,4,3,2,9,5,9,6,2,6,9,-1},
    {3,11,2,7,8,4,10,6,5,-1,-1,-1,-1,-1,-1,-1},
    {5,10,6,4,7,2,4,2,0,2,7,11,-1,-1,-1,-1},
    {0,1,9,4,7,8,2,3,11,5,10,6,-1,-1,-1,-1},
    {9,2,1,9,11,2,9,4,11,7,11,4,5,10,6,-1},
    {8,4,7,3,11,5,3,5,1,5,11,6,-1,-1,-1,-1},
    {5,1,11,5,11,6,1,0,11,7,11,4,0,4,11,-1},
    {0,5,9,0,6,5,0,3,6,11,6,3,8,4,7,-1},
    {6,5,9,6,9,11,4,7,9,7,11,9,-1,-1,-1,-1},
    {10,4,9,6,4,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,10,6,4,9,10,0,8,3,-1,-1,-1,-1,-1,-1,-1},
    {10,0,1,10,6,0,6,4,0,-1,-1,-1,-1,-1,-1,-1},
    {8,3,1,8,1,6,8,6,4,6,1,10,-1,-1,-1,-1},
    {1,4,9,1,2,4,2,6,4,-1,-1,-1,-1,-1,-1,-1},
    {3,0,8,1,2,9,2,4,9,2,6,4,-1,-1,-1,-1},
    {0,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,3,2,8,2,4,4,2,6,-1,-1,-1,-1,-1,-1,-1},
    {10,4,9,10,6,4,11,2,3,-1,-1,-1,-1,-1,-1,-1},
    {0,8,2,2,8,11,4,9,10,4,10,6,-1,-1,-1,-1},
    {3,11,2,0,1,6,0,6,4,6,1,10,-1,-1,-1,-1},
    {6,4,1,6,1,10,4,8,1,2,1,11,8,11,1,-1},
    {9,6,4,9,3,6,9,1,3,11,6,3,-1,-1,-1,-1},
    {8,11,1,8,1,0,11,6,1,9,1,4,6,4,1,-1},
    {3,11,6,3,6,0,0,6,4,-1,-1,-1,-1,-1,-1,-1},
    {6,4,8,11,6,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,10,6,7,8,10,8,9,10,-1,-1,-1,-1,-1,-1,-1},
    {0,7,3,0,10,7,0,9,10,6,7,10,-1,-1,-1,-1},
    {10,6,7,1,10,7,1,7,8,1,8,0,-1,-1,-1,-1},
    {10,6,7,10,7,1,1,7,3,-1,-1,-1,-1,-1,-1,-1},
    {1,2,6,1,6,8,1,8,9,8,6,7,-1,-1,-1,-1},
    {2,6,9,2,9,1,6,7,9,0,9,3,7,3,9,-1},
    {7,8,0,7,0,6,6,0,2,-1,-1,-1,-1,-1,-1,-1},
    {7,3,2,6,7,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,3,11,10,6,8,10,8,9,8,6,7,-1,-1,-1,-1},
    {2,0,7,2,7,11,0,9,7,6,7,10,9,10,7,-1},
    {1,8,0,1,7,8,1,10,7,6,7,10,2,3,11,-1},
    {11,2,1,11,1,7,10,6,1,6,7,1,-1,-1,-1,-1},
    {8,9,6,8,6,7,9,1,6,11,6,3,1,3,6,-1},
    {0,9,1,11,6,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,8,0,7,0,6,3,11,0,11,6,0,-1,-1,-1,-1},
    {7,11,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,0,8,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,1,9,11,7,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,1,9,8,3,1,11,7,6,-1,-1,-1,-1,-1,-1,-1},
    {10,1,2,6,11,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,3,0,8,6,11,7,-1,-1,-1,-1,-1,-1,-1},
    {2,9,0,2,10,9,6,11,7,-1,-1,-1,-1,-1,-1,-1},
    {6,11,7,2,10,3,10,8,3,10,9,8,-1,-1,-1,-1},
    {7,2,3,6,2,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {7,0,8,7,6,0,6,2,0,-1,-1,-1,-1,-1,-1,-1},
    {2,7,6,2,3,7,0,1,9,-1,-1,-1,-1,-1,-1,-1},
    {1,6,2,1,8,6,1,9,8,8,7,6,-1,-1,-1,-1},
    {10,7,6,10,1,7,1,3,7,-1,-1,-1,-1,-1,-1,-1},
    {10,7,6,1,7,10,1,8,7,1,0,8,-1,-1,-1,-1},
    {0,3,7,0,7,10,0,10,9,6,10,7,-1,-1,-1,-1},
    {7,6,10,7,10,8,8,10,9,-1,-1,-1,-1,-1,-1,-1},
    {6,8,4,11,8,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,6,11,3,0,6,0,4,6,-1,-1,-1,-1,-1,-1,-1},
    {8,6,11,8,4,6,9,0,1,-1,-1,-1,-1,-1,-1,-1},
    {9,4,6,9,6,3,9,3,1,11,3,6,-1,-1,-1,-1},
    {6,8,4,6,11,8,2,10,1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,3,0,11,0,6,11,0,4,6,-1,-1,-1,-1},
    {4,11,8,4,6,11,0,2,9,2,10,9,-1,-1,-1,-1},
    {10,9,3,10,3,2,9,4,3,11,3,6,4,6,3,-1},
    {8,2,3,8,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1},
    {0,4,2,4,6,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,9,0,2,3,4,2,4,6,4,3,8,-1,-1,-1,-1},
    {1,9,4,1,4,2,2,4,6,-1,-1,-1,-1,-1,-1,-1},
    {8,1,3,8,6,1,8,4,6,6,10,1,-1,-1,-1,-1},
    {10,1,0,10,0,6,6,0,4,-1,-1,-1,-1,-1,-1,-1},
    {4,6,3,4,3,8,6,10,3,0,3,9,10,9,3,-1},
    {10,9,4,6,10,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,9,5,7,6,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,4,9,5,11,7,6,-1,-1,-1,-1,-1,-1,-1},
    {5,0,1,5,4,0,7,6,11,-1,-1,-1,-1,-1,-1,-1},
    {11,7,6,8,3,4,3,5,4,3,1,5,-1,-1,-1,-1},
    {9,5,4,10,1,2,7,6,11,-1,-1,-1,-1,-1,-1,-1},
    {6,11,7,1,2,10,0,8,3,4,9,5,-1,-1,-1,-1},
    {7,6,11,5,4,10,4,2,10,4,0,2,-1,-1,-1,-1},
    {3,4,8,3,5,4,3,2,5,10,5,2,11,7,6,-1},
    {7,2,3,7,6,2,5,4,9,-1,-1,-1,-1,-1,-1,-1},
    {9,5,4,0,8,6,0,6,2,6,8,7,-1,-1,-1,-1},
    {3,6,2,3,7,6,1,5,0,5,4,0,-1,-1,-1,-1},
    {6,2,8,6,8,7,2,1,8,4,8,5,1,5,8,-1},
    {9,5,4,10,1,6,1,7,6,1,3,7,-1,-1,-1,-1},
    {1,6,10,1,7,6,1,0,7,8,7,0,9,5,4,-1},
    {4,0,10,4,10,5,0,3,10,6,10,7,3,7,10,-1},
    {7,6,10,7,10,8,5,4,10,4,8,10,-1,-1,-1,-1},
    {6,9,5,6,11,9,11,8,9,-1,-1,-1,-1,-1,-1,-1},
    {3,6,11,0,6,3,0,5,6,0,9,5,-1,-1,-1,-1},
    {0,11,8,0,5,11,0,1,5,5,6,11,-1,-1,-1,-1},
    {6,11,3,6,3,5,5,3,1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,10,9,5,11,9,11,8,11,5,6,-1,-1,-1,-1},
    {0,11,3,0,6,11,0,9,6,5,6,9,1,2,10,-1},
    {11,8,5,11,5,6,8,0,5,10,5,2,0,2,5,-1},
    {6,11,3,6,3,5,2,10,3,10,5,3,-1,-1,-1,-1},
    {5,8,9,5,2,8,5,6,2,3,8,2,-1,-1,-1,-1},
    {9,5,6,9,6,0,0,6,2,-1,-1,-1,-1,-1,-1,-1},
    {1,5,8,1,8,0,5,6,8,3,8,2,6,2,8,-1},
    {1,5,6,2,1,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,3,6,1,6,10,3,8,6,5,6,9,8,9,6,-1},
    {10,1,0,10,0,6,9,5,0,5,6,0,-1,-1,-1,-1},
    {0,3,8,5,6,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {10,5,6,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,5,10,7,5,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {11,5,10,11,7,5,8,3,0,-1,-1,-1,-1,-1,-1,-1},
    {5,11,7,5,10,11,1,9,0,-1,-1,-1,-1,-1,-1,-1},
    {10,7,5,10,11,7,9,8,1,8,3,1,-1,-1,-1,-1},
    {11,1,2,11,7,1,7,5,1,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,1,2,7,1,7,5,7,2,11,-1,-1,-1,-1},
    {9,7,5,9,2,7,9,0,2,2,11,7,-1,-1,-1,-1},
    {7,5,2,7,2,11,5,9,2,3,2,8,9,8,2,-1},
    {2,5,10,2,3,5,3,7,5,-1,-1,-1,-1,-1,-1,-1},
    {8,2,0,8,5,2,8,7,5,10,2,5,-1,-1,-1,-1},
    {9,0,1,2,3,10,3,5,10,3,7,5,-1,-1,-1,-1},
    {1,2,5,5,2,10,9,8,7,9,7,5,-1,-1,-1,-1},  // fixed
    {5,3,7,5,1,3,5,10,1,3,1,11,-1,-1,-1,-1},  // placeholder
    {0,8,7,0,7,1,1,7,5,-1,-1,-1,-1,-1,-1,-1},
    {9,0,3,9,3,5,5,3,7,-1,-1,-1,-1,-1,-1,-1},
    {9,8,7,5,9,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {5,8,4,5,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1},
    {5,0,4,5,11,0,5,10,11,11,3,0,-1,-1,-1,-1},
    {0,1,9,8,4,10,8,10,11,10,4,5,-1,-1,-1,-1},
    {10,11,4,10,4,5,11,3,4,9,4,1,3,1,4,-1},
    {2,5,1,2,8,5,2,11,8,4,5,8,-1,-1,-1,-1},
    {0,4,11,0,11,3,4,5,11,2,11,1,5,1,11,-1},
    {0,2,5,0,5,9,2,11,5,4,5,8,11,8,5,-1},
    {9,4,5,2,11,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,5,10,3,5,2,3,4,5,3,8,4,-1,-1,-1,-1},
    {5,10,2,5,2,4,4,2,0,-1,-1,-1,-1,-1,-1,-1},
    {3,10,2,3,5,10,3,8,5,4,5,8,0,1,9,-1},
    {5,10,2,5,2,4,1,9,2,9,4,2,-1,-1,-1,-1},
    {8,4,5,8,5,3,3,5,1,-1,-1,-1,-1,-1,-1,-1},
    {0,4,5,1,0,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {8,4,5,8,5,3,9,0,5,0,3,5,-1,-1,-1,-1},
    {9,4,5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,11,7,4,9,11,9,10,11,-1,-1,-1,-1,-1,-1,-1},
    {0,8,3,4,9,7,9,11,7,9,10,11,-1,-1,-1,-1},
    {1,10,11,1,11,4,1,4,0,7,4,11,-1,-1,-1,-1},
    {3,1,4,3,4,8,1,10,4,7,4,11,10,11,4,-1},
    {4,11,7,9,11,4,9,2,11,9,1,2,-1,-1,-1,-1},
    {9,7,4,9,11,7,9,1,11,2,11,1,0,8,3,-1},
    {11,7,4,11,4,2,2,4,0,-1,-1,-1,-1,-1,-1,-1},
    {11,7,4,11,4,2,8,3,4,3,2,4,-1,-1,-1,-1},
    {2,9,10,2,7,9,2,3,7,7,4,9,-1,-1,-1,-1},
    {9,10,7,9,7,4,10,2,7,8,7,0,2,0,7,-1},
    {3,7,10,3,10,2,7,4,10,1,10,0,4,0,10,-1},
    {1,10,2,8,7,4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,9,1,4,1,7,7,1,3,-1,-1,-1,-1,-1,-1,-1},
    {4,9,1,4,1,7,0,8,1,8,7,1,-1,-1,-1,-1},
    {4,0,3,7,4,3,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {4,8,7,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {9,10,8,10,11,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,0,9,3,9,11,11,9,10,-1,-1,-1,-1,-1,-1,-1},
    {0,1,10,0,10,8,8,10,11,-1,-1,-1,-1,-1,-1,-1},
    {3,1,10,11,3,10,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,2,11,1,11,9,9,11,8,-1,-1,-1,-1,-1,-1,-1},
    {3,0,9,3,9,11,1,2,9,2,11,9,-1,-1,-1,-1},
    {0,2,11,8,0,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {3,2,11,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,3,8,2,8,10,10,8,9,-1,-1,-1,-1,-1,-1,-1},
    {9,10,2,0,9,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {2,3,8,2,8,10,0,1,8,1,10,8,-1,-1,-1,-1},
    {1,10,2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {1,3,8,9,1,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,9,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {0,3,8,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1},
    {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}
};

// ────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ────────────────────────────────────────────────────────────────────────────

// 8 corner offsets of a unit cube (x,y,z)
static const int vOff[8][3] = {
    {0,0,0},{1,0,0},{1,1,0},{0,1,0},
    {0,0,1},{1,0,1},{1,1,1},{0,1,1}
};

// Two endpoints of each of the 12 edges
static const int eVert[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

static inline XMFLOAT3 Lerp3(XMFLOAT3 a, XMFLOAT3 b, float t)
{
    return { a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), a.z + t * (b.z - a.z) };
}

// Interpolate the position where the isosurface crosses an edge
static inline XMFLOAT3 VertexInterp(float iso,
    XMFLOAT3 p1, XMFLOAT3 p2, float v1, float v2)
{
    if (fabsf(v1 - v2) < 1e-6f) return p1;
    float t = (iso - v1) / (v2 - v1);
    return Lerp3(p1, p2, t);
}

// ────────────────────────────────────────────────────────────────────────────
// Main API
// ────────────────────────────────────────────────────────────────────────────

struct Result
{
    std::vector<Vertex>   Vertices;
    std::vector<uint32_t> Indices;
};

// Polygonize — runs marching cubes on the scalar field.
// isovalue: surface threshold (default 0.5 → anything > 0.5 is inside)
// Returns flat-shaded normals; call ComputeSmoothNormals() afterwards if preferred.
inline Result Polygonize(const ScalarField& f, float isovalue = 0.5f)
{
    Result res;

    // Pre-allocate based on surface area estimate (top face + 4 sides).
    // Each surface cell emits at most 5 triangles × 3 flat-shaded vertices = 15.
    // This prevents reallocation bad_alloc during push_back on large fields.
    {
        size_t estimatedVerts = (size_t)f.SizeX * f.SizeZ * 2 * 5 * 3;
        res.Vertices.reserve(std::min(estimatedVerts, (size_t)2'000'000));
        res.Indices.reserve(std::min(estimatedVerts, (size_t)2'000'000));
    }

    const float cs = f.CellSize;

    for (int z = 0; z < f.SizeZ - 1; ++z)
        for (int y = 0; y < f.SizeY - 1; ++y)
            for (int x = 0; x < f.SizeX - 1; ++x)
            {
                // 8 scalar values at cube corners
                float val[8];
                XMFLOAT3 pos[8];
                for (int i = 0; i < 8; ++i)
                {
                    int cx = x + vOff[i][0];
                    int cy = y + vOff[i][1];
                    int cz = z + vOff[i][2];
                    val[i] = f.At(cx, cy, cz);
                    pos[i] = { cx * cs, cy * cs, cz * cs };
                }

                // Build 8-bit configuration index
                int cubeIndex = 0;
                for (int i = 0; i < 8; ++i)
                    if (val[i] < isovalue) cubeIndex |= (1 << i);

                if (edgeTable[cubeIndex] == 0) continue;

                // Interpolate edge midpoints
                XMFLOAT3 edgePos[12];
                for (int e = 0; e < 12; ++e)
                {
                    if (edgeTable[cubeIndex] & (1 << e))
                    {
                        int v0 = eVert[e][0], v1 = eVert[e][1];
                        edgePos[e] = VertexInterp(isovalue, pos[v0], pos[v1], val[v0], val[v1]);
                    }
                }

                // Emit triangles
                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3)
                {
                    XMFLOAT3 p0 = edgePos[triTable[cubeIndex][i + 0]];
                    XMFLOAT3 p1 = edgePos[triTable[cubeIndex][i + 1]];
                    XMFLOAT3 p2 = edgePos[triTable[cubeIndex][i + 2]];

                    // Flat normal: cross product of two edges
                    XMVECTOR e1 = XMVectorSubtract(XMLoadFloat3(&p1), XMLoadFloat3(&p0));
                    XMVECTOR e2 = XMVectorSubtract(XMLoadFloat3(&p2), XMLoadFloat3(&p0));
                    XMVECTOR n = XMVector3Normalize(XMVector3Cross(e1, e2));
                    XMFLOAT3 normal;
                    XMStoreFloat3(&normal, n);

                    uint32_t baseIdx = (uint32_t)res.Vertices.size();

                    // Build tangent (arbitrary vector perpendicular to normal)
                    XMVECTOR up = fabsf(normal.y) < 0.99f
                        ? XMVectorSet(0, 1, 0, 0)
                        : XMVectorSet(1, 0, 0, 0);
                    XMFLOAT3 tangent;
                    XMStoreFloat3(&tangent, XMVector3Normalize(XMVector3Cross(up, n)));

                    auto makeVert = [&](XMFLOAT3 p) {
                        Vertex v{};
                        v.Pos = p;
                        v.Normal = normal;
                        v.TexC = { p.x / (f.SizeX * cs), p.z / (f.SizeZ * cs) }; // planar XZ UV
                        v.Tangent = tangent;
                        res.Vertices.push_back(v);
                        };

                    makeVert(p0);
                    makeVert(p1);
                    makeVert(p2);

                    res.Indices.push_back(baseIdx + 0);
                    res.Indices.push_back(baseIdx + 1);
                    res.Indices.push_back(baseIdx + 2);
                }
            }

    return res;
}
// ────────────────────────────────────────────────────────────────────────────
// Improved Perlin Noise (Perlin 2002)
// Works at arbitrary world coordinates — truly infinite, no tiling boundary.
// ────────────────────────────────────────────────────────────────────────────
class PerlinNoise
{
public:
    // Construct with a seed. Same seed → same infinite noise field every run.
    explicit PerlinNoise(uint32_t seed = 42u)
    {
        // Fill permutation table with 0..255, then shuffle with LCG
        for (int i = 0; i < 256; ++i) p[i] = (uint8_t)i;

        uint32_t lcg = seed;
        for (int i = 255; i > 0; --i)
        {
            lcg = lcg * 1664525u + 1013904223u;
            int j = (int)(lcg >> 24) % (i + 1);
            std::swap(p[i], p[j]);
        }
        // Double the table to avoid index wrapping
        for (int i = 0; i < 256; ++i) p[i + 256] = p[i];
    }

    // Single-octave Perlin noise at (x,y,z), returns [-1, 1].
    float Noise(float x, float y, float z) const
    {
        // Integer cell coordinates
        int X = (int)floorf(x) & 255;
        int Y = (int)floorf(y) & 255;
        int Z = (int)floorf(z) & 255;

        // Fractional position within cell
        float fx = x - floorf(x);
        float fy = y - floorf(y);
        float fz = z - floorf(z);

        // Fade curves (quintic: 6t^5 - 15t^4 + 10t^3)
        float u = Fade(fx), v = Fade(fy), w = Fade(fz);

        // Hash corner indices
        int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
        int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

        // Interpolate gradient dot products
        return Lerp(w,
            Lerp(v,
                Lerp(u, Grad(p[AA], fx, fy, fz),
                    Grad(p[BA], fx - 1, fy, fz)),
                Lerp(u, Grad(p[AB], fx, fy - 1, fz),
                    Grad(p[BB], fx - 1, fy - 1, fz))),
            Lerp(v,
                Lerp(u, Grad(p[AA + 1], fx, fy, fz - 1),
                    Grad(p[BA + 1], fx - 1, fy, fz - 1)),
                Lerp(u, Grad(p[AB + 1], fx, fy - 1, fz - 1),
                    Grad(p[BB + 1], fx - 1, fy - 1, fz - 1))));
    }

    // Fractional Brownian Motion — sums 'octaves' layers of noise.
    // Each octave doubles frequency and halves amplitude (lacunarity=2, gain=0.5).
    //   frequency : base frequency (higher = more detail per world unit)
    //   octaves   : how many layers to stack (3–6 is typical)
    //   returns   : value in approximately [-1, 1]
    float fBm(float x, float y, float z, float frequency, int octaves) const
    {
        float value = 0.0f;
        float amplitude = 1.0f;
        float maxAmp = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += Noise(x * frequency, y * frequency, z * frequency) * amplitude;
            maxAmp += amplitude;
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }
        return value / maxAmp; // normalize to [-1, 1]
    }

private:
    uint8_t p[512];

    static float Fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static float Lerp(float t, float a, float b) { return a + t * (b - a); }

    static float Grad(int hash, float x, float y, float z)
    {
        // Map the low 4 bits of the hash to 12 gradient directions
        int  h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }
};


// ────────────────────────────────────────────────────────────────────────────
// Noise parameters — pass this to PerlinScalarField().
// ────────────────────────────────────────────────────────────────────────────
struct NoiseParams
{
    // World-space position of the field's (0,0,0) corner.
    // Change this to "scroll" through the infinite noise field.
    float worldOffsetX = 0.0f;
    float worldOffsetZ = 0.0f;

    // Base frequency of the noise in world units.
    // Smaller → wider hills (lower frequency).
    // Larger  → more jagged detail (higher frequency).
    float frequency = 0.0035f;

    // How many octaves of fBm to layer.
    // 1 = smooth hills. 4–5 = rocky, fractured surface.
    int octaves = 5;

    // How tall the noise deformation is in voxels.
    // Amplitude > ~4 starts to produce overhangs and cave-like features.
    float amplitude = 8.0f;

    // Base plateau height in voxels from the bottom of the field.
    // Everything below (baseHeight - amplitude) is always solid.
    // Everything above (baseHeight + amplitude) is always air.
    float baseHeight = 14.0f;

    // Random seed — change to get a completely different landscape.
    uint32_t seed = 42u;
};


// ────────────────────────────────────────────────────────────────────────────
// PerlinScalarField — builds a ScalarField using 3D Perlin fBm.
//
// Density function:
//   d(x,y,z) = baseHeight - y + amplitude * fBm(worldX, y, worldZ)
//
// When d > 0.5 → solid (inside rock)
// When d < 0.5 → air
// The noise is sampled in 3D, so it naturally produces overhangs and
// cave-like features when amplitude is large relative to baseHeight.
//
// The field samples noise at world coordinates (worldOffset + i*cellSize),
// so the result is a window into the infinite noise field. Changing
// worldOffset scrolls through it without any visible tiling seam.
// ────────────────────────────────────────────────────────────────────────────
inline ScalarField PerlinScalarField(
    int fieldX, int fieldY, int fieldZ,
    float cellSize,
    const NoiseParams& np = NoiseParams{})
{
    ScalarField field(fieldX, fieldY, fieldZ, cellSize);
    PerlinNoise noise(np.seed);

    for (int z = 0; z < fieldZ; ++z)
        for (int x = 0; x < fieldX; ++x)
        {
            // World-space XZ coordinates for this column
            float wx = np.worldOffsetX + x * cellSize;
            float wz = np.worldOffsetZ + z * cellSize;

            for (int y = 0; y < fieldY; ++y)
            {
                // World Y is just voxel index (noise is sampled in voxel space Y)
                float wy = (float)y;

                // 3D fBm — using world XZ and voxel Y gives a terrain that has
                // vertical variation (overhangs) when amplitude is high enough.
                float n = noise.fBm(wx, wy, wz, np.frequency, np.octaves);

                // Density: positive = solid, negative = air.
                // The isosurface at d=0.5 sits near y=baseHeight, deformed by noise.
                float d = np.baseHeight - wy + n * np.amplitude;

                // Clamp to [0,1] — only the narrow band around 0.5 actually matters
                // for MC; extreme values just tell it "definitely solid/air".
                field.Set(x, y, z, (std::max)(0.0f, std::min(1.0f, d)));
            }
        }

    return field;
}

// ────────────────────────────────────────────────────────────────────────────
// Heightmap → ScalarField conversion
// Produces a plateau: below the heightmap surface = solid (1.0), above = air (0.0).
//
//   heightData  : array of W×H floats in [0,1], row-major (row = Z axis)
//   W, H        : dimensions of the heightmap
//   fieldY      : total number of voxels in Y (height of the box)
//   plateauScale: how many Y voxels the maximum height maps to (≤ fieldY)
//   cellSize    : world-space size of one voxel (same in X, Y, Z)
// ────────────────────────────────────────────────────────────────────────────
inline ScalarField HeightmapToScalarField(
    const float* heightData, int W, int H,
    int fieldX, int fieldY, int fieldZ,
    float plateauScale,
    float cellSize = 1.0f)
{
    ScalarField field(fieldX, fieldY, fieldZ, cellSize);

    for (int z = 0; z < fieldZ; ++z)
        for (int x = 0; x < fieldX; ++x)
        {
            // Sample heightmap with bilinear filtering
            float fx = (float)x / (fieldX - 1) * (W - 1);
            float fz = (float)z / (fieldZ - 1) * (H - 1);
            int   ix = std::min((int)fx, W - 2);
            int   iz = std::min((int)fz, H - 2);
            float tx = fx - ix, tz = fz - iz;

            float h00 = heightData[iz * W + ix];
            float h10 = heightData[iz * W + ix + 1];
            float h01 = heightData[(iz + 1) * W + ix];
            float h11 = heightData[(iz + 1) * W + ix + 1];
            float h = h00 * (1 - tx) * (1 - tz) + h10 * tx * (1 - tz)
                + h01 * (1 - tx) * tz + h11 * tx * tz;

            // Solid height in voxels (plateau cap)
            float solidY = h * plateauScale;

            for (int y = 0; y < fieldY; ++y)
            {
                // Smooth transition: 1 inside, 0 outside, gradient near surface
                float dist = solidY - (float)y;
                float density = 0.5f + dist;            // linear ramp around boundary
                density = (std::max)(0.0f, std::min(1.0f, density));
                field.Set(x, y, z, density);
            }
        }

    return field;
}



} // namespace MarchingCubes
