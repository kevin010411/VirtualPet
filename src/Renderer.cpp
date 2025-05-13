#include <vector>
#include <Adafruit_ST7735.h>
#include <SdFat.h>

class Renderer
{

    std::vector<String> animation_list_name;
    std::vector<String> now_animation_list;
    String now_animation_name;
    Adafruit_ST7735 *tft;
    SdFat *SD;

    unsigned short animation_index;
    int max_animation_index;

    String fileNameAsString(File activeFile)
    {
        char filename[20];
        String fn = "";

        activeFile.getName(filename, 20);

        for (unsigned short i = 0; i < strlen(filename); i++)
        {
            fn = fn + filename[i];
        }

        return fn;
    }

    void GetAnimationList(String now_animation_list_name)
    {
        String animation_name = "/animation/";
        animation_name += now_animation_list_name;
        File root = SD->open(animation_name);
        now_animation_list.clear();

        while (true)
        {
            File frameFile = root.openNextFile();
            if (!frameFile)
                break;
            String fileName = fileNameAsString(frameFile);
            now_animation_list.push_back(fileName);

            frameFile.close();
        }
    }

public:
    Renderer(Adafruit_ST7735 *ref_tft, SdFat *red_SD) : tft(ref_tft), SD(red_SD), animation_index(0), max_animation_index(0)
    {
    }

    void initAnimations()
    {
        tft->fillRect(0, 64, 128, 128, tft->color565(0, 0, 0));
        tft->setCursor(0, 64);
        tft->println("SD Init Success");
        Serial.println("SD Init Success");

        File root = SD->open("/animation");
        if (!root || !root.isDirectory())
        {
            tft->print("Failed to open root directory");
            Serial.println("Failed to open root directory");
            return;
        }

        while (true)
        {
            File folder = root.openNextFile();
            if (!folder || !folder.isDirectory())
                break;
            String fileName = fileNameAsString(folder);
            animation_list_name.push_back(fileName);
            folder.close();
        }
        root.close();
    }

    void ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0)
    {
        File bmpFile = SD->open(img_path);
        if (!bmpFile)
        {
            tft->println("Failed to open BMP file");
            tft->println(img_path);
            Serial.println("Failed to open BMP file");
            return;
        }

        // === BMP Header Parsing ===
        if (bmpFile.read() != 'B' || bmpFile.read() != 'M')
        {
            tft->println("Not a valid BMP file");
            Serial.println("Not a valid BMP file");
            bmpFile.close();
            return;
        }

        bmpFile.seek(10); // Offset to pixel data
        uint32_t pixelDataOffset = bmpFile.read() |
                                   (bmpFile.read() << 8) |
                                   (bmpFile.read() << 16) |
                                   (bmpFile.read() << 24);

        bmpFile.seek(18); // Width & Height
        int32_t bmpWidth = bmpFile.read() |
                           (bmpFile.read() << 8) |
                           (bmpFile.read() << 16) |
                           (bmpFile.read() << 24);
        int32_t bmpHeight = bmpFile.read() |
                            (bmpFile.read() << 8) |
                            (bmpFile.read() << 16) |
                            (bmpFile.read() << 24);

        bmpFile.seek(28); // Bits per pixel
        uint16_t bitsPerPixel = bmpFile.read() | (bmpFile.read() << 8);

        if (bitsPerPixel != 24)
        {
            tft->println("Only 24-bit BMP supported");
            Serial.println("Only 24-bit BMP supported");
            bmpFile.close();
            return;
        }

        bmpFile.seek(pixelDataOffset);

        // BMP rows are padded to multiples of 4 bytes
        int rowSize = ((bmpWidth * 3 + 3) / 4) * 4;

        // Buffer for one row of RGB565 pixels
        uint8_t rowBuffer[rowSize];
        uint16_t lineBuffer[bmpWidth];

        // === Draw BMP (bottom to top) ===
        for (int y = 0; y < bmpHeight; y++)
        {
            bmpFile.read(rowBuffer, rowSize);

            for (int x = 0; x < bmpWidth; x++)
            {
                int i = x * 3;
                uint8_t b = rowBuffer[i];
                uint8_t g = rowBuffer[i + 1];
                uint8_t r = rowBuffer[i + 2];

                lineBuffer[x] = ((r & 0xF8) << 8) |
                                ((g & 0xFC) << 3) |
                                (b >> 3);
            }

            // Draw this line in one shot
            tft->drawRGBBitmap(xmin, ymin + y, lineBuffer, bmpWidth, 1);
        }

        bmpFile.close();
        Serial.println("BMP render done.");
    }

    void DisplayAnimation()
    {
        if (now_animation_list.size() <= animation_index)
        {
            animation_index = 0;
        }
        String animation_name = "/animation/";
        animation_name += now_animation_name;
        animation_name += "/";
        animation_name += now_animation_list[animation_index++];
        ShowSDCardImage(animation_name.c_str(), 0, 32);
    }

    void DisplayAnimation(String animationName)
    {
        int index = -1;
        for (size_t i = 0; i < animation_list_name.size(); ++i)
        {
            if (animation_list_name[i] == animationName)
            {
                index = i;
                break;
            }
        }

        if (index == -1)
        {
            Serial.print("找不到指定的animationName:");
            Serial.println(animationName);
            ShowAnimationList();
        }
        else
        {
            if (animation_list_name[index] != now_animation_name)
            {
                now_animation_name = animation_list_name[index];
                GetAnimationList(now_animation_name);
            }
            return DisplayAnimation();
        }
    }

    void ShowAnimationList()
    {
        tft->setCursor(0, 64);
        for (unsigned short pos = 0; pos < animation_list_name.size(); ++pos)
        {
            tft->print(animation_list_name[pos]);
            Serial.println(animation_list_name[pos]);
        }
    }
};