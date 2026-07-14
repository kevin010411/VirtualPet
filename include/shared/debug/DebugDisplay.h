#ifndef DEBUG_DISPLAY_H
#define DEBUG_DISPLAY_H

#include "shared/config/AppProfile.h"

#if ENABLE_DEBUG
class DebugDisplay
{
public:
    virtual ~DebugDisplay() = default;

    virtual void showMessage(const char *title, const char *detail) = 0;
};
#endif

#endif // DEBUG_DISPLAY_H
