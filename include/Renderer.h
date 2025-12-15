
#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <Adafruit_ST7735.h>
#include <SdFat.h>

class Renderer
{
private:
    std::vector<String> animation_list_name;
    std::vector<String> now_animation_list;
    String now_animation_name;

    Adafruit_ST7735 *tft;
    SdFat *SD;

    unsigned short animation_index;
    int max_animation_index;
    bool showOneTime;

    String fileNameAsString(File &file);
    void InitAnimationList(String animation_name);

public:
    Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);

    void initAnimations();
    // void ShowSDCardImage(String path, int xpos, int ypos, int opacity);
    void ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0, int batch_lines = 4);
    bool DisplayAnimation();
    bool DisplayAnimation(String animationName, bool showOnce, String animateName = "");
    void ShowAnimationList();
};

#endif
