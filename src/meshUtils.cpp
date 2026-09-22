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

static float SignedArea2D(const std::vector<GfVec2f>& points)
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

static bool EarClipTriangulate(const std::vector<GfVec2f>& points, std::vector<int>& triangleVertexIndices)
{
    triangleVertexIndices.clear();
    const int n = static_cast<int>(points.size());
    if (n < 3) {
        return false;
    }
    if (n == 3) {
        triangleVertexIndices.push_back(0);
        triangleVertexIndices.push_back(1);
        triangleVertexIndices.push_back(2);
        return true;
    }

    std::vector<int> polygon;
    polygon.resize(n);
    for (int i = 0; i < n; ++i) {
        polygon[i] = i;
    }

    if (SignedArea2D(points) < 0.0f) {
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

            const GfVec2f& a = points[i0];
            const GfVec2f& b = points[i1];
            const GfVec2f& c = points[i2];
            if (Cross2D(a, b, c) <= kEps) {
                continue;
            }

            bool containsOther = false;
            for (int j = 0; j < m; ++j) {
                const int ip = polygon[j];
                if (ip == i0 || ip == i1 || ip == i2) {
                    continue;
                }
                if (PointInTriangle2D(points[ip], a, b, c)) {
                    containsOther = true;
                    break;
                }
            }

            if (containsOther) {
                continue;
            }

            triangleVertexIndices.push_back(i0);
            triangleVertexIndices.push_back(i1);
            triangleVertexIndices.push_back(i2);
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

    triangleVertexIndices.push_back(polygon[0]);
    triangleVertexIndices.push_back(polygon[1]);
    triangleVertexIndices.push_back(polygon[2]);
    return true;
}

static GfVec3f ComputePolygonNormal(const MeshData& input, const int* polygonIndices, int polygonSize)
{
    GfVec3f normal(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < polygonSize; ++i) {
        const int i0 = polygonIndices[i];
        const int i1 = polygonIndices[(i + 1) % polygonSize];
        const GfVec3f& p0 = input.points[i0];
        const GfVec3f& p1 = input.points[i1];
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

static bool ValidateTriangulation(const int* polygonIndices, int polygonSize, const std::vector<int>& triangle2DVertexIndices)
{
    std::unordered_set<int> results;
    results.reserve(triangle2DVertexIndices.size());

    for (int localIndex : triangle2DVertexIndices) {
        if (localIndex < 0 || localIndex >= polygonSize) {
            return false;
        }
        results.insert(polygonIndices[localIndex]);
    }

    for (int i = 0; i < polygonSize; ++i) {
        if (results.find(polygonIndices[i]) == results.end()) {
            return false;
        }
    }

    return true;
}

static bool Tess2dConstrainedTriangulateFace(const MeshData& input, const int* polygonIndices, int polygonSize, std::vector<int>& triangle2DVertexIndices)
{
    triangle2DVertexIndices.clear();
    if (polygonSize < 3) {
        return false;
    }

    for (int i = 0; i < polygonSize; ++i) {
        const int index = polygonIndices[i];
        if (index < 0 || static_cast<size_t>(index) >= input.points.size()) {
            return false;
        }
    }

    const GfVec3f normal = ComputePolygonNormal(input, polygonIndices, polygonSize);
    const ProjectionAxis axis = SelectProjectionAxis(normal);

    std::vector<GfVec2f> polygon2DVertices;
    polygon2DVertices.reserve(polygonSize);
    for (int i = 0; i < polygonSize; ++i) {
        polygon2DVertices.push_back(ProjectToPolygon2D(input.points[polygonIndices[i]], axis));
    }

    if (!EarClipTriangulate(polygon2DVertices, triangle2DVertexIndices)) {
        return false;
    }

    return ValidateTriangulation(polygonIndices, polygonSize, triangle2DVertexIndices);
}

static void SimpleTriangulation(VtArray<int>& dst, const int* polygonIndices, int polygonSize)
{
    if (polygonSize == 3) {
        dst.push_back(polygonIndices[0]);
        dst.push_back(polygonIndices[1]);
        dst.push_back(polygonIndices[2]);
        return;
    }

    int nextVertex = 3;
    int prevVertex = polygonSize - 1;
    int lastVertex = 2;
    int firstVertex = 0;

    const int triangleCount = polygonSize - 2;
    const int totalPoints = triangleCount * 3;
    int written = 0;

    dst.push_back(polygonIndices[0]);
    dst.push_back(polygonIndices[1]);
    dst.push_back(polygonIndices[2]);
    written += 3;

    while (nextVertex <= prevVertex) {
        dst.push_back(polygonIndices[lastVertex]);
        dst.push_back(polygonIndices[nextVertex]);
        dst.push_back(polygonIndices[firstVertex]);
        written += 3;

        if (written == totalPoints) {
            break;
        }

        dst.push_back(polygonIndices[firstVertex]);
        dst.push_back(polygonIndices[nextVertex]);
        dst.push_back(polygonIndices[prevVertex]);
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
        if (faceSize < 3) {
            return false;
        }
        requiredInputIndices += static_cast<size_t>(faceSize);
        totalTriangles += static_cast<size_t>(faceSize - 2);
    }

    if (requiredInputIndices != input.indices.size()) {
        return false;
    }

    output.indices.reserve(totalTriangles * 3);
    output.counts.resize(totalTriangles, 3);

    size_t inputOffset = 0;
    for (int faceSize : input.counts) {
        const int* polygonIndices = input.indices.data() + inputOffset;

        if (faceSize == 3) {
            output.indices.push_back(polygonIndices[0]);
            output.indices.push_back(polygonIndices[1]);
            output.indices.push_back(polygonIndices[2]);
        }
        else {
            std::vector<int> triangle2DVertexIndices;
            const bool status = Tess2dConstrainedTriangulateFace(input, polygonIndices, faceSize, triangle2DVertexIndices);
            if (status) {
                for (int localIndex : triangle2DVertexIndices) {
                    output.indices.push_back(polygonIndices[localIndex]);
                }
            }
            else {
                SimpleTriangulation(output.indices, polygonIndices, faceSize);
            }
        }

        inputOffset += static_cast<size_t>(faceSize);
    }

    return true;
}
#pragma endregion Triangulation
