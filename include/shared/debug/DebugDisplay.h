#ifndef DEBUG_DISPLAY_H
#define DEBUG_DISPLAY_H

class DebugDisplay
{
public:
    virtual ~DebugDisplay() = default;

    virtual void showMessage(const char *title, const char *detail) = 0;
};

#endif // DEBUG_DISPLAY_H
