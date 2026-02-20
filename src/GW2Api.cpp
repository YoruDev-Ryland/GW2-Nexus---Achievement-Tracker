#include "GW2Api.h"
#include "Shared.h"
#include <winhttp.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>

using json = nlohmann::json;

namespace GW2Api {

    std::map<int, Achievement> s_Achievements;
    std::map<int, Item> s_Items;
    std::map<int, AccountAchievement> s_AccountAchievements;
    std::mutex s_Mutex;

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

    void LoadTextures() {
        std::lock_guard<std::mutex> lock(s_Mutex);
        for (const auto& pair : s_Achievements) {
            if (!pair.second.icon.empty()) {
                std::string texName = "ACHIEVEMENT_ICON_" + std::to_string(pair.first);
                if (!APIDefs->Textures_Get(texName.c_str())) {
                    std::string url = pair.second.icon;
                    size_t pos = url.find("render.guildwars2.com");
                    if (pos != std::string::npos) {
                        std::string path = url.substr(pos + 21);
                        APIDefs->Textures_LoadFromURL(texName.c_str(), "https://render.guildwars2.com", path.c_str(), nullptr);
                    }
                }
            }
        }
        for (const auto& pair : s_Items) {
            if (!pair.second.icon.empty()) {
                std::string texName = "ITEM_ICON_" + std::to_string(pair.first);
                if (!APIDefs->Textures_Get(texName.c_str())) {
                    std::string url = pair.second.icon;
                    size_t pos = url.find("render.guildwars2.com");
                    if (pos != std::string::npos) {
                        std::string path = url.substr(pos + 21);
                        APIDefs->Textures_LoadFromURL(texName.c_str(), "https://render.guildwars2.com", path.c_str(), nullptr);
                    }
                }
            }
        }
    }
}
