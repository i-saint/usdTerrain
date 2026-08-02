#include "pch.h"
#include "fileFormat.h"
#include "unorm.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(
    UsdTerrainFileFormatTokens,
    USD_TERRAIN_FILE_FORMAT_TOKENS);

TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(UsdTerrainFileFormat, SdfFileFormat);
}


#pragma region Utils
template<class T>
inline std::string ToString(const T& value)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    }
    else {
        return std::to_string(value);
    }
}

template<class T>
inline T Parse(std::string_view str)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return std::string(str);
    }
    else if constexpr (std::is_arithmetic_v<T>) {
        T value;
        std::from_chars(str.data(), str.data() + str.size(), value);
        return value;
    }
    else {
        static_assert(std::is_same_v<T, void>, "Unsupported type");
    }
}

template<class T>
inline std::optional<T> Lookup(const VtDictionary& dict, const TfToken& key)
{
    if (auto it = dict.find(key); it != dict.end() && it->second.IsHolding<T>()) {
        return it->second.UncheckedGet<T>();
    }
    return std::nullopt;
}

template<class T>
inline std::optional<T> Lookup(const SdfFileFormat::FileFormatArguments& args, const TfToken& key)
{
    if (auto it = args.find(key); it != args.end()) {
        return Parse<T>(it->second);
    }
    return std::nullopt;
}

template<class FloatT>
inline float SampleHeight(std::span<const FloatT> image, int width, int height, float u, float v)
{
    float fx = u * (width - 1);
    float fy = v * (height - 1);

    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int x1 = std::min(x0 + 1, width - 1);
    int y1 = std::min(y0 + 1, height - 1);

    float tx = fx - x0;
    float ty = fy - y0;
    float h00 = image[y0 * width + x0];
    float h10 = image[y0 * width + x1];
    float h01 = image[y1 * width + x0];
    float h11 = image[y1 * width + x1];
    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;
    float h = h0 * (1.0f - ty) + h1 * ty;
    return h;
}
#pragma endregion Utils


#pragma region UsdTerrainGenerator

class UsdTerrainGenerator
{
public:
    struct Params
    {
#define Body(name, type, defaultValue) type name = defaultValue;
        USD_TERRAIN_PARAMS_EACH(Body);
#undef Body

        void Parse(const SdfFileFormat::FileFormatArguments& args);
    };

    struct MeshData
    {
        VtArray<int> counts;
        VtArray<int> indices;
        VtArray<GfVec3f> vertices;
        VtArray<GfVec3f> normals;
        VtArray<GfVec2f> uvs;

        bool generate(const HioImage::StorageSpec& image, const Params& params, int xdiv, int ydiv);
        template<class SamplerT>
        bool generate(SamplerT sampler, const Params& params, int xdiv, int ydiv);
    };

    UsdTerrainGenerator(SdfLayer* layer);
    bool generate();

private:
    SdfLayer* _layer = nullptr;
    Params _params;
    std::vector<MeshData> _meshes;
};


void UsdTerrainGenerator::Params::Parse(const SdfFileFormat::FileFormatArguments& args)
{
#define Body(name, type, defaultValue) if (auto value = Lookup<type>(args, UsdTerrainFileFormatTokens->name)) { name = *value; }
    USD_TERRAIN_PARAMS_EACH(Body);
#undef Body
    MaxLodLevel = std::max(1, MaxLodLevel);
    DefaultLodLevel = std::clamp(DefaultLodLevel, 0, MaxLodLevel - 1);
}


template<class FloatT>
struct Sampler
{
    Sampler(const void* image, int width, int height)
        : _image(static_cast<const FloatT*>(image), width * height), _width(width), _height(height) {}

    float operator()(float u, float v) const
    {
        return SampleHeight(_image, _width, _height, u, v);
    }

private:
    std::span<const FloatT> _image;
    int _width;
    int _height;
};

bool UsdTerrainGenerator::MeshData::generate(const HioImage::StorageSpec& image, const Params& params, int xdiv, int ydiv)
{
    if (image.format == HioFormatUNorm8 || image.format == HioFormatSNorm8) {
        auto sampler = Sampler<Unorm8>(image.data, image.width, image.height);
        return generate(sampler, params, xdiv, ydiv);
    }
    else if (image.format == HioFormatUInt16) {
        auto sampler = Sampler<Unorm16>(image.data, image.width, image.height);
        return generate(sampler, params, xdiv, ydiv);
    }
    else if (image.format == HioFormatFloat16) {
        auto sampler = Sampler<GfHalf>(image.data, image.width, image.height);
        return generate(sampler, params, xdiv, ydiv);
    }
    else if (image.format == HioFormatFloat32) {
        auto sampler = Sampler<float>(image.data, image.width, image.height);
        return generate(sampler, params, xdiv, ydiv);
    }
    else {
        TF_WARN("Unsupported image format: %s", TfEnum::GetName(image.format).c_str());
        return false;
    }
}

template<class SamplerT>
bool UsdTerrainGenerator::MeshData::generate(SamplerT sampler, const Params& params, int xdiv, int ydiv)
{
    const int numVertices = xdiv * ydiv;
    const int numQuads = (xdiv - 1) * (ydiv - 1);
    const int numIndices = numQuads * 6;
    vertices.resize(numVertices);
    normals.resize(numVertices);
    uvs.resize(numVertices);
    counts.resize(numQuads, 4);
    indices.resize(numIndices);

    float dx = params.XSize / (xdiv - 1);
    float dy = params.YSize / (ydiv - 1);
    float du = 1.0f / (xdiv - 1);
    float dv = 1.0f / (ydiv - 1);
    for (int j = 0; j < ydiv; ++j) {
        for (int i = 0; i < xdiv; ++i) {
            float x = dx * i;
            float y = dy * j;
            float u = du * i;
            float v = dv * j;
            float z = sampler(u, v) * params.MaxHeight;
            vertices[j * xdiv + i] = GfVec3f(x, y, z);
            uvs[j * xdiv + i] = GfVec2f(u, v);
        }
    }

    for (int j = 0; j < ydiv; ++j) {
        for (int i = 0; i < xdiv; ++i) {
            GfVec3f normal(0.0f, 0.0f, 0.0f);
            if (i > 0 && j > 0) {
                GfVec3f v0 = vertices[j * xdiv + i] - vertices[(j - 1) * xdiv + i - 1];
                GfVec3f v1 = vertices[(j - 1) * xdiv + i] - vertices[(j - 1) * xdiv + i - 1];
                normal += GfCross(v1, v0);
            }
            if (i < xdiv - 1 && j > 0) {
                GfVec3f v0 = vertices[j * xdiv + i] - vertices[(j - 1) * xdiv + i + 1];
                GfVec3f v1 = vertices[(j - 1) * xdiv + i] - vertices[(j - 1) * xdiv + i + 1];
                normal += GfCross(v0, v1);
            }
            if (i > 0 && j < ydiv - 1) {
                GfVec3f v0 = vertices[j * xdiv + i] - vertices[(j + 1) * xdiv + i - 1];
                GfVec3f v1 = vertices[(j + 1) * xdiv + i] - vertices[(j + 1) * xdiv + i - 1];
                normal += GfCross(v0, v1);
            }
            if (i < xdiv - 1 && j < ydiv - 1) {
                GfVec3f v0 = vertices[j * xdiv + i] - vertices[(j + 1) * xdiv + i + 1];
                GfVec3f v1 = vertices[(j + 1) * xdiv + i] - vertices[(j + 1) * xdiv + i + 1];
                normal += GfCross(v1, v0);
            }
            normals[j * xdiv + i] = normal.GetNormalized();
        }
    }

    for (int j = 0; j < ydiv - 1; ++j) {
        for (int i = 0; i < xdiv - 1; ++i) {
            auto dst = std::span<int>(indices).subspan((j * (xdiv - 1) + i) * 6, 6);
            dst[0] = j * xdiv + i;
            dst[1] = j * xdiv + (i + 1);
            dst[2] = (j + 1) * xdiv + i;
            dst[3] = (j + 1) * xdiv + i;
            dst[4] = j * xdiv + (i + 1);
            dst[5] = (j + 1) * xdiv + (i + 1);
        }
    }
    return true;
}

UsdTerrainGenerator::UsdTerrainGenerator(SdfLayer* layer)
    : _layer(layer)
{
    _params.Parse(layer->GetFileFormatArguments());
}

bool UsdTerrainGenerator::generate()
{
    auto image = HioImage::OpenForReading(_params.MapFile);
    if (!image) {
        TF_WARN("Failed to open image file: %s", _params.MapFile.c_str());
        return false;
    }

    HioImage::StorageSpec imageData;
    imageData.width = image->GetWidth();
    imageData.height = image->GetHeight();
    imageData.format = image->GetFormat();

    size_t dataSize = imageData.width * imageData.height * image->GetBytesPerPixel();
    std::shared_ptr<void> rawData = std::shared_ptr<void>(malloc(dataSize), free);
    imageData.data = rawData.get();

    if (!image->Read(imageData)) {
        TF_WARN("Failed to get image storage: %s", _params.MapFile.c_str());
        return false;
    }

    if (_params.XDiv < 0) {
        _params.XDiv = image->GetWidth();
    }
    if (_params.YDiv < 0) {
        _params.YDiv = image->GetHeight();
    }

    _meshes.resize(_params.MaxLodLevel);
    for (int lod = 0; lod < _params.MaxLodLevel; ++lod) {
        int xdiv = std::max(2, _params.XDiv >> lod);
        int ydiv = std::max(2, _params.YDiv >> lod);
        if (xdiv < 2 || ydiv < 2) {
            _meshes.resize(lod);
            break;
        }
        if (!_meshes[lod].generate(imageData, _params, xdiv, ydiv)) {
            TF_WARN("Failed to generate mesh for LOD %d", lod);
            return false;
        }
    }

    return true;
}

#pragma endregion UsdTerrainGenerator


#pragma region UsdTerrainFileFormat

UsdTerrainFileFormat::UsdTerrainFileFormat()
    : SdfFileFormat(
        UsdTerrainFileFormatTokens->Id,
        UsdTerrainFileFormatTokens->Version,
        UsdTerrainFileFormatTokens->Target,
        UsdTerrainFileFormatTokens->Extension)
{}

UsdTerrainFileFormat::~UsdTerrainFileFormat()
{}

bool UsdTerrainFileFormat::CanRead(const std::string& file) const
{
    return file.ends_with(UsdTerrainFileFormatTokens->Extension.GetString());
}

bool UsdTerrainFileFormat::Read(SdfLayer* layer, const std::string& resolvedPath, bool metadataOnly) const
{
    UsdTerrainGenerator generator(layer);
    return generator.generate();
}

bool UsdTerrainFileFormat::WriteToString(const SdfLayer& layer, std::string* str, const std::string& comment) const
{
    return false;
}

bool UsdTerrainFileFormat::WriteToStream(const SdfSpecHandle& spec, std::ostream& out, size_t indent) const
{
    return false;
}

void UsdTerrainFileFormat::ComposeFieldsForFileFormatArguments(
    const std::string& assetPath,
    const PcpDynamicFileFormatContext& context,
    FileFormatArguments* args,
    VtValue* contextDependencyData) const
{
    if (!args || !contextDependencyData || !contextDependencyData->IsHolding<VtDictionary>())
        return;

    auto& tokens = UsdTerrainFileFormatTokens;
    auto& dict = contextDependencyData->UncheckedGet<VtDictionary>();

#define LookupAndSetArg(name, type, _) if (auto value = Lookup<type>(dict, tokens->name)) { (*args)[tokens->name] = ToString(*value); }
    USD_TERRAIN_PARAMS_EACH(LookupAndSetArg)
#undef LookupAndSetArg
}

bool UsdTerrainFileFormat::CanFieldChangeAffectFileFormatArguments(
    const TfToken& field,
    const VtValue& oldValue,
    const VtValue& newValue,
    const VtValue& contextDependencyData) const
{
    return false;
}

#pragma endregion UsdTerrainFileFormat
PXR_NAMESPACE_CLOSE_SCOPE
