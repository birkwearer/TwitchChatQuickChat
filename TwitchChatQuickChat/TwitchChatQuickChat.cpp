#include "pch.h"
#include "TwitchChatQuickChat.h"
#include "Server.h"
#include "Config.h"
#include <thread>
#include <Windows.h>
#include <shellapi.h>
#include <httplib.h>
#include "URL.h"

BAKKESMOD_PLUGIN(TwitchChatQuickChat, "Twitch Chat Quick Chat", plugin_version,
    PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING | PLUGINTYPE_SPECTATOR |
    PLUGINTYPE_BOTAI | PLUGINTYPE_REPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
bool twitchChatQuickChatEnabled = false;

void TwitchChatQuickChat::onLoad()
{
	_globalCvarManager = cvarManager;
	
	cvarManager->registerCvar("twitchChatQuickChat_enabled", "0", "Enable Twitch Chat Quick Chat", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			twitchChatQuickChatEnabled = cvar.getBoolValue();
		});

	cvarManager->registerCvar("twitchChatQuickChat_channel", "", "Twitch channel to join");

	cvarManager->registerNotifier("twitchChatQuickChatLetChat", [this](std::vector<std::string> args) {
		LetChat();
	}, "Let chat", PERMISSION_ALL);
}

void TwitchChatQuickChat::onUnload()
{
	if (twitchEventSub_) {
		twitchEventSub_->Disconnect();
	}
}

void TwitchChatQuickChat::LetChat() {
	if (!twitchChatQuickChatEnabled) {
		return;
	}

	if (!isLoggedIn_) {
		if (!isAuthenticating_) {
			StartOAuthFlow();
		}
		return;
	}

	ConnectToTwitchChat();
}

void TwitchChatQuickChat::StartOAuthFlow() {
	isAuthenticating_ = true;
	
	// Build authorization URL for implicit flow
	std::string authUrl = "https://id.twitch.tv/oauth2/authorize"
		"?response_type=token"
		"&client_id=" + Config::TWITCH_CLIENT_ID +
		"&redirect_uri=" + Config::TWITCH_REDIRECT_URI +
		"&scope=" + URL::encode("user:read:chat") +
		"&force_verify=true";
	
	// Start local server to receive the token
	startAuthServerAsync(3000, [this](bool success, const std::string& token) {
		gameWrapper->Execute([this, success, token](GameWrapper* gw) {
			if (success && !token.empty()) {
				OnTokenReceived(token);
			} else {
				isAuthenticating_ = false;
			}
		});
	});
	
	// Give server a moment to start, then open browser
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	ShellExecuteA(nullptr, "open", authUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TwitchChatQuickChat::OnTokenReceived(const std::string& accessToken) {
	accessToken_ = accessToken;
	
	// Fetch username and user ID from Twitch API
	std::thread([this]() {
		httplib::SSLClient client("api.twitch.tv");
		client.set_connection_timeout(10);
		client.set_read_timeout(10);
		
		httplib::Headers headers = {
			{"Authorization", "Bearer " + accessToken_},
			{"Client-Id", Config::TWITCH_CLIENT_ID}
		};
		
		auto result = client.Get("/helix/users", headers);
		
		std::string username = "unknown";
		std::string odId;
		
		if (result && result->status == 200) {
			std::string body = result->body;
			
			// Parse login
			size_t loginPos = body.find("\"login\":\"");
			if (loginPos != std::string::npos) {
				size_t start = loginPos + 9;
				size_t end = body.find('"', start);
				if (end != std::string::npos) {
					username = body.substr(start, end - start);
				}
			}
			
			// Parse user ID
			size_t idPos = body.find("\"id\":\"");
			if (idPos != std::string::npos) {
				size_t start = idPos + 6;
				size_t end = body.find('"', start);
				if (end != std::string::npos) {
					odId = body.substr(start, end - start);
				}
			}
		}
		
		gameWrapper->Execute([this, username, odId](GameWrapper* gw) {
			twitchUsername_ = username;
			twitchUserId_ = odId;
			isLoggedIn_ = true;
			isAuthenticating_ = false;
			
			ConnectToTwitchChat();
		});
	}).detach();
}

void TwitchChatQuickChat::FetchBroadcasterId(const std::string& channel, std::function<void(const std::string&)> callback) {
	std::thread([this, channel, callback]() {
		httplib::SSLClient client("api.twitch.tv");
		client.set_connection_timeout(10);
		client.set_read_timeout(10);
		
		httplib::Headers headers = {
			{"Authorization", "Bearer " + accessToken_},
			{"Client-Id", Config::TWITCH_CLIENT_ID}
		};
		
		auto result = client.Get(("/helix/users?login=" + channel).c_str(), headers);
		
		std::string odId;
		if (result && result->status == 200) {
			std::string body = result->body;
			size_t idPos = body.find("\"id\":\"");
			if (idPos != std::string::npos) {
				size_t start = idPos + 6;
				size_t end = body.find('"', start);
				if (end != std::string::npos) {
					odId = body.substr(start, end - start);
				}
			}
		}
		
		gameWrapper->Execute([callback, odId](GameWrapper* gw) {
			callback(odId);
		});
	}).detach();
}

void TwitchChatQuickChat::ConnectToTwitchChat() {
	if (!isLoggedIn_) {
		return;
	}
	
	CVarWrapper channelCvar = cvarManager->getCvar("twitchChatQuickChat_channel");
	if (channelCvar) {
		twitchChannel_ = channelCvar.getStringValue();
	}
	
	if (twitchChannel_.empty()) {
		twitchChannel_ = twitchUsername_;
	}
	
	// Remove # prefix if present
	if (!twitchChannel_.empty() && twitchChannel_[0] == '#') {
		twitchChannel_ = twitchChannel_.substr(1);
	}
	
	// Fetch the broadcaster's user ID, then connect
	FetchBroadcasterId(twitchChannel_, [this](const std::string& fetchedId) {
		if (fetchedId.empty()) {
			return;
		}
		
		twitchChannelId_ = fetchedId;
		
		if (twitchEventSub_) {
			twitchEventSub_->Disconnect();
		}
		
		twitchEventSub_ = std::make_unique<TwitchEventSub>();
		twitchEventSub_->SetMessageCallback([this](const std::string& username, const std::string& message) {
			gameWrapper->Execute([this, username, message](GameWrapper* gw) {
				OnTwitchMessage(username, message);
			});
		});
		
		std::thread([this]() {
			twitchEventSub_->Connect(accessToken_, Config::TWITCH_CLIENT_ID, twitchUserId_, twitchChannelId_);
		}).detach();
	});
}

void TwitchChatQuickChat::OnTwitchMessage(const std::string& username, const std::string& message) {
	// Convert username to uppercase for display
	std::string displayName = username;
	for (char& c : displayName) {
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	
	gameWrapper->LogToChatbox(message, displayName);
}

void TwitchChatQuickChat::RenderWindow() {
	// Debug UI removed
}

