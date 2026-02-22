#pragma once
#include <vector>
#include <string>
#include <unordered_set>

struct Settings {
    bool  ShowWindow   = true;
    float Opacity      = 1.0f;
    std::string ApiKey;
    std::vector<int> TrackedAchievements;
    std::unordered_set<int> CollapsedHeaders; // achievement IDs whose top header is collapsed
    std::unordered_set<int> CollapsedDetails; // achievement IDs whose Details section is collapsed

    void Load();
    void Save();
};

extern Settings g_Settings;
