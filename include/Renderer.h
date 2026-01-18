
#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <unordered_set>
#include <Adafruit_ST7735.h>
#include <SdFat.h>

struct AnimInfo
{
    String baseDir;    // 例如 "/animation/idle/breathe"
    uint16_t maxFrame; // 最大幀編號（用檔名數字算）
};

// Arduino String 當 unordered_map key 需要 hash/eq
struct StringHash
{
    size_t operator()(const String &s) const noexcept
    {
        // 簡單 FNV-1a
        const char *p = s.c_str();
        size_t h = 1469598103934665603ull;
        while (*p)
        {
            h ^= (unsigned char)(*p++);
            h *= 1099511628211ull;
        }
        return h;
    }
};
struct StringEq
{
    bool operator()(const String &a, const String &b) const noexcept
    {
        return a == b;
    }
};

class Renderer
{
private:
    std::unordered_map<String, AnimInfo, StringHash, StringEq> animCache;
    int frame_count = 0;

    Adafruit_ST7735 *tft;
    SdFat *SD;

    String now_anim_key;
    unsigned short animation_index;
    AnimInfo *now_anim_info = nullptr;
    bool showOneTime;

public:
    Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);

    void initAnimations();
    void scanAnimationTree(const char *dirPath);
    // void ShowSDCardImage(String path, int xpos, int ypos, int opacity);
    void ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0, int batch_lines = 4);
    bool DisplayAnimation();
    bool DisplayAnimation(const String &key, bool showOnce);
};

#endif
