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

    // ── helpers ───────────────────────────────────────────────────────────────
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

    // Run the search whenever the query changes. Tries numeric ID first, then
    // falls back to substring-by-name across the local cache.
    static void RunSearch()
    {
        s_SearchResults.clear();
        std::string q(s_SearchBuf);
        if (q.empty()) return;

        bool isNumeric = std::all_of(q.begin(), q.end(), ::isdigit);
        if (isNumeric) {
            int id = std::stoi(q);
            const Achievement* ach = GW2Api::GetAchievement(id);
            if (ach) {
                s_SearchResults.push_back(*ach);
            } else {
                Achievement stub;
                stub.id = id;
                stub.name = "Achievement #" + q;
                s_SearchResults.push_back(stub);
            }
        } else {
            s_SearchResults = GW2Api::SearchAchievements(q);
        }
    }

    void RenderAchievement(int id, bool& removed) {
        const Achievement*        ach    = GW2Api::GetAchievement(id);
        const AccountAchievement* accAch = GW2Api::GetAccountAchievement(id);

        std::string headerLabel = ach ? ach->name : "Achievement #" + std::to_string(id);
        headerLabel += "##ach" + std::to_string(id);

        bool open = ImGui::CollapsingHeader(headerLabel.c_str(),
                                            ImGuiTreeNodeFlags_DefaultOpen |
                                            ImGuiTreeNodeFlags_AllowItemOverlap);

        // "×" remove button flush right on the header row
        float btnWidth = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - btnWidth);
        if (ImGui::SmallButton(("x##rm" + std::to_string(id)).c_str())) {
            removed = true;
            UntrackAchievement(id);
        }

        if (!open || !ach) return;
            ImGui::TextWrapped("%s", ach->description.c_str());
            if (!ach->requirement.empty()) {
                ImGui::TextWrapped("%s", ach->requirement.c_str());
            }

            if (accAch) {
                std::string progressText = std::to_string(accAch->current) + "/" + std::to_string(accAch->max);
                ImGui::ProgressBar((float)accAch->current / (float)accAch->max, ImVec2(-1, 0), progressText.c_str());
            }

            if (!ach->bits.empty()) {
                ImGui::Spacing();
                int columns = 6;
                if (ImGui::BeginTable("AchievementBits", columns)) {
                    for (size_t i = 0; i < ach->bits.size(); ++i) {
                        ImGui::TableNextColumn();
                        const auto& bit = ach->bits[i];
                        
                        bool isDone = false;
                        if (accAch) {
                            isDone = std::find(accAch->bits.begin(), accAch->bits.end(), i) != accAch->bits.end();
                        }

                        if (bit.type == "Item") {
                            const Item* item = GW2Api::GetItem(bit.id);
                            if (item) {
                                std::string texName = "ITEM_ICON_" + std::to_string(item->id);
                                void* tex = GetTexResource(texName);
                                if (tex) {
                                    ImVec4 tint = isDone ? ImVec4(1, 1, 1, 1) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
                                    ImGui::Image((ImTextureID)tex, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1, 1), tint);
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::BeginTooltip();
                                        ImGui::Text("%s", item->name.c_str());
                                        ImGui::TextWrapped("%s", item->description.c_str());
                                        ImGui::EndTooltip();
                                    }
                                } else {
                                    ImGui::Text("Item %d", bit.id);
                                }
                            } else {
                                ImGui::Text("Item %d", bit.id);
                            }
                        } else if (bit.type == "Text") {
                            ImVec4 color = isDone ? ImVec4(0, 1, 0, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                            ImGui::TextColored(color, "%s", bit.text.c_str());
                        }
                    }
                    ImGui::EndTable();
                }
            }
        }
    }

    void Render() {
        if (!g_Settings.ShowWindow || !IsInGame()) return;

        ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(g_Settings.Opacity);
        
        if (ImGui::Begin("Tracked Achievements", &g_Settings.ShowWindow)) {
            if (ImGui::Button("Collapse All")) {
                // Implement collapse all logic if needed
            }

            for (int id : g_Settings.TrackedAchievements) {
                RenderAchievement(id);
            }
        }
        ImGui::End();
    }

    void RenderOptions() {
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
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f),
                "Downloading achievement data...");
            ImGui::Text("%d achievements cached so far", cached);
            ImGui::ProgressBar(-1.0f * (float)ImGui::GetTime(), ImVec2(-1, 0), "Working...");
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                "Cache ready -- %d achievements", cached);
            ImGui::TextDisabled("Saved to disk. Loaded automatically on next launch.");
        }

        ImGui::Spacing();

        if (loading) ImGui::BeginDisabled();
        const char* btnLabel = (cached == 0) ? "Download Achievement Data" : "Refresh Cache";
        if (ImGui::Button(btnLabel))
            GW2Api::FetchAllAchievementsAsync();
        if (loading) ImGui::EndDisabled();

        if (!loading && cached > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(re-downloads everything from the API)");
        }
    }
}
