#include "pch.h"
#include "fileFormat.h"


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
inline T Parse(const std::string& str)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return str;
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
#pragma endregion Utils


#pragma region UsdTerrainGenerator

struct UsdTerrainParams
{
#define Body(name, type, defaultValue) type name = defaultValue;
    USD_TERRAIN_PARAMS_EACH(Body);
#undef Body

    void Parse(const SdfFileFormat::FileFormatArguments& args)
    {
#define Body(name, type, defaultValue) if (auto value = Lookup<type>(args, UsdTerrainFileFormatTokens->name)) { name = *value; }
        USD_TERRAIN_PARAMS_EACH(Body);
#undef Body
    }
};

class UsdTerrainGenerator
{
public:
    UsdTerrainGenerator(SdfLayer* layer);
    bool generate();

private:
    SdfLayer* _layer = nullptr;
    UsdTerrainParams _params;
};

UsdTerrainGenerator::UsdTerrainGenerator(SdfLayer* layer)
    : _layer(layer)
{
    _params.Parse(layer->GetFileFormatArguments());
}

bool UsdTerrainGenerator::generate()
{
    if (_params.MapFile.empty())
    {
        TF_WARN("MapFile parameter is empty.");
        return false;
    }

    // todo

    return false;
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
