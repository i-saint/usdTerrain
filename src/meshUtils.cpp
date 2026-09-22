#include "pch.h"
#include "meshUtils.h"

#include <algorithm>
#include <unordered_set>

using float2 = GfVec2f;
using float3 = GfVec3f;

#pragma region Triangulation
static float Cross2D(const float2& a, const float2& b, const float2& c)
{
    const float2 ab = b - a;
    const float2 ac = c - a;
    return ab[0] * ac[1] - ab[1] * ac[0];
}

static float SignedArea2D(std::span<const float2> points)
{
    float area = 0.0f;
    const size_t n = points.size();
    for (size_t i = 0; i < n; ++i) {
        const float2& a = points[i];
        const float2& b = points[(i + 1) % n];
        area += a[0] * b[1] - b[0] * a[1];
    }
    return area * 0.5f;
}

static bool PointInTriangle2D(const float2& p, const float2& a, const float2& b, const float2& c)
{
    constexpr float kEps = 1e-7f;
    const float c0 = Cross2D(a, b, p);
    const float c1 = Cross2D(b, c, p);
    const float c2 = Cross2D(c, a, p);
    const bool hasNeg = (c0 < -kEps) || (c1 < -kEps) || (c2 < -kEps);
    const bool hasPos = (c0 > kEps) || (c1 > kEps) || (c2 > kEps);
    return !(hasNeg && hasPos);
}

static float3 ComputePolygonNormal(std::span<const float3> points, std::span<const int> indices)
{
    float3 normal(0.0f, 0.0f, 0.0f);
    const int count = static_cast<int>(indices.size());
    for (int i = 0; i < count; ++i) {
        const int i0 = indices[i];
        const int i1 = indices[(i + 1) % count];
        const float3& p0 = points[i0];
        const float3& p1 = points[i1];
        normal[0] += (p0[1] - p1[1]) * (p0[2] + p1[2]);
        normal[1] += (p0[2] - p1[2]) * (p0[0] + p1[0]);
        normal[2] += (p0[0] - p1[0]) * (p0[1] + p1[1]);
    }
    return normal;
}

enum class ProjectionAxis
{
    kZPositive,
    kZNegative,
    kYPositive,
    kYNegative,
    kXPositive,
    kXNegative,
};

static ProjectionAxis SelectProjectionAxis(const float3& normal)
{
    enum class LongestAxis { kX, kY, kZ };
    LongestAxis longest = LongestAxis::kY;
    const float ax = std::abs(normal[0]);
    const float ay = std::abs(normal[1]);
    const float az = std::abs(normal[2]);
    constexpr float kEps = 1e-7f;

    if ((az - ay) > kEps && (az - ax) > kEps) {
        longest = LongestAxis::kZ;
    }
    else if ((ay - ax) > kEps && (ay - az) > kEps) {
        longest = LongestAxis::kY;
    }
    else if ((ax - ay) > kEps && (ax - az) > kEps) {
        longest = LongestAxis::kX;
    }
    else if ((az - ay) > kEps) {
        longest = LongestAxis::kZ;
    }
    else if ((ax - az) > kEps) {
        longest = LongestAxis::kX;
    }

    switch (longest) {
    case LongestAxis::kZ: return normal[2] < 0.0f ? ProjectionAxis::kZNegative : ProjectionAxis::kZPositive;
    case LongestAxis::kX: return normal[0] < 0.0f ? ProjectionAxis::kXNegative : ProjectionAxis::kXPositive;
    case LongestAxis::kY:
    default: return normal[1] < 0.0f ? ProjectionAxis::kYNegative : ProjectionAxis::kYPositive;
    }
}

static float2 ProjectToPolygon2D(const float3& p, ProjectionAxis axis)
{
    switch (axis) {
    case ProjectionAxis::kZPositive: return float2(p[0], p[1]);
    case ProjectionAxis::kZNegative: return float2(p[0], -p[1]);
    case ProjectionAxis::kYPositive: return float2(-p[0], p[2]);
    case ProjectionAxis::kYNegative: return float2(p[0], p[2]);
    case ProjectionAxis::kXPositive: return float2(p[1], p[2]);
    case ProjectionAxis::kXNegative: return float2(-p[1], p[2]);
    default: return float2(0.0f, 0.0f);
    }
}


// ear clipping 法による三角形化を行うためのコンテキスト
class Triangulator
{
public:
    bool EarClipTriangulate(std::span<const float3> points, std::span<const int> indices);
    bool SimpleTriangulate(std::span<const int> indices);
    std::span<const int> GetTriangleIndices() const { return result; }

private:
    std::vector<int> polygon;
    std::vector<int> result;
    std::vector<float2> polygon2DVertices;
    std::unordered_set<int> triangulatedVertexSet;

    bool DoEarClipTriangulate();
    bool ValidateTriangulation(std::span<const int> indices);
};

bool Triangulator::DoEarClipTriangulate()
{
    result.clear();
    const int n = static_cast<int>(polygon2DVertices.size());
    if (n < 3) {
        return false;
    }
    if (n == 3) {
        result.push_back(0);
        result.push_back(1);
        result.push_back(2);
        return true;
    }

    polygon.resize(n);
    std::iota(polygon.begin(), polygon.end(), 0);

    if (SignedArea2D(polygon2DVertices) < 0.0f) {
        std::reverse(polygon.begin(), polygon.end());
    }

    constexpr float kEps = 1e-7f;
    int guard = 0;
    const int maxGuard = n * n;
    while (polygon.size() > 3 && guard < maxGuard) {
        bool earFound = false;
        const int m = static_cast<int>(polygon.size());
        for (int i = 0; i < m; ++i) {
            const int i0 = polygon[(i + m - 1) % m];
            const int i1 = polygon[i];
            const int i2 = polygon[(i + 1) % m];

            const float2& a = polygon2DVertices[i0];
            const float2& b = polygon2DVertices[i1];
            const float2& c = polygon2DVertices[i2];
            if (Cross2D(a, b, c) <= kEps) {
                continue;
            }

            bool containsOther = false;
            for (int j = 0; j < m; ++j) {
                const int ip = polygon[j];
                if (ip == i0 || ip == i1 || ip == i2) {
                    continue;
                }
                if (PointInTriangle2D(polygon2DVertices[ip], a, b, c)) {
                    containsOther = true;
                    break;
                }
            }

            if (containsOther) {
                continue;
            }

            result.push_back(i0);
            result.push_back(i1);
            result.push_back(i2);
            polygon.erase(polygon.begin() + i);
            earFound = true;
            break;
        }

        if (!earFound) {
            return false;
        }
        ++guard;
    }

    if (polygon.size() != 3) {
        return false;
    }

    result.push_back(polygon[0]);
    result.push_back(polygon[1]);
    result.push_back(polygon[2]);
    return true;
}

bool Triangulator::ValidateTriangulation(std::span<const int> indices)
{
    triangulatedVertexSet.clear();
    triangulatedVertexSet.reserve(result.size());

    for (int index : result) {
        if (index < 0 || index >= indices.size()) {
            return false;
        }
        triangulatedVertexSet.insert(indices[index]);
    }

    for (int i = 0; i < indices.size(); ++i) {
        if (triangulatedVertexSet.find(indices[i]) == triangulatedVertexSet.end()) {
            return false;
        }
    }

    return true;
}

bool Triangulator::EarClipTriangulate(std::span<const float3> points, std::span<const int> indices)
{
    result.clear();
    if (indices.size() < 3) {
        return false;
    }

    for (int i = 0; i < indices.size(); ++i) {
        const int index = indices[i];
        if (index < 0 || index >= points.size()) {
            return false;
        }
    }

    const float3 normal = ComputePolygonNormal(points, indices);
    const ProjectionAxis axis = SelectProjectionAxis(normal);

    polygon2DVertices.clear();
    polygon2DVertices.reserve(indices.size());
    for (int i = 0; i < indices.size(); ++i) {
        polygon2DVertices.push_back(ProjectToPolygon2D(points[indices[i]], axis));
    }

    if (!DoEarClipTriangulate()) {
        return false;
    }
    return ValidateTriangulation(indices);
}

bool Triangulator::SimpleTriangulate(std::span<const int> indices)
{
    result.clear();
    if (indices.size() < 3) {
        return false;
    }
    else if (indices.size() == 3) {
        result.push_back(indices[0]);
        result.push_back(indices[1]);
        result.push_back(indices[2]);
        return true;
    }

    int nextVertex = 3;
    int prevVertex = static_cast<int>(indices.size()) - 1;
    int lastVertex = 2;
    int firstVertex = 0;

    const int triangleCount = static_cast<int>(indices.size()) - 2;
    const int totalPoints = triangleCount * 3;
    int written = 0;

    result.push_back(indices[0]);
    result.push_back(indices[1]);
    result.push_back(indices[2]);
    written += 3;

    while (nextVertex <= prevVertex) {
        result.push_back(indices[lastVertex]);
        result.push_back(indices[nextVertex]);
        result.push_back(indices[firstVertex]);
        written += 3;

        if (written == totalPoints) {
            break;
        }

        result.push_back(indices[firstVertex]);
        result.push_back(indices[nextVertex]);
        result.push_back(indices[prevVertex]);
        written += 3;

        lastVertex = nextVertex;
        firstVertex = prevVertex;

        ++nextVertex;
        --prevVertex;
    }
    return true;
}


bool TriangulateMesh(const MeshData& input, MeshData& output)
{
    output.indices.clear();
    output.counts.clear();

    size_t totalCount = 0;
    size_t totalTriangles = 0;
    for (int count : input.counts) {
        if (count >= 3) {
            totalCount += count;
            totalTriangles += count - 2;
        }
    }
    if (totalTriangles == 0) {
        return false;
    }

    output.indices.reserve(totalTriangles * 3);
    output.counts.resize(totalTriangles, 3);

    Triangulator ctx;
    size_t offset = 0;
    for (int count : input.counts) {
        std::span<const int> indices{ input.indices.data() + offset, static_cast<size_t>(count) };

        if (count == 3) {
            output.indices.push_back(indices[0]);
            output.indices.push_back(indices[1]);
            output.indices.push_back(indices[2]);
        }
        else if (count > 3 && (ctx.EarClipTriangulate(input.points, indices) || ctx.SimpleTriangulate(indices))) {
            for (int index : ctx.GetTriangleIndices()) {
                output.indices.push_back(indices[index]);
            }
        }

        offset += count;
    }

    return true;
}
#pragma endregion Triangulation
