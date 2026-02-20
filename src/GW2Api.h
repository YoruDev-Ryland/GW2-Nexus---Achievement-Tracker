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

    // Fetches every achievement ID then downloads data in batches of 200.
    // Runs fully in a detached background thread — safe to call from AddonLoad.
    void FetchAllAchievementsAsync();
    bool IsLoadingAllAchievements();
    int  CachedAchievementCount();

    const Achievement* GetAchievement(int id);
    const Item*        GetItem(int id);
    const AccountAchievement* GetAccountAchievement(int id);

    // Case-insensitive substring search across cached achievements.
    // Returns copies so callers don't need to hold the mutex.
    std::vector<Achievement> SearchAchievements(const std::string& query);

    void LoadTextures();
    void FetchAndTrack(int id);  // fetch achievement + its items, then LoadTextures

    // Account progress — call after the player has entered the game
    void FetchAccountAchievementsAsync(const std::string& apiKey);

    // Disk cache — call LoadAchievementCache() at startup, SaveAchievementCache()
    // is called automatically after FetchAllAchievementsAsync() finishes.
    void LoadAchievementCache();
    void SaveAchievementCache();
    bool HasAchievementCache();  // true if a valid cache file exists on disk
}
