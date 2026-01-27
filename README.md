# Twitch Chat Quick Chat

A lightweight BakkesMod plugin that displays Twitch chat messages in your in-game chatbox.

## Features
- Display incoming Twitch chat messages in the local in-game chatbox (they are not sent to the game server or other players).
- Works in most BakkesMod contexts (freeplay, custom training, spectator, replay, bot AI) ((even online!))

## Troubleshooting
- If authentication fails: ensure port 3000 is free.
- If no messages appear: verify the plugin is enabled and Twitch Chat Quick Chat (TCQC) has been authorized in your Twitch account.

## Requirements
- Windows with Rocket League + BakkesMod
- C++20 toolchain compatible with Visual Studio 2022
- OpenSSL & cpp-httplib via vcpkg

## Notes
I found this idea to be simple, yet really cool. I figured I'd share it in case anyone else found it useful. Enjoy!