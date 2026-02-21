> **Model Used: Claude Sonnet 4.6**

# Achievement Tracker — GW2 Nexus Addon

A [Nexus](https://raidcore.gg/Nexus) addon for Guild Wars 2 that lets you pin achievements to a persistent overlay and track their collection progress in real time.

---

## Features

- Pin any achievement by name or ID to a persistent in-game overlay
- Live collection progress — see item counts update as you play (requires API key)
- Icon previews for required collection items
- Search by achievement name or ID
- Adjustable opacity

---

## Installation

1. Download `AchievementTracker.dll` from the [latest release](https://github.com/YoruDev-Ryland/GW2-Nexus---Achievement-Tracker/releases/latest).
2. Place it in your Nexus addons folder (typically `Guild Wars 2/addons/`).
3. Launch the game
4. Load the addon from your Nexus library

---

## Usage

- Click the Achievement Tracker icon in the Nexus quick-access bar to open the tracker window.
- Use the search box to find an achievement by name or numeric ID, then click **Add**.
- To track live completion status, enter a GW2 API key with the `progression` permission in the Options panel (Nexus → Options → Achievement Tracker).

---

## Building from source

**Requirements:** CMake ≥ 3.21, MSVC (Visual Studio 2022), Ninja, internet access for FetchContent.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
```

The output is `build/AchievementTracker.dll`.

---

## License

MIT