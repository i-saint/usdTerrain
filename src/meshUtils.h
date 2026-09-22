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

    PrimvarData<GfVec3f> normals, binormals, tangents;
    PrimvarData<GfVec4f> colors;
    PrimvarData<GfVec2f> uvs[kMaxUvSets];
};

#define EachMeshPrimvar(Action) \
    Action(normals) \
    Action(binormals) \
    Action(tangents) \
    Action(colors) \
    Action(uvs[0]) \
    Action(uvs[1]) \
    Action(uvs[2]) \
    Action(uvs[3]) \


class Triangulator
{
public:
    using float2 = GfVec2f;
    using float3 = GfVec3f;

    bool EarClipTriangulate(std::span<const float3> points, std::span<const int> indices);
    bool SimpleTriangulate(std::span<const int> indices);
    std::span<const int> GetResult() const { return result; }

private:
    bool DoEarClipTriangulate();

    std::vector<int> polygon;
    std::vector<int> result;
    std::vector<float2> polygon2DVertices;
};


// 三角形化。dst は indices と counts のみ出力される (indices を持つ primvar はそれも dst 側に出力される) 
// line / point は無視される
bool TriangulateMesh(const MeshData& src, MeshData& dst);
