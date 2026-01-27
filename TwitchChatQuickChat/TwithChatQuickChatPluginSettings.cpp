# include "pch.h"
# include "TwitchChatQuickChat.h"

void TwitchChatQuickChat::RenderSettings() {
    ImGui::TextUnformatted("Twitch Chat Quick Chat");

    CVarWrapper enableCvar = cvarManager->getCvar("twitchChatQuickChat_enabled");  // Fixed: lowercase 't'
    if (!enableCvar) { return; }
    bool enabled = enableCvar.getBoolValue();
    if (ImGui::Checkbox("Enable plugin", &enabled)) {
        enableCvar.setValue(enabled);
        gameWrapper->Execute([this](GameWrapper* gw) {
            cvarManager->executeCommand("twitchChatQuickChatLetChat");
        });
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle Twitch Chat Quick Chat");
    }
}

