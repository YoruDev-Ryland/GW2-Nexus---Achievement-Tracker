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
        CollapsedHeaders.clear();
        if (j.contains("CollapsedHeaders") && j["CollapsedHeaders"].is_array()) {
            for (const auto& v : j["CollapsedHeaders"]) CollapsedHeaders.insert(v.get<int>());
        }
        CollapsedDetails.clear();
        if (j.contains("CollapsedDetails") && j["CollapsedDetails"].is_array()) {
            for (const auto& v : j["CollapsedDetails"]) CollapsedDetails.insert(v.get<int>());
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
    j["CollapsedHeaders"]    = json::array();
    for (int id : CollapsedHeaders) j["CollapsedHeaders"].push_back(id);
    j["CollapsedDetails"]    = json::array();
    for (int id : CollapsedDetails) j["CollapsedDetails"].push_back(id);
    std::ofstream(SettingsPath()) << j.dump(4);
}
