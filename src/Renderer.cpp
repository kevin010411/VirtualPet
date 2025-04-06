#include <vector>
#include <SD.h>
#include <Adafruit_ST7735.h>

class Renderer
{

    std::vector<String> animation_list_name;
    std::vector<String> now_animation_list;
    String now_animation_name;

    // SdFat *SD;
    // Adafruit_ImageReader reader = Adafruit_ImageReader(SD);
    Adafruit_ST7735 *tft;
    unsigned short animation_index;
    int max_animation_index;

    String fileNameAsString(SDLib::File activeFile)
    {
        return String(activeFile.name());
    }

    void GetAnimationList(String now_animation_list_name)
    {
        String animation_name = "/animation/";
        animation_name += now_animation_list_name;
        SDLib::File root = SD.open(animation_name);
        now_animation_list.clear();

        while (true)
        {
            SDLib::File frameFile = root.openNextFile();
            if (!frameFile)
                break;
            String fileName = fileNameAsString(frameFile);
            now_animation_list.push_back(fileName);

            frameFile.close();
        }
    }

public:
    Renderer(Adafruit_ST7735 *ref_tft) : tft(ref_tft), animation_index(0), max_animation_index(0)
    {
    }

    void initAnimations()
    {
        // tft->setCursor(0, 0);
        if (!SD.begin())
        { // ESP32 requires 25 MHz limit
            // tft->print("SD initialization failed");
            Serial.println("SD initialization failed");
            return;
        }
        tft->print("SD Init Success");
        Serial.println("SD Init Success");

        SDLib::File root = SD.open("/animation");
        if (!root)
        {
            tft->print("Failed to open root directory");
            Serial.println("Failed to open root directory");
            return;
        }

        while (true)
        {
            SDLib::File folder = root.openNextFile();
            if (!folder || !folder.isDirectory())
                break;
            String fileName = fileNameAsString(folder);
            animation_list_name.push_back(fileName);
        }
        root.close();
    }

    void ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0)
    {
        SDLib::File bmpFile = SD.open(img_path);
        if (!bmpFile)
        {
            Serial.println("Failed to open BMP file");
            return;
        }

        // === BMP Header Parsing ===
        if (bmpFile.read() != 'B' || bmpFile.read() != 'M')
        {
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
            Serial.println("Only 24-bit BMP supported");
            bmpFile.close();
            return;
        }

        bmpFile.seek(pixelDataOffset);

        // BMP rows are padded to multiples of 4 bytes
        int rowSize = ((bmpWidth * 3 + 3) / 4) * 4;

        // Temporary storage
        uint8_t r, g, b;
        uint16_t color;

        // === Draw BMP (bottom to top) ===
        for (int y = 0; y < bmpHeight; y++)
        {
            int pos = pixelDataOffset + (bmpHeight - 1 - y) * rowSize;
            bmpFile.seek(pos);
            for (int x = 0; x < bmpWidth; x++)
            {
                b = bmpFile.read();
                g = bmpFile.read();
                r = bmpFile.read();
                color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                tft->drawPixel(xmin + x, ymin + y, color);
            }
        }

        bmpFile.close();
        Serial.println("BMP render done.");
        // reader.drawBMP(img_path, *tft, xmin, ymin);
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
        for (unsigned short pos = 0; pos < animation_list_name.size(); ++pos)
        {
            Serial.println(animation_list_name[pos]);
        }
    }
};