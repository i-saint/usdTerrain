#include "pch.h"
#include "meshUtils.h"

#include <algorithm>
#include <unordered_set>

#pragma region Triangulation
static float Cross2D(const GfVec2f& a, const GfVec2f& b, const GfVec2f& c)
{
    const GfVec2f ab = b - a;
    const GfVec2f ac = c - a;
    return ab[0] * ac[1] - ab[1] * ac[0];
}

static float SignedArea2D(std::span<const GfVec2f> points)
{
    float area = 0.0f;
    const size_t n = points.size();
    for (size_t i = 0; i < n; ++i) {
        const GfVec2f& a = points[i];
        const GfVec2f& b = points[(i + 1) % n];
        area += a[0] * b[1] - b[0] * a[1];
    }
    return area * 0.5f;
}

static bool PointInTriangle2D(const GfVec2f& p, const GfVec2f& a, const GfVec2f& b, const GfVec2f& c)
{
    constexpr float kEps = 1e-7f;
    const float c0 = Cross2D(a, b, p);
    const float c1 = Cross2D(b, c, p);
    const float c2 = Cross2D(c, a, p);
    const bool hasNeg = (c0 < -kEps) || (c1 < -kEps) || (c2 < -kEps);
    const bool hasPos = (c0 > kEps) || (c1 > kEps) || (c2 > kEps);
    return !(hasNeg && hasPos);
}

static GfVec3f ComputePolygonNormal(std::span<const GfVec3f> points, std::span<const int> indices)
{
    GfVec3f normal(0.0f, 0.0f, 0.0f);
    const int count = static_cast<int>(indices.size());
    for (int i = 0; i < count; ++i) {
        const int i0 = indices[i];
        const int i1 = indices[(i + 1) % count];
        const GfVec3f& p0 = points[i0];
        const GfVec3f& p1 = points[i1];
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

static ProjectionAxis SelectProjectionAxis(const GfVec3f& normal)
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

static GfVec2f ProjectToPolygon2D(const GfVec3f& p, ProjectionAxis axis)
{
    switch (axis) {
    case ProjectionAxis::kZPositive: return GfVec2f(p[0], p[1]);
    case ProjectionAxis::kZNegative: return GfVec2f(p[0], -p[1]);
    case ProjectionAxis::kYPositive: return GfVec2f(-p[0], p[2]);
    case ProjectionAxis::kYNegative: return GfVec2f(p[0], p[2]);
    case ProjectionAxis::kXPositive: return GfVec2f(p[1], p[2]);
    case ProjectionAxis::kXNegative: return GfVec2f(-p[1], p[2]);
    default: return GfVec2f(0.0f, 0.0f);
    }
}


// std::vector や std::unordered_set を使い回すため、context を用意
struct TriangulationContext
{
    std::vector<int> polygon;
    std::vector<int> triangle2DVertexIndices;
    std::vector<GfVec2f> polygon2DVertices;
    std::unordered_set<int> triangulatedVertexSet;

    bool EarClipTriangulate();
    bool ValidateTriangulation(std::span<const int> indices);
    bool TriangulateFace(const MeshData& input, std::span<const int> indices);
};

bool TriangulationContext::EarClipTriangulate()
{
    triangle2DVertexIndices.clear();
    const int n = static_cast<int>(polygon2DVertices.size());
    if (n < 3) {
        return false;
    }
    if (n == 3) {
        triangle2DVertexIndices.push_back(0);
        triangle2DVertexIndices.push_back(1);
        triangle2DVertexIndices.push_back(2);
        return true;
    }

    polygon.resize(n);
    for (int i = 0; i < n; ++i) {
        polygon[i] = i;
    }

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

            const GfVec2f& a = polygon2DVertices[i0];
            const GfVec2f& b = polygon2DVertices[i1];
            const GfVec2f& c = polygon2DVertices[i2];
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

            triangle2DVertexIndices.push_back(i0);
            triangle2DVertexIndices.push_back(i1);
            triangle2DVertexIndices.push_back(i2);
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

    triangle2DVertexIndices.push_back(polygon[0]);
    triangle2DVertexIndices.push_back(polygon[1]);
    triangle2DVertexIndices.push_back(polygon[2]);
    return true;
}

bool TriangulationContext::ValidateTriangulation(std::span<const int> indices)
{
    triangulatedVertexSet.clear();
    triangulatedVertexSet.reserve(triangle2DVertexIndices.size());

    for (int localIndex : triangle2DVertexIndices) {
        if (localIndex < 0 || localIndex >= static_cast<int>(indices.size())) {
            return false;
        }
        triangulatedVertexSet.insert(indices[localIndex]);
    }

    for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
        if (triangulatedVertexSet.find(indices[i]) == triangulatedVertexSet.end()) {
            return false;
        }
    }

    return true;
}

bool TriangulationContext::TriangulateFace(const MeshData& input, std::span<const int> indices)
{
    triangle2DVertexIndices.clear();
    if (indices.size() < 3) {
        return false;
    }

    for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
        const int index = indices[i];
        if (index < 0 || static_cast<size_t>(index) >= input.points.size()) {
            return false;
        }
    }

    const GfVec3f normal = ComputePolygonNormal(input.points, indices);
    const ProjectionAxis axis = SelectProjectionAxis(normal);

    polygon2DVertices.clear();
    polygon2DVertices.reserve(indices.size());
    for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
        polygon2DVertices.push_back(ProjectToPolygon2D(input.points[indices[i]], axis));
    }

    if (!EarClipTriangulate()) {
        return false;
    }

    return ValidateTriangulation(indices);
}

static void SimpleTriangulation(VtArray<int>& dst, std::span<const int> indices)
{
    if (indices.size() == 3) {
        dst.push_back(indices[0]);
        dst.push_back(indices[1]);
        dst.push_back(indices[2]);
        return;
    }

    int nextVertex = 3;
    int prevVertex = static_cast<int>(indices.size()) - 1;
    int lastVertex = 2;
    int firstVertex = 0;

    const int triangleCount = static_cast<int>(indices.size()) - 2;
    const int totalPoints = triangleCount * 3;
    int written = 0;

    dst.push_back(indices[0]);
    dst.push_back(indices[1]);
    dst.push_back(indices[2]);
    written += 3;

    while (nextVertex <= prevVertex) {
        dst.push_back(indices[lastVertex]);
        dst.push_back(indices[nextVertex]);
        dst.push_back(indices[firstVertex]);
        written += 3;

        if (written == totalPoints) {
            break;
        }

        dst.push_back(indices[firstVertex]);
        dst.push_back(indices[nextVertex]);
        dst.push_back(indices[prevVertex]);
        written += 3;

        lastVertex = nextVertex;
        firstVertex = prevVertex;

        ++nextVertex;
        --prevVertex;
    }
}


bool TriangulateMesh(const MeshData& input, MeshData& output)
{
    output.indices.clear();
    output.counts.clear();

    size_t requiredInputIndices = 0;
    size_t totalTriangles = 0;
    for (int faceSize : input.counts) {
        if (faceSize >= 3) {
            requiredInputIndices += static_cast<size_t>(faceSize);
            totalTriangles += static_cast<size_t>(faceSize - 2);
        }
    }

    if (requiredInputIndices != input.indices.size()) {
        return false;
    }

    output.indices.reserve(totalTriangles * 3);
    output.counts.resize(totalTriangles, 3);

    TriangulationContext ctx;
    size_t inputOffset = 0;
    for (int count : input.counts) {
        std::span<const int> indices{ input.indices.data() + inputOffset, static_cast<size_t>(count) };

        if (count < 3) {
            continue;
        }
        else if (count == 3) {
            output.indices.push_back(indices[0]);
            output.indices.push_back(indices[1]);
            output.indices.push_back(indices[2]);
        }
        else if (ctx.TriangulateFace(input, indices)) {
            for (int localIndex : ctx.triangle2DVertexIndices) {
                output.indices.push_back(indices[localIndex]);
            }
        }
        else {
            SimpleTriangulation(output.indices, indices);
        }

        inputOffset += static_cast<size_t>(count);
    }

    return true;
}
#pragma endregion Triangulation
