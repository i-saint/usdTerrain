#pragma once

constexpr int kMaxUvSets = 4;

template<typename T>
struct PrimvarData
{
    TfToken name;
    VtArray<int> indices;
    VtArray<T> values;
};

struct MeshData
{
    VtArray<int> indices;
    VtArray<int> counts;
    VtArray<GfVec3f> points;

    PrimvarData<GfVec3f> normals;
    PrimvarData<GfVec2f> uvs[kMaxUvSets];
};

// 三角形化。output は indices と counts のみ出力される
bool TriangulateMesh(const MeshData& input, MeshData& output);
