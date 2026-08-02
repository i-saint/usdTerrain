#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <functional>
#include <type_traits>
#include <charconv>

#pragma warning(push, 0)
#pragma warning(disable: 4244 4273 4305 4996)
#include <pxr/pxr.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/gf/half.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/pcp/dynamicFileFormatInterface.h>
#include <pxr/imaging/hio/image.h>
#pragma warning(pop)