#pragma once

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "TwitchEventSub.h"
#include <functional>
#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);


class TwitchChatQuickChat: public BakkesMod::Plugin::BakkesModPlugin
	,public SettingsWindowBase
	,public PluginWindowBase
{
	// Authentication state
	std::string accessToken_;
	std::string twitchUsername_;
	std::string twitchUserId_;
	std::string twitchChannel_;
	std::string twitchChannelId_;
	bool isLoggedIn_ = false;
	bool isAuthenticating_ = false;

	// EventSub connection
	std::unique_ptr<TwitchEventSub> twitchEventSub_;

	void onLoad() override;
	void onUnload() override;
	void LetChat();

	// Authentication
	void StartOAuthFlow();
	void OnTokenReceived(const std::string& accessToken);
	
	// Chat connection
	void ConnectToTwitchChat();
	void OnTwitchMessage(const std::string& username, const std::string& message);
	void FetchBroadcasterId(const std::string& channel, std::function<void(const std::string&)> callback);

public:
	void RenderSettings() override;
	void RenderWindow() override;
};