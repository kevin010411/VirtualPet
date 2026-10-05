#pragma once

#include "appearance/domain/RuntimeTableAppearance.h"
#include "shared/runtime_table/detail/RuntimeTableReader.h"

namespace RuntimeTableInternal
{
bool validateAppearanceSections(const RuntimeTable &table);
bool decodeInitialAppearance(const RuntimeTable &table, BundleReader &bundleReader,
                             AppearanceSelection &selection);
} // namespace RuntimeTableInternal
