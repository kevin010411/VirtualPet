#include <vector>
#include <SdFat.h>
#include <Adafruit_ImageReader.h> // Image-reading functions
#include <Adafruit_ST7735.h>

class Renderer
{
    std::vector<String> animation_list_name;
    std::vector<String> now_animation_list;
    String now_animation_name;

    SdFat *SD;
    Adafruit_ImageReader reader;
    Adafruit_ST7735 *tft;
    unsigned short animation_index;
    int max_animation_index;
    void initAnimations()
    {

        if (!(*SD).begin(10, SD_SCK_MHZ(25)))
        { // ESP32 requires 25 MHz limit
            tft->print("SD initialization failed");
            Serial.println("SD initialization failed");
            return;
        }
        Serial.println("SD Init Success");

        File root = SD->open("/animation");
        if (!root)
        {
            Serial.println("Failed to open root directory.");
            return;
        }

        while (true)
        {
            File folder = root.openNextFile();
            if (!folder || !folder.isDirectory())
                break;
            String fileName = fileNameAsString(folder);
            animation_list_name.push_back(fileName);
        }
        root.close();
    }

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
    Renderer(SdFat *sd, Adafruit_ST7735 *ref_tft) : SD(sd), reader(Adafruit_ImageReader(*sd)),
                                                    tft(ref_tft), animation_index(0), max_animation_index(0)
    {
        initAnimations();
    }

    void ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0)
    {
        reader.drawBMP(img_path, *tft, xmin, ymin);
        // reader.printStatus(stat);
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