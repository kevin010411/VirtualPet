#include <assert.h>
#include <string.h>

#include "commands/domain/StatusSetContract.h"

namespace
{
bool readValue(const char *source, const void *, int32_t &value)
{
    if (strcmp(source, "custom0") != 0)
        return false;
    value = 100;
    return true;
}

void testRuntimeContractStatusResolution()
{
    StatusSetConfig set = {};
    strcpy(set.animation, "StatusCustom0");
    strcpy(set.conditions[0].source, "custom0");
    set.conditions[0].levels = 2;
    set.conditions[0].minValue = 0;
    set.conditions[0].maxValue = 100;
    set.conditionCount = 1;

    StatusSetResolution resolution = {};
    assert(resolveStatusSet(set, readValue, nullptr, resolution));
    assert(resolution.frame == 2);
    assert(resolution.requiredFrames == 2);
}
} // namespace

int main()
{
    testRuntimeContractStatusResolution();
    return 0;
}
