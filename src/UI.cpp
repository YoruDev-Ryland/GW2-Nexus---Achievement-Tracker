#include "UI.h"
#include "Shared.h"
#include "Settings.h"
#include "GW2Api.h"
#include <imgui.h>
#include <algorithm>
#include <string>
#include <vector>

namespace UI {

    // ── Persistent state ──────────────────────────────────────────────────────
    static char  s_SearchBuf[256] = "";
    static std::vector<Achievement> s_SearchResults;
    static bool  s_SearchDirty = false;

    // ── Helpers ───────────────────────────────────────────────────────────────
    static bool IsTracked(int id)
    {
        return std::find(g_Settings.TrackedAchievements.begin(),
                         g_Settings.TrackedAchievements.end(), id)
               != g_Settings.TrackedAchievements.end();
    }

    static void TrackAchievement(int id)
    {
        if (IsTracked(id)) return;
        g_Settings.TrackedAchievements.push_back(id);
        g_Settings.Save();
        GW2Api::FetchAndTrack(id);
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

    // ── Render one tracked achievement entry ──────────────────────────────────
    static void RenderAchievement(int id, bool& removed)
    {
        const Achievement*        ach    = GW2Api::GetAchievement(id);
        const AccountAchievement* accAch = GW2Api::GetAccountAchievement(id);

        std::string headerLabel = ach ? ach->name : "Achievement #" + std::to_string(id);
        headerLabel += "##ach" + std::to_string(id);

        bool open = ImGui::CollapsingHeader(headerLabel.c_str(),
                        ImGuiTreeNodeFlags_DefaultOpen |
                        ImGuiTreeNodeFlags_AllowItemOverlap);

        // Small "x" button aligned to the right of the header row
        float btnW = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - btnW);
        if (ImGui::SmallButton(("x##rm" + std::to_string(id)).c_str())) {
            removed = true;
            UntrackAchievement(id);
        }

        if (!open || !ach) return;

        ImGui::TextWrapped("%s", ach->description.c_str());
        if (!ach->requirement.empty())
            ImGui::TextWrapped("%s", ach->requirement.c_str());

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
                        ImVec4 tint = isDone ? ImVec4(1,1,1,1) : ImVec4(0.3f,0.3f,0.3f,1);
                        ImGui::Image((ImTextureID)tex, ImVec2(32,32),
                                     ImVec2(0,0), ImVec2(1,1), tint);
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", item ? item->name.c_str()
                                                   : ("ID " + std::to_string(bit.id)).c_str());
                            if (item && !item->description.empty())
                                ImGui::TextWrapped("%s", item->description.c_str());
                            ImGui::EndTooltip();
                        }
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

    // ── Main window ───────────────────────────────────────────────────────────
    void Render()
    {
        if (!g_Settings.ShowWindow || !IsInGame()) return;

        ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(g_Settings.Opacity);

        if (!ImGui::Begin("Achievement Tracker", &g_Settings.ShowWindow)) {
            ImGui::End();
            return;
        }

        // Search bar (full width)
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

        bool hasQuery = s_SearchBuf[0] != '\0';

        if (GW2Api::IsLoadingAllAchievements()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d cached...)", GW2Api::CachedAchievementCount());
        }

        // Search results dropdown
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

        // Tracked list
        if (g_Settings.TrackedAchievements.empty()) {
            ImGui::TextDisabled("No achievements tracked.\nSearch above to add one.");
        } else {
            for (size_t i = 0; i < g_Settings.TrackedAchievements.size(); ++i) {
                bool removed = false;
                RenderAchievement(g_Settings.TrackedAchievements[i], removed);
                if (removed) --i;
            }
        }

        ImGui::End();
    }

    // ── Options panel ─────────────────────────────────────────────────────────
    void RenderOptions()
    {
        if (ImGui::Checkbox("Show Tracker", &g_Settings.ShowWindow))
            g_Settings.Save();
        if (ImGui::SliderFloat("Opacity", &g_Settings.Opacity, 0.1f, 1.0f))
            g_Settings.Save();

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

        // ImGui v1.80 has no BeginDisabled — grey-out the button manually
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

} // namespace UI
