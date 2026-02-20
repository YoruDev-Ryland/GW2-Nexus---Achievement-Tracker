#include "UI.h"
#include "Shared.h"
#include "Settings.h"
#include "GW2Api.h"
#include <imgui.h>
#include <algorithm>

namespace UI {

    void RenderAchievement(int id) {
        const Achievement* ach = GW2Api::GetAchievement(id);
        if (!ach) return;

        const AccountAchievement* accAch = GW2Api::GetAccountAchievement(id);

        if (ImGui::CollapsingHeader(ach->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (ImGui::Checkbox("Show Tracker", &g_Settings.ShowWindow)) {
            g_Settings.Save();
        }
        if (ImGui::SliderFloat("Opacity", &g_Settings.Opacity, 0.1f, 1.0f)) {
            g_Settings.Save();
        }
        
        ImGui::Separator();
        ImGui::Text("Tracked Achievements (IDs):");
        
        static char inputBuffer[256] = "";
        ImGui::InputText("Add ID", inputBuffer, sizeof(inputBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            try {
                int id = std::stoi(inputBuffer);
                if (std::find(g_Settings.TrackedAchievements.begin(), g_Settings.TrackedAchievements.end(), id) == g_Settings.TrackedAchievements.end()) {
                    g_Settings.TrackedAchievements.push_back(id);
                    g_Settings.Save();
                    
                    // Fetch data for new achievement
                    std::vector<int> ids = {id};
                    GW2Api::FetchAchievements(ids);
                    
                    const Achievement* ach = GW2Api::GetAchievement(id);
                    if (ach) {
                        std::vector<int> itemIds;
                        for (const auto& bit : ach->bits) {
                            if (bit.type == "Item") {
                                itemIds.push_back(bit.id);
                            }
                        }
                        if (!itemIds.empty()) {
                            GW2Api::FetchItems(itemIds);
                        }
                    }
                    GW2Api::LoadTextures();
                }
                inputBuffer[0] = '\0';
            } catch (...) {}
        }

        for (size_t i = 0; i < g_Settings.TrackedAchievements.size(); ++i) {
            int id = g_Settings.TrackedAchievements[i];
            const Achievement* ach = GW2Api::GetAchievement(id);
            std::string label = ach ? ach->name : "ID: " + std::to_string(id);
            
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine();
            std::string btnLabel = "Remove##" + std::to_string(id);
            if (ImGui::Button(btnLabel.c_str())) {
                g_Settings.TrackedAchievements.erase(g_Settings.TrackedAchievements.begin() + i);
                g_Settings.Save();
                --i;
            }
        }
    }
}
