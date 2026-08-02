#pragma once

PXR_NAMESPACE_OPEN_SCOPE

class Unorm8
{
public:
    Unorm8() : value(0) {}
    Unorm8(float f) : value(static_cast<uint8_t>(std::clamp(f, 0.0f, 1.0f) * 255.0f)) {}
    operator float() const { return static_cast<float>(value) / 255.0f; }
    uint8_t value;
};

class Unorm16
{
public:
    Unorm16() : value(0) {}
    Unorm16(float f) : value(static_cast<uint16_t>(std::clamp(f, 0.0f, 1.0f) * 65535.0f)) {}
    operator float() const { return static_cast<float>(value) / 65535.0f; }
    uint16_t value;
};


template<class T>
class TVec2Unorm
{
public:
    TVec2Unorm() : values{ 0, 0 } {}
    TVec2Unorm(const GfVec2f& v) : values{ v[0], v[1] } {}
    operator GfVec2f() const { return GfVec2f((float)values[0], (float)values[1]); }
    T& operator[](size_t index) { return values[index]; }
    const T& operator[](size_t index) const { return values[index]; }
    T values[2];
};

template<class T>
class TVec3Unorm
{
public:
    TVec3Unorm() : values{ 0, 0, 0 } {}
    TVec3Unorm(const GfVec3f& v) : values{ v[0], v[1], v[2] } {}
    operator GfVec3f() const { return GfVec3f((float)values[0], (float)values[1], (float)values[2]); }
    T& operator[](size_t index) { return values[index]; }
    const T& operator[](size_t index) const { return values[index]; }
    T values[3];
};

template<class T>
class TVec4Unorm
{
public:
    TVec4Unorm() : values{ 0, 0, 0, 0 } {}
    TVec4Unorm(const GfVec4f& v) : values{ v[0], v[1], v[2], v[3] } {}
    operator GfVec4f() const { return GfVec4f((float)values[0], (float)values[1], (float)values[2], (float)values[3]); }
    T& operator[](size_t index) { return values[index]; }
    const T& operator[](size_t index) const { return values[index]; }
    T values[4];
};

using Vec2Unorm8 = TVec2Unorm<Unorm8>;
using Vec3Unorm8 = TVec3Unorm<Unorm8>;
using Vec4Unorm8 = TVec4Unorm<Unorm8>;
using Vec2Unorm16 = TVec2Unorm<Unorm16>;
using Vec3Unorm16 = TVec3Unorm<Unorm16>;
using Vec4Unorm16 = TVec4Unorm<Unorm16>;


PXR_NAMESPACE_CLOSE_SCOPE
