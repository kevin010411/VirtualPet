#pragma once

#include "appearance/RuntimeTableAppearance.h"
#include "resources/detail/RuntimeTableReader.h"

namespace RuntimeTableInternal
{
bool validateAppearanceSections(const RuntimeTable &table);
bool decodeInitialAppearance(const RuntimeTable &table, BundleReader &bundleReader,
                             AppearanceSelection &selection);
} // namespace RuntimeTableInternal
