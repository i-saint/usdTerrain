#include "pch.h"
#include "fileFormat.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(
    UsdTerrainFileFormatTokens,
    USD_DANCING_CUBES_EXAMPLE_FILE_FORMAT_TOKENS);

TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(UsdTerrainFileFormat, SdfFileFormat);
}


UsdTerrainFileFormat::UsdTerrainFileFormat()
    : SdfFileFormat(
        UsdTerrainFileFormatTokens->Id,
        UsdTerrainFileFormatTokens->Version,
        UsdTerrainFileFormatTokens->Target,
        UsdTerrainFileFormatTokens->Extension)
{
}

UsdTerrainFileFormat::~UsdTerrainFileFormat()
{
}

bool UsdTerrainFileFormat::CanRead(const std::string& file) const
{
    return false;
}

bool UsdTerrainFileFormat::Read(SdfLayer* layer, const std::string& resolvedPath, bool metadataOnly) const
{
    return false;
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
}

bool UsdTerrainFileFormat::CanFieldChangeAffectFileFormatArguments(
    const TfToken& field,
    const VtValue& oldValue,
    const VtValue& newValue,
    const VtValue& contextDependencyData) const
{
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE
