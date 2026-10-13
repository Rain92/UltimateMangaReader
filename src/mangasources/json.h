#pragma once

#include <stdexcept>

// rapidjson for the sources: a failed assertion (a missing member, a value of another type, ...) throws a
// JsonError instead of aborting the app. Only include it from .cpp files, before anything else that includes
// rapidjson: the assertion has to be defined first.

struct JsonError : std::runtime_error
{
    JsonError() : std::runtime_error("unexpected json") {}
};

#ifdef RAPIDJSON_RAPIDJSON_H_
#error "json.h has to be included before rapidjson"
#endif

#define RAPIDJSON_ASSERT(x) ((x) ? (void)0 : throw JsonError())

#include "thirdparty/rapidjson.h"
