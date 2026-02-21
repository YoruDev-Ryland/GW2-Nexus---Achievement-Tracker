#include "Shared.h"
#include "Settings.h"
#include "UI.h"
#include "GW2Api.h"
#include <imgui.h>
#include <cstring>
#include <thread>

static void OnKeybind(const char* id, bool release)
{
    if (!release && strcmp(id, "KB_ACHIEVEMENT_TRACKER_TOGGLE") == 0) {
        g_Settings.ShowWindow = !g_Settings.ShowWindow;
        g_Settings.Save();
    }
}

static void AddonLoad(AddonAPI_t* aApi)
{
    APIDefs = aApi;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(aApi->ImguiContext));
    ImGui::SetAllocatorFunctions(
        reinterpret_cast<void*(*)(size_t,void*)>(aApi->ImguiMalloc),
        reinterpret_cast<void(*)(void*,void*)>(aApi->ImguiFree));

    MumbleLink  = static_cast<Mumble::LinkedMem*>(aApi->DataLink_Get(DL_MUMBLE_LINK));
    MumbleIdent = static_cast<Mumble::Identity*>(aApi->DataLink_Get(DL_MUMBLE_LINK_IDENTITY));

    g_Settings.Load();

    // Load the local achievement name cache immediately so searching works
    // as soon as the addon starts without any network call.
    std::thread([]() { GW2Api::LoadAchievementCache(); }).detach();

    aApi->GUI_Register(RT_Render, UI::Render);
    aApi->GUI_Register(RT_OptionsRender, UI::RenderOptions);
    aApi->InputBinds_RegisterWithString("KB_ACHIEVEMENT_TRACKER_TOGGLE", OnKeybind, "(null)");
    
    aApi->Textures_GetOrCreateFromResource("ICON_ACHIEVEMENT_TRACKER", 101, Self);
    aApi->QuickAccess_Add("QA_ACHIEVEMENT_TRACKER", "ICON_ACHIEVEMENT_TRACKER", "ICON_ACHIEVEMENT_TRACKER",
                          "KB_ACHIEVEMENT_TRACKER_TOGGLE", "Achievement Tracker");

    std::thread([]() {
        if (!g_Settings.TrackedAchievements.empty()) {
            GW2Api::FetchAchievements(g_Settings.TrackedAchievements);
            std::vector<int> itemIds;
            for (int id : g_Settings.TrackedAchievements) {
                const Achievement* ach = GW2Api::GetAchievement(id);
                if (ach) {
                    for (const auto& bit : ach->bits) {
                        if (bit.type == "Item") {
                            itemIds.push_back(bit.id);
                        }
                    }
                }
            }
            if (!itemIds.empty()) {
                GW2Api::FetchItems(itemIds);
            }
            GW2Api::LoadTextures();
        }
        // Refresh account progress immediately on load if an API key is configured
        if (!g_Settings.ApiKey.empty() && !g_Settings.TrackedAchievements.empty())
            GW2Api::FetchAccountAchievementsAsync(g_Settings.ApiKey);
    }).detach();
}

static void AddonUnload()
{
    if (!APIDefs) return;
    APIDefs->GUI_Deregister(UI::Render);
    APIDefs->GUI_Deregister(UI::RenderOptions);
    APIDefs->InputBinds_Deregister("KB_ACHIEVEMENT_TRACKER_TOGGLE");
    APIDefs->QuickAccess_Remove("QA_ACHIEVEMENT_TRACKER");
    APIDefs = nullptr;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { Self = hMod; DisableThreadLibraryCalls(hMod); }
    return TRUE;
}

static AddonDefinition_t s_Def{};
extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    s_Def.Signature   = 0x41434854; // ACHT
    s_Def.APIVersion  = NEXUS_API_VERSION;
    s_Def.Name        = "Achievement Tracker";
    s_Def.Version     = { 1, 0, 0, 3 };
    s_Def.Author      = "YoruDev-Ryland";
    s_Def.Description = "Pin GW2 achievements to an overlay and track collection progress at a glance. "
                        "Search by name or ID, browse required items with icon previews, "
                        "and optionally link your API key to see live completion status.";
    s_Def.Load        = AddonLoad;
    s_Def.Unload      = AddonUnload;
    s_Def.Flags       = AF_None;
    s_Def.Provider    = UP_GitHub;
    s_Def.UpdateLink  = "https://github.com/YoruDev-Ryland/GW2-Nexus---Achievement-Tracker";
    return &s_Def;
}
