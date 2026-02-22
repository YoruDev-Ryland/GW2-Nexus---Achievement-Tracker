#include "GW2Api.h"
#include "Shared.h"
#include <winhttp.h>
#include <sstream>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <cctype>
#include <algorithm>
#include <unordered_set>
#include <windows.h>

using json = nlohmann::json;

namespace GW2Api {

    std::map<int, Achievement>        s_Achievements;
    std::map<int, Item>                s_Items;
    std::map<int, AccountAchievement>  s_AccountAchievements;
    std::mutex                         s_Mutex;
    std::atomic<bool>                  s_LoadingAll{false};
    std::atomic<bool>                  s_Shutdown{false};
    std::unordered_set<std::string>    s_QueuedIcons;  // texNames already queued for download

    void Shutdown() { s_Shutdown = true; }

    static std::string CachePath()
    {
        return std::string(APIDefs->Paths_GetAddonDirectory("AchievementTracker")) + "achievements_cache.json";
    }

    bool HasAchievementCache()
    {
        std::ifstream f(CachePath());
        return f.good();
    }

    void SaveAchievementCache()
    {
        json arr = json::array();
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            for (const auto& kv : s_Achievements) {
                const auto& a = kv.second;
                json entry;
                entry["id"]          = a.id;
                entry["name"]        = a.name;
                entry["description"] = a.description;
                entry["requirement"] = a.requirement;
                entry["locked_text"] = a.locked_text;
                entry["type"]        = a.type;
                entry["icon"]        = a.icon;
                entry["flags"]       = a.flags;
                json bits = json::array();
                for (const auto& b : a.bits) {
                    json bj;
                    bj["type"] = b.type;
                    bj["id"]   = b.id;
                    bj["text"] = b.text;
                    bits.push_back(bj);
                }
                entry["bits"] = bits;
                arr.push_back(entry);
            }
        }
        std::ofstream(CachePath()) << arr.dump();
    }

    void LoadAchievementCache()
    {
        std::ifstream f(CachePath());
        if (!f.is_open()) return;
        try {
            json j = json::parse(f);
            std::lock_guard<std::mutex> lock(s_Mutex);
            for (const auto& item : j) {
                Achievement ach;
                ach.id          = item.value("id", 0);
                ach.name        = item.value("name", "");
                ach.description = item.value("description", "");
                ach.requirement = item.value("requirement", "");
                ach.locked_text = item.value("locked_text", "");
                ach.type        = item.value("type", "");
                ach.icon        = item.value("icon", "");
                if (item.contains("flags"))
                    for (const auto& flag : item["flags"])
                        ach.flags.push_back(flag.get<std::string>());
                if (item.contains("bits"))
                    for (const auto& bit : item["bits"]) {
                        AchievementBit b;
                        b.type = bit.value("type", "");
                        b.id   = bit.value("id", 0);
                        b.text = bit.value("text", "");
                        ach.bits.push_back(b);
                    }
                s_Achievements[ach.id] = ach;
            }
        } catch (...) {}
    }

    std::string HttpGet(const std::wstring& path, const std::string& apiKey)
    {
        std::string result;
        HINTERNET hSession = WinHttpOpen(L"AchievementTracker/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
        if (!hSession) return result;

        HINTERNET hConnect = WinHttpConnect(hSession,
            L"api.guildwars2.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
            nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);

        if (!apiKey.empty())
        {
            std::wstring auth = L"Authorization: Bearer ";
            auth += std::wstring(apiKey.begin(), apiKey.end());
            WinHttpAddRequestHeaders(hReq, auth.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
        }

        WinHttpSendRequest(hReq, nullptr, 0, nullptr, 0, 0, 0);
        WinHttpReceiveResponse(hReq, nullptr);

        DWORD bytesAvail = 0;
        while (WinHttpQueryDataAvailable(hReq, &bytesAvail) && bytesAvail > 0)
        {
            std::string buf(bytesAvail, '\0');
            DWORD bytesRead = 0;
            WinHttpReadData(hReq, buf.data(), bytesAvail, &bytesRead);
            result.append(buf.data(), bytesRead);
        }

        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    void FetchAchievements(const std::vector<int>& ids) {
        if (ids.empty()) return;

        std::stringstream ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            ss << ids[i];
            if (i < ids.size() - 1) ss << ",";
        }

        std::wstring path = L"/v2/achievements?ids=";
        std::string idsStr = ss.str();
        path += std::wstring(idsStr.begin(), idsStr.end());

        std::string response = HttpGet(path);
        if (response.empty()) return;

        try {
            json j = json::parse(response);
            std::lock_guard<std::mutex> lock(s_Mutex);
            for (const auto& item : j) {
                Achievement ach;
                ach.id = item.value("id", 0);
                ach.name = item.value("name", "");
                ach.description = item.value("description", "");
                ach.requirement = item.value("requirement", "");
                ach.locked_text = item.value("locked_text", "");
                ach.type = item.value("type", "");
                ach.icon = item.value("icon", "");
                
                if (item.contains("flags")) {
                    for (const auto& flag : item["flags"]) {
                        ach.flags.push_back(flag.get<std::string>());
                    }
                }

                if (item.contains("bits")) {
                    for (const auto& bit : item["bits"]) {
                        AchievementBit b;
                        b.type = bit.value("type", "");
                        b.id = bit.value("id", 0);
                        b.text = bit.value("text", "");
                        ach.bits.push_back(b);
                    }
                }
                s_Achievements[ach.id] = ach;
            }
        } catch (...) {}
    }

    void FetchItems(const std::vector<int>& ids) {
        if (ids.empty()) return;

        std::stringstream ss;
        for (size_t i = 0; i < ids.size(); ++i) {
            ss << ids[i];
            if (i < ids.size() - 1) ss << ",";
        }

        std::wstring path = L"/v2/items?ids=";
        std::string idsStr = ss.str();
        path += std::wstring(idsStr.begin(), idsStr.end());

        std::string response = HttpGet(path);
        if (response.empty()) return;

        try {
            json j = json::parse(response);
            std::lock_guard<std::mutex> lock(s_Mutex);
            for (const auto& item : j) {
                Item it;
                it.id = item.value("id", 0);
                it.name = item.value("name", "");
                it.description = item.value("description", "");
                it.type = item.value("type", "");
                it.rarity = item.value("rarity", "");
                it.icon = item.value("icon", "");
                it.chat_link = item.value("chat_link", "");
                s_Items[it.id] = it;
            }
        } catch (...) {}
    }

    void FetchAccountAchievements(const std::string& apiKey) {
        if (apiKey.empty()) return;

        std::string response = HttpGet(L"/v2/account/achievements", apiKey);
        if (response.empty()) return;

        try {
            json j = json::parse(response);
            std::lock_guard<std::mutex> lock(s_Mutex);
            for (const auto& item : j) {
                AccountAchievement ach;
                ach.id = item.value("id", 0);
                ach.current = item.value("current", 0);
                ach.max = item.value("max", 0);
                ach.done = item.value("done", false);
                
                if (item.contains("bits")) {
                    for (const auto& bit : item["bits"]) {
                        ach.bits.push_back(bit.get<int>());
                    }
                }
                s_AccountAchievements[ach.id] = ach;
            }
        } catch (...) {}
    }

    const Achievement* GetAchievement(int id) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        auto it = s_Achievements.find(id);
        if (it != s_Achievements.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const Item* GetItem(int id) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        auto it = s_Items.find(id);
        if (it != s_Items.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const AccountAchievement* GetAccountAchievement(int id) {
        std::lock_guard<std::mutex> lock(s_Mutex);
        auto it = s_AccountAchievements.find(id);
        if (it != s_AccountAchievements.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool IsLoadingAllAchievements() { return s_LoadingAll.load(); }

    int CachedAchievementCount() {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return static_cast<int>(s_Achievements.size());
    }

    void FetchAllAchievementsAsync() {
        if (s_LoadingAll.exchange(true)) return;
        std::thread([]() {
            if (s_Shutdown) { s_LoadingAll = false; return; }

            std::string resp = HttpGet(L"/v2/achievements");
            if (resp.empty() || s_Shutdown) { s_LoadingAll = false; return; }
            std::vector<int> allIds;
            try {
                auto j = nlohmann::json::parse(resp);
                for (auto& v : j) allIds.push_back(v.get<int>());
            } catch (...) { s_LoadingAll = false; return; }

            const int BATCH = 200;
            for (size_t i = 0; i < allIds.size(); i += BATCH) {
                if (s_Shutdown) break;
                std::vector<int> batch(allIds.begin() + i,
                    allIds.begin() + std::min(i + BATCH, allIds.size()));
                FetchAchievements(batch);
            }

            if (!s_Shutdown) SaveAchievementCache();
            s_LoadingAll = false;
        }).detach();
    }

    std::vector<Achievement> SearchAchievements(const std::string& query) {
        std::string lower = query;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c){ return std::tolower(c); });

        std::lock_guard<std::mutex> lock(s_Mutex);
        std::vector<Achievement> results;
        for (const auto& kv : s_Achievements) {
            std::string name = kv.second.name;
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c){ return std::tolower(c); });
            if (name.find(lower) != std::string::npos) {
                results.push_back(kv.second);
                if (results.size() >= 50) break;
            }
        }
        return results;
    }

    void FetchAndTrack(int id) {
        std::thread([id]() {
            if (s_Shutdown) return;
            FetchAchievements({id});
            if (s_Shutdown) return;
            const Achievement* ach = GetAchievement(id);
            if (ach) {
                std::vector<int> itemIds;
                for (const auto& bit : ach->bits)
                    if (bit.type == "Item") itemIds.push_back(bit.id);
                if (!itemIds.empty()) FetchItems(itemIds);
            }
            if (!s_Shutdown) LoadTextures();
        }).detach();
    }

    static bool DownloadIconToDisk(const std::wstring& urlPath, const std::string& localPath)
    {
        HINTERNET hSession = WinHttpOpen(L"AchievementTracker/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession,
            L"render.guildwars2.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
        HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", urlPath.c_str(),
            nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE);

        bool ok = false;
        if (hReq &&
            WinHttpSendRequest(hReq, nullptr, 0, nullptr, 0, 0, 0) &&
            WinHttpReceiveResponse(hReq, nullptr))
        {
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(hReq,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                nullptr, &status, &sz, nullptr);
            if (status == 200)
            {
                std::ofstream ofs(localPath, std::ios::binary);
                DWORD avail = 0;
                while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0)
                {
                    std::string buf(avail, '\0');
                    DWORD read = 0;
                    WinHttpReadData(hReq, buf.data(), avail, &read);
                    ofs.write(buf.data(), read);
                }
                ok = ofs.good();
            }
        }
        if (hReq)     WinHttpCloseHandle(hReq);
        if (hConnect) WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return ok;
    }

    static const std::string& IconsDir()
    {
        static std::string dir;
        if (dir.empty()) {
            dir = std::string(APIDefs->Paths_GetAddonDirectory("AchievementTracker")) + "icons\\";
            CreateDirectoryA(dir.c_str(), nullptr);
        }
        return dir;
    }

    static void EnsureIconCached(const std::string& url, const std::string& texName)
    {
        if (s_Shutdown || !APIDefs) return;
        if (APIDefs->Textures_Get(texName.c_str())) return;

        size_t pos = url.find("render.guildwars2.com");
        if (pos == std::string::npos) return;
        std::string urlPath = url.substr(pos + 21);

        size_t slash = urlPath.rfind('/');
        std::string filename = (slash != std::string::npos) ? urlPath.substr(slash + 1) : urlPath;
        std::string localPath = IconsDir() + filename;

        if (std::ifstream(localPath, std::ios::binary).good()) {
            APIDefs->Textures_LoadFromFile(texName.c_str(), localPath.c_str(), nullptr);
            return;
        }

        std::wstring wPath(urlPath.begin(), urlPath.end());
        if (DownloadIconToDisk(wPath, localPath))
            APIDefs->Textures_LoadFromFile(texName.c_str(), localPath.c_str(), nullptr);
    }

    void FetchAccountAchievementsAsync(const std::string& apiKey) {
        if (apiKey.empty()) return;
        std::thread([apiKey]() {
            if (!s_Shutdown) FetchAccountAchievements(apiKey);
        }).detach();
    }

    void RequestIconAsync(const std::string& url, const std::string& texName) {
        if (url.empty() || texName.empty() || s_Shutdown) return;
        // Skip if already loaded
        if (APIDefs && APIDefs->Textures_Get(texName.c_str())) return;
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            if (s_QueuedIcons.count(texName)) return;
            s_QueuedIcons.insert(texName);
        }
        std::thread([url, texName]() {
            if (!s_Shutdown) EnsureIconCached(url, texName);
        }).detach();
    }

    void LoadTextures() {
        std::vector<std::pair<std::string,std::string>> achIcons, itemIcons;
        {
            std::lock_guard<std::mutex> lock(s_Mutex);
            for (const auto& pair : s_Achievements)
                if (!pair.second.icon.empty())
                    achIcons.push_back({"ACHIEVEMENT_ICON_" + std::to_string(pair.first), pair.second.icon});
            for (const auto& pair : s_Items)
                if (!pair.second.icon.empty())
                    itemIcons.push_back({"ITEM_ICON_" + std::to_string(pair.first), pair.second.icon});
        }
        for (const auto& kv : achIcons)  EnsureIconCached(kv.second, kv.first);
        for (const auto& kv : itemIcons) EnsureIconCached(kv.second, kv.first);
    }
}
