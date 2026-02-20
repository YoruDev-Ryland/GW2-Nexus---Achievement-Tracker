# Achievement Tracker — GW2 Nexus Addon

A [Nexus](https://raidcore.gg/Nexus) addon for Guild Wars 2 that lets you pin achievements to a persistent overlay and track their collection progress in real time.

---

## Features

- **Search & pin** — search any achievement by name or ID and add it to your tracked list
- **Collection grid** — item collection bits are displayed as a grid of 32 × 32 icon thumbnails; completed items are shown at full brightness, missing ones are dimmed
- **Live progress** — link your GW2 API key to see your current progress bar and which collection items you've already obtained
- **Wiki integration** — click the **W** button on any achievement header, or right-click any item icon, to open the corresponding Guild Wars 2 Wiki page in your browser
- **Collapsible text** — the description and requirement text of each achievement can be collapsed independently so collections with many items stay compact
- **Offline-first cache** — achievement data is downloaded once via the GW2 API and persisted to disk; the overlay is instantly usable on subsequent game launches with no network wait
- **Disk icon cache** — item icons are downloaded once and stored locally so they load instantly on every subsequent session
- **Delete confirmation** — a small confirmation popup appears right where you click the remove button, so you don't have to reach across the screen

---

## Installation

1. Download `AchievementTracker.dll` from the [latest release](https://github.com/YoruDev-Ryland/GW2-Nexus---Achievement-Tracker/releases/latest).
2. Place it in your Nexus addons folder (typically `Guild Wars 2/addons/`).
3. Launch the game — Nexus will load the addon automatically.

---

## Usage

### Tracking an achievement
Open the tracker window (Quick Access bar or the keybind set in Nexus), type part of an achievement name or its numeric ID into the search bar, and click the result to pin it.

### Seeing your progress
1. Open the Nexus **Options** panel and navigate to the **Achievement Tracker** section.
2. Paste a GW2 API key that has the **progression** permission.
3. Click **Refresh Progress** — your current counts and completed collection bits will appear immediately.

### Downloading achievement data
On first install (or to refresh stale data) open Options → Achievement Tracker and click **Download Achievement Data**. This fetches all ~4 000 achievement definitions from the API and caches them to disk.

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
