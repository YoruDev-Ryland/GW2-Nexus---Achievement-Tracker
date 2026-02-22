#pragma once
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

struct AchievementBit {
    std::string type;
    int id = 0;
    std::string text;
};

struct Achievement {
    int id;
    std::string name;
    std::string description;
    std::string requirement;
    std::string locked_text;
    std::string type;
    std::vector<std::string> flags;
    std::vector<AchievementBit> bits;
    std::string icon;
};

struct Item {
    int id;
    std::string name;
    std::string description;
    std::string type;
    std::string rarity;
    std::string icon;
    std::string chat_link;
};

struct AccountAchievement {
    int id;
    int current;
    int max;
    bool done;
    std::vector<int> bits;
};

namespace GW2Api {
    std::string HttpGet(const std::wstring& path, const std::string& apiKey = "");

    void FetchAchievements(const std::vector<int>& ids);
    void FetchItems(const std::vector<int>& ids);
    void FetchAccountAchievements(const std::string& apiKey);

    void FetchAllAchievementsAsync();
    bool IsLoadingAllAchievements();
    int  CachedAchievementCount();

    const Achievement* GetAchievement(int id);
    const Item*        GetItem(int id);
    const AccountAchievement* GetAccountAchievement(int id);

    std::vector<Achievement> SearchAchievements(const std::string& query);

    void LoadTextures();
    void FetchAndTrack(int id);

    void FetchAccountAchievementsAsync(const std::string& apiKey);

    // Triggers an async download+load of a single icon if not already queued/loaded.
    // Safe to call every frame; internally de-duplicates requests.
    void RequestIconAsync(const std::string& url, const std::string& texName);

    void Shutdown();

    void LoadAchievementCache();
    void SaveAchievementCache();
    bool HasAchievementCache();
}
