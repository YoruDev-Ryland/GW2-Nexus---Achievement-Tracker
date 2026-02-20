#include "Settings.h"
#include "Shared.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Settings g_Settings;

static std::string SettingsPath()
{
    return std::string(APIDefs->Paths_GetAddonDirectory("AchievementTracker")) + "settings.json";
}

void Settings::Load()
{
    std::ifstream f(SettingsPath());
    if (!f.is_open()) return;
    try {
        json j = json::parse(f);
        ShowWindow = j.value("ShowWindow", ShowWindow);
        Opacity    = j.value("Opacity",    Opacity);
        ApiKey     = j.value("ApiKey",     ApiKey);
        if (j.contains("TrackedAchievements") && j["TrackedAchievements"].is_array()) {
            TrackedAchievements = j["TrackedAchievements"].get<std::vector<int>>();
        }
    } catch (...) {}
}

void Settings::Save()
{
    json j;
    j["ShowWindow"]          = ShowWindow;
    j["Opacity"]             = Opacity;
    j["ApiKey"]              = ApiKey;
    j["TrackedAchievements"] = TrackedAchievements;
    std::ofstream(SettingsPath()) << j.dump(4);
}
