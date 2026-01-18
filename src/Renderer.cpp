#include "Renderer.h"
#include <Arduino.h>

Renderer::Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD) : tft(ref_tft), SD(ref_SD), animation_index(0)
{
}

void Renderer::scanAnimationTree(const char *dirPath)
{
    File dir = SD->open(dirPath);
    if (!dir || !dir.isDirectory())
        return;

    uint16_t fileCount = 0;

    while (true)
    {
        File entry = dir.openNextFile();
        if (!entry)
            break;

        char name[32];
        entry.getName(name, sizeof(name));

        if (entry.isDirectory())
        {
            char child[128];
            snprintf(child, sizeof(child), "%s/%s", dirPath, name);
            entry.close();
            scanAnimationTree(child);
        }
        else
        {
            fileCount++;
            entry.close();
        }
    }
    dir.close();

    if (fileCount > 0)
    {
        const char *prefix = "/animation/";
        String base = String(dirPath);

        String key = base.startsWith(prefix)
                         ? base.substring(strlen(prefix)) // e.g. "idle/breathe"
                         : base;

        AnimInfo info;
        info.baseDir = base;
        info.maxFrame = fileCount;

        animCache[key] = info;
    }
}

void Renderer::initAnimations()
{
    animCache.clear();
    scanAnimationTree("/animation");
}

void Renderer::ShowSDCardImage(const char *img_path, int xmin, int ymin, int batch_lines)
{
    File bmpFile = SD->open(img_path);
    if (!bmpFile)
    {
        tft->println("Failed to open BMP file");
        // Serial.println("Failed to open BMP file");
        return;
    }

    if (bmpFile.read() != 'B' || bmpFile.read() != 'M')
    {
        tft->println("Not a valid BMP file");
        bmpFile.close();
        return;
    }

    bmpFile.seek(10);
    uint32_t pixelDataOffset = bmpFile.read() |
                               (bmpFile.read() << 8) |
                               (bmpFile.read() << 16) |
                               (bmpFile.read() << 24);

    bmpFile.seek(18);
    int32_t bmpWidth = bmpFile.read() |
                       (bmpFile.read() << 8) |
                       (bmpFile.read() << 16) |
                       (bmpFile.read() << 24);
    int32_t bmpHeight = bmpFile.read() |
                        (bmpFile.read() << 8) |
                        (bmpFile.read() << 16) |
                        (bmpFile.read() << 24);

    bmpFile.seek(28);
    uint16_t bitsPerPixel = bmpFile.read() | (bmpFile.read() << 8);
    if (bitsPerPixel != 24)
    {
        tft->println("Only 24-bit BMP supported");
        bmpFile.close();
        return;
    }

    bool flipX = true;
    bool flipY = false;

    bmpFile.seek(pixelDataOffset);

    int rowSize = ((bmpWidth * 3 + 3) / 4) * 4;
    uint8_t rowBuffer[rowSize * batch_lines];
    uint16_t lineBuffer[bmpWidth * batch_lines];

    for (int y = 0; y < bmpHeight; y += batch_lines)
    {
        int actualLines = (y + batch_lines > bmpHeight) ? (bmpHeight - y) : batch_lines;

        for (int i = 0; i < actualLines; i++)
        {
            bmpFile.read(&rowBuffer[i * rowSize], rowSize);
            for (int x = 0; x < bmpWidth; x++)
            {
                int j = x * 3;
                uint8_t b = rowBuffer[i * rowSize + j];
                uint8_t g = rowBuffer[i * rowSize + j + 1];
                uint8_t r = rowBuffer[i * rowSize + j + 2];
                if (flipX)
                {
                    int flipped_x = bmpWidth - 1 - x;
                    lineBuffer[i * bmpWidth + flipped_x] = ((r & 0xF8) << 8) |
                                                           ((g & 0xFC) << 3) |
                                                           (b >> 3);
                }
                else
                    lineBuffer[i * bmpWidth + x] =
                        ((r & 0xF8) << 8) |
                        ((g & 0xFC) << 3) |
                        (b >> 3);
            }
        }

        for (int i = 0; i < actualLines; i++)
        {
            int srcRow = y + i;
            int drawY;
            // 反轉上下順序
            if (flipY)
                drawY = ymin + (bmpHeight - 1 - srcRow);
            else
                drawY = ymin + srcRow;
            tft->drawRGBBitmap(xmin, drawY,
                               &lineBuffer[i * bmpWidth],
                               bmpWidth, 1);
        }
    }

    bmpFile.close();
    // Serial.println("BMP render done (optimized).\n");
}

bool Renderer::DisplayAnimation()
{
    if (animation_index > now_anim_info->maxFrame)
    {
        animation_index = 1;
        if (showOneTime)
            return true;
    }

    char path[128];
    snprintf(path, sizeof(path), "/animation/%s/%u.bmp",
             now_anim_key.c_str(),
             (unsigned)animation_index);

    ShowSDCardImage(path, 0, 32);
    animation_index++;
    return false;
}

bool Renderer::DisplayAnimation(const String &key, bool showOnce)
{
    auto it = animCache.find(key);
    if (it == animCache.end())
    {
        tft->print("animCache 找不到: ");
        tft->println(key);
        return true;
    }

    if (key != now_anim_key)
    {
        now_anim_key = key;
        now_anim_info = &(it->second);
        animation_index = 1; //
        showOneTime = showOnce;
    }

    return DisplayAnimation();
}