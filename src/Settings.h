#pragma once
#include <vector>
#include <string>

struct Settings {
    bool  ShowWindow   = true;
    float Opacity      = 1.0f;
    std::string ApiKey;
    std::vector<int> TrackedAchievements;

    void Load();
    void Save();
};

extern Settings g_Settings;
