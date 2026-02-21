#include "UI.h"
#include "Shared.h"
#include "Settings.h"
#include "GW2Api.h"
#include <imgui.h>
#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>
#include <shellapi.h>
#include <windows.h>

namespace UI {

    static void OpenURL(const std::string& url)
    {
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    static std::string WikiURL(const std::string& name)
    {
        std::string encoded;
        for (unsigned char c : name) {
            if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '(' || c == ')') {
                encoded += (char)c;
            } else if (c == ' ') {
                encoded += '_';
            } else {
                char buf[4];
                snprintf(buf, sizeof(buf), "%%%02X", c);
                encoded += buf;
            }
        }
        return "https://wiki.guildwars2.com/wiki/" + encoded;
    }

    static char  s_SearchBuf[256] = "";
    static std::vector<Achievement> s_SearchResults;
    static bool  s_SearchDirty = false;

    static std::unordered_set<int> s_TextCollapsed;

    static bool        s_ShowDeleteConfirm = false;
    static int         s_PendingDeleteId   = 0;
    static std::string s_PendingDeleteName;
    static ImVec2      s_DeleteConfirmPos  = {};

    static double s_LastProgressRefresh = 0.0;
    static bool   s_WasInGame           = false;

    static int         s_TooltipItemId = 0;
    static const Item* s_TooltipItem   = nullptr;
    static void*       s_TooltipTex    = nullptr;

    static bool IsTracked(int id)
    {
        return std::find(g_Settings.TrackedAchievements.begin(),
                         g_Settings.TrackedAchievements.end(), id)
               != g_Settings.TrackedAchievements.end();
    }

    static void RefreshProgress()
    {
        if (!g_Settings.ApiKey.empty() && !g_Settings.TrackedAchievements.empty()) {
            GW2Api::FetchAccountAchievementsAsync(g_Settings.ApiKey);
            s_LastProgressRefresh = ImGui::GetTime();
        }
    }

    static void TrackAchievement(int id)
    {
        if (IsTracked(id)) return;
        g_Settings.TrackedAchievements.push_back(id);
        g_Settings.Save();
        GW2Api::FetchAndTrack(id);
        RefreshProgress();
    }

    static void UntrackAchievement(int id)
    {
        auto& v = g_Settings.TrackedAchievements;
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        g_Settings.Save();
    }

    static void RunSearch()
    {
        s_SearchResults.clear();
        std::string q(s_SearchBuf);
        if (q.empty()) return;

        bool isNumeric = !q.empty() && std::all_of(q.begin(), q.end(), ::isdigit);
        if (isNumeric) {
            int id = std::stoi(q);
            const Achievement* ach = GW2Api::GetAchievement(id);
            if (ach) {
                s_SearchResults.push_back(*ach);
            } else {
                Achievement stub;
                stub.id   = id;
                stub.name = "Achievement #" + q;
                s_SearchResults.push_back(stub);
            }
        } else {
            s_SearchResults = GW2Api::SearchAchievements(q);
        }
    }

    static void DrawItemTooltip(const Item* item, int itemId, void* tex)
    {
        constexpr float PAD = 8.f;
        constexpr float IMG = 64.f;
        constexpr float TTW = 260.f;

        ImFont*     font   = ImGui::GetFont();
        float       fsz    = ImGui::GetFontSize();
        ImDrawList* dl     = ImGui::GetForegroundDrawList();
        ImVec2      mouse  = ImGui::GetMousePos();
        ImVec2      dispSz = ImGui::GetIO().DisplaySize;

        const std::string nameStr   = item ? item->name : ("Item #" + std::to_string(itemId));
        const std::string rarityStr = (item && !item->rarity.empty()) ? item->rarity : "";
        const std::string descStr   = (item && !item->description.empty()) ? item->description : "";

        float textColW  = TTW - (tex ? (IMG + PAD) : 0.f) - PAD * 2.f;
        ImVec2 nameSz   = font->CalcTextSizeA(fsz,         textColW, 0.f, nameStr.c_str());
        ImVec2 raritySz = rarityStr.empty() ? ImVec2{} :
                          font->CalcTextSizeA(fsz * 0.9f,  textColW, 0.f, rarityStr.c_str());
        ImVec2 descSz   = descStr.empty() ? ImVec2{} :
                          font->CalcTextSizeA(fsz, FLT_MAX, TTW - PAD * 2.f, descStr.c_str());

        float topRowH  = std::max(tex ? IMG : 0.f,
                                  nameSz.y + (rarityStr.empty() ? 0.f : PAD * 0.5f + raritySz.y));
        float totalH   = PAD + topRowH + PAD;
        if (!descStr.empty())
            totalH += 1.f + PAD + descSz.y + PAD;

        ImVec2 pos = ImVec2(mouse.x - TTW - 12.f, mouse.y - totalH * 0.5f);
        pos.x = std::max(0.f, std::min(pos.x, dispSz.x - TTW));
        pos.y = std::max(0.f, std::min(pos.y, dispSz.y - totalH));
        ImVec2 posEnd = ImVec2(pos.x + TTW, pos.y + totalH);

        dl->AddRectFilled(pos, posEnd, IM_COL32(28, 28, 28, 238), 4.f);
        dl->AddRect      (pos, posEnd, IM_COL32(90, 90, 90, 200),  4.f);

        float cx = pos.x + PAD;
        float cy = pos.y + PAD;

        if (tex) {
            dl->AddImage((ImTextureID)tex, ImVec2(cx, cy), ImVec2(cx + IMG, cy + IMG));
            cx += IMG + PAD;
        }

        dl->AddText(font, fsz, ImVec2(cx, cy), IM_COL32(255, 255, 255, 255), nameStr.c_str());
        cy += nameSz.y + PAD * 0.5f;

        if (!rarityStr.empty())
            dl->AddText(font, fsz * 0.9f, ImVec2(cx, cy), IM_COL32(160, 160, 160, 220), rarityStr.c_str());

        if (!descStr.empty()) {
            float sepY = pos.y + PAD + topRowH + PAD * 0.5f;
            dl->AddLine(ImVec2(pos.x + 4.f, sepY), ImVec2(posEnd.x - 4.f, sepY), IM_COL32(80, 80, 80, 180));
            dl->AddText(font, fsz, ImVec2(pos.x + PAD, sepY + PAD * 0.5f),
                        IM_COL32(200, 200, 200, 255), descStr.c_str(), nullptr, TTW - PAD * 2.f);
        }
    }

    static void RenderAchievement(int id, bool& removed)
    {
        const Achievement*        ach    = GW2Api::GetAchievement(id);
        const AccountAchievement* accAch = GW2Api::GetAccountAchievement(id);

        std::string headerLabel = ach ? ach->name : "Achievement #" + std::to_string(id);
        headerLabel += "##ach" + std::to_string(id);

        bool open = ImGui::CollapsingHeader(headerLabel.c_str(),
                        ImGuiTreeNodeFlags_DefaultOpen |
                        ImGuiTreeNodeFlags_AllowItemOverlap);

        float btnW = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float wikiW = ImGui::CalcTextSize("W").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float totalBtnW = wikiW + ImGui::GetStyle().ItemSpacing.x + btnW;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - totalBtnW);

        if (ImGui::SmallButton(("W##wiki" + std::to_string(id)).c_str())) {
            if (ach) OpenURL(WikiURL(ach->name));
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Wiki page");

        ImGui::SameLine();
        if (ImGui::SmallButton(("X##rm" + std::to_string(id)).c_str())) {
            s_PendingDeleteId   = id;
            s_PendingDeleteName = ach ? ach->name : "Achievement #" + std::to_string(id);
            s_ShowDeleteConfirm = true;
            s_DeleteConfirmPos  = ImGui::GetMousePos();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop tracking");

        if (!open || !ach) return;

        bool textCollapsed = s_TextCollapsed.count(id) > 0;

        ImGuiTreeNodeFlags textFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!textCollapsed) textFlags |= ImGuiTreeNodeFlags_DefaultOpen;
        bool textOpen = ImGui::TreeNodeEx(("Details##txt" + std::to_string(id)).c_str(), textFlags);

        if (textOpen  &&  textCollapsed) s_TextCollapsed.erase(id);
        if (!textOpen && !textCollapsed) s_TextCollapsed.insert(id);
        if (textOpen) {
            ImGui::TextWrapped("%s", ach->description.c_str());
            if (!ach->requirement.empty())
                ImGui::TextWrapped("%s", ach->requirement.c_str());
            ImGui::TreePop();
        }

        if (accAch && accAch->max > 0) {
            std::string prog = std::to_string(accAch->current) + "/" + std::to_string(accAch->max);
            ImGui::ProgressBar((float)accAch->current / (float)accAch->max,
                               ImVec2(-1, 0), prog.c_str());
        }

        if (ach->bits.empty()) return;

        ImGui::Spacing();
        std::string tableId = "bits##" + std::to_string(id);
        if (ImGui::BeginTable(tableId.c_str(), 8)) {
            for (size_t i = 0; i < ach->bits.size(); ++i) {
                ImGui::TableNextColumn();
                const auto& bit = ach->bits[i];

                bool isDone = false;
                if (accAch)
                    isDone = std::find(accAch->bits.begin(), accAch->bits.end(),
                                       (int)i) != accAch->bits.end();

                if (bit.type == "Item") {
                    const Item* item = GW2Api::GetItem(bit.id);
                    std::string texName = "ITEM_ICON_" + std::to_string(bit.id);
                    void* tex = GetTexResource(texName);

                    if (tex) {
                        ImGui::PushID((int)i + id * 10000);
                        ImVec4 tint = isDone ? ImVec4(1,1,1,1) : ImVec4(0.3f,0.3f,0.3f,1.f);
                        ImGui::Image((ImTextureID)tex, ImVec2(32,32),
                                     ImVec2(0,0), ImVec2(1,1), tint);

                        if (ImGui::IsItemHovered()) {
                            s_TooltipItemId = bit.id;
                            s_TooltipItem   = item;
                            s_TooltipTex    = tex;
                        }

                        if (ImGui::BeginPopupContextItem("##ctx")) {
                            std::string wikiLbl = item
                                ? ("Open Wiki: " + item->name)
                                : ("Open Wiki: Item #" + std::to_string(bit.id));
                            if (ImGui::MenuItem(wikiLbl.c_str())) {
                                std::string wikiName = item ? item->name : std::to_string(bit.id);
                                OpenURL(WikiURL(wikiName));
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    } else {
                        std::string lbl = item ? item->name : "ID " + std::to_string(bit.id);
                        ImVec4 col = isDone ? ImVec4(0.8f,0.8f,0.8f,1) : ImVec4(0.4f,0.4f,0.4f,1);
                        ImGui::TextColored(col, "%s", lbl.c_str());
                    }
                } else if (bit.type == "Text") {
                    ImVec4 col = isDone ? ImVec4(0.4f,1.0f,0.4f,1) : ImVec4(0.5f,0.5f,0.5f,1);
                    ImGui::TextColored(col, "%s", bit.text.c_str());
                }
            }
            ImGui::EndTable();
        }
    }

    static void DrawDeleteConfirm()
    {
        if (!s_ShowDeleteConfirm) return;

        ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_Always);

        ImGui::SetNextWindowPos(ImVec2(s_DeleteConfirmPos.x, s_DeleteConfirmPos.y + 8.f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        bool open = true;
        if (ImGui::Begin("Confirm##del_confirm", &open,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove))
        {
            ImGui::TextWrapped("Stop tracking:");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.f,0.9f,0.4f,1.f), "%s", s_PendingDeleteName.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("Yes, remove", ImVec2(140, 0))) {
                UntrackAchievement(s_PendingDeleteId);
                s_ShowDeleteConfirm = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140, 0)))
                s_ShowDeleteConfirm = false;
        }
        ImGui::End();
        if (!open) s_ShowDeleteConfirm = false;
    }

    void Render()
    {

        bool inGame = IsInGame();
        if (!g_Settings.ApiKey.empty() && !g_Settings.TrackedAchievements.empty()) {
            double now = ImGui::GetTime();
            bool justEntered = inGame && !s_WasInGame;
            bool timerFired  = inGame && (now - s_LastProgressRefresh) > 30.0;
            if (justEntered || timerFired) {
                GW2Api::FetchAccountAchievementsAsync(g_Settings.ApiKey);
                s_LastProgressRefresh = now;
            }
        }
        s_WasInGame = inGame;

        if (!g_Settings.ShowWindow || !inGame) return;

        DrawDeleteConfirm();

        ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(g_Settings.Opacity);

        if (!ImGui::Begin("Achievement Tracker", &g_Settings.ShowWindow)) {
            ImGui::End();
            return;
        }

        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextWithHint("##search", "Search by name or ID...",
                                     s_SearchBuf, sizeof(s_SearchBuf)))
        {
            s_SearchDirty = true;
        }

        if (s_SearchDirty) {
            RunSearch();
            s_SearchDirty = false;
        }

        if (GW2Api::IsLoadingAllAchievements()) {
            ImGui::TextDisabled("(%d cached...)", GW2Api::CachedAchievementCount());
        }

        bool hasQuery = s_SearchBuf[0] != '\0';

        if (hasQuery) {
            ImGui::Separator();
            if (s_SearchResults.empty()) {
                if (GW2Api::IsLoadingAllAchievements())
                    ImGui::TextDisabled("Still loading... try again shortly.");
                else
                    ImGui::TextDisabled("No results.");
            } else {
                float childH = std::min((float)s_SearchResults.size() * 24.0f + 8.0f, 160.0f);
                ImGui::BeginChild("##results", ImVec2(0, childH), true);
                for (const auto& ach : s_SearchResults) {
                    bool tracked = IsTracked(ach.id);
                    if (tracked) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.9f,0.5f,1));
                    bool sel = ImGui::Selectable((ach.name + "##sr" + std::to_string(ach.id)).c_str());
                    if (tracked) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(tracked ? "Already tracked" : "Click to track");
                    if (sel && !tracked) {
                        TrackAchievement(ach.id);
                        s_SearchBuf[0] = '\0';
                        s_SearchResults.clear();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::Separator();
        }

        if (g_Settings.TrackedAchievements.empty()) {
            ImGui::TextDisabled("No achievements tracked.\nSearch above to add one.");
        } else {
            bool removedAny = false;
            for (size_t i = 0; i < g_Settings.TrackedAchievements.size(); ++i) {
                bool removed = false;
                RenderAchievement(g_Settings.TrackedAchievements[i], removed);
                if (removed) { --i; removedAny = true; }
            }
            (void)removedAny;
        }

        ImGui::End();

        if (s_TooltipItemId != 0) {
            DrawItemTooltip(s_TooltipItem, s_TooltipItemId, s_TooltipTex);
            s_TooltipItemId = 0;
            s_TooltipItem   = nullptr;
            s_TooltipTex    = nullptr;
        }
    }

    void RenderOptions()
    {
        if (ImGui::Checkbox("Show Tracker", &g_Settings.ShowWindow))
            g_Settings.Save();
        if (ImGui::SliderFloat("Opacity", &g_Settings.Opacity, 0.1f, 1.0f))
            g_Settings.Save();

        ImGui::Separator();
        ImGui::TextUnformatted("GW2 API Key");
        ImGui::TextDisabled("Required to see your progress (e.g. collection bits).");
        ImGui::TextDisabled("Needs 'progression' permission.");
        static char s_ApiKeyBuf[256] = "";
        static bool s_ApiKeyBufInit  = false;
        if (!s_ApiKeyBufInit) {
            strncpy_s(s_ApiKeyBuf, sizeof(s_ApiKeyBuf), g_Settings.ApiKey.c_str(), _TRUNCATE);
            s_ApiKeyBufInit = true;
        }
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##apikey", s_ApiKeyBuf, sizeof(s_ApiKeyBuf),
                             ImGuiInputTextFlags_Password))
        {
            g_Settings.ApiKey = s_ApiKeyBuf;
            g_Settings.Save();
        }
        if (ImGui::Button("Refresh Progress")) {
            GW2Api::FetchAccountAchievementsAsync(g_Settings.ApiKey);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Re-fetches your account achievement progress from the API.");

        ImGui::Separator();
        ImGui::TextUnformatted("Achievement Data Cache");
        ImGui::Spacing();

        bool loading = GW2Api::IsLoadingAllAchievements();
        int  cached  = GW2Api::CachedAchievementCount();

        if (cached == 0 && !loading) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                "No local data. Click the button below to download\n"
                "all ~4000 achievements in the background (~1 min).\n"
                "This only needs to be done once.");
        } else if (loading) {
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Downloading achievement data...");
            ImGui::Text("%d achievements cached so far", cached);
            ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-1, 0), "Working...");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                "Cache ready -- %d achievements", cached);
            ImGui::TextDisabled("Saved to disk. Loaded automatically on next launch.");
        }

        ImGui::Spacing();

        if (loading) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.3f,0.3f,0.3f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.3f,1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f,0.3f,0.3f,1));
        }
        const char* btnLabel = (cached == 0) ? "Download Achievement Data" : "Refresh Cache";
        if (ImGui::Button(btnLabel) && !loading)
            GW2Api::FetchAllAchievementsAsync();
        if (loading) ImGui::PopStyleColor(3);

        if (!loading && cached > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(re-downloads everything from the API)");
        }
    }

}
