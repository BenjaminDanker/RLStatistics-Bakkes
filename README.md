# RLStatistics

**BakkesMod plugin for Rocket League that tracks and displays real-time match statistics and analytics.**

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Usage](#usage)
- [Configuration](#configuration)
- [Building from Source](#building-from-source)
- [Contributing](#contributing)
- [License](#license)

---

## Features

- **Real-Time Stats**: Tracks goals, assists, saves, shots, demolitions, and more during matches.
- **On-Screen Overlay**: Displays current and cumulative statistics via a customizable HUD.
- **Telemetry & Analytics**: Collects in-game events for post-match review.
- **Chat & Keybind Commands**: Toggle overlay or reset stats on the fly.

## Requirements

1. **Install BakkesMod**: Download and install from [BakkesMod.net](https://www.bakkesmod.com/).


## Usage

- **Toggle Overlay**:
  ```
  toggle_stats
  ```
- **Reset Stats**:
  ```
  reset_stats
  ```
- **Keybind**:
  - Configure in `BakkesMod/config/config.cfg`:
    ```
    "StatsToggleKey": "F9"
    ```

## Configuration

All settings are stored in `RLStatistics.json` under `%LOCALAPPDATA%/bakkesmod/cfg/plugins/`:

```json
{
  "overlayPosition": { "x": 10, "y": 10 },
  "fontSize": 14,
  "displayStats": [
    "goals", "assists", "saves", "shots", "demoCount"
  ],
  "theme": "dark"
}
```

- **overlayPosition**: Pixel coordinates for the overlay.
- **fontSize**: Size of the display font.
- **displayStats**: List of stats to show.
- **theme**: `"light"` or `"dark"` color scheme.

## Building from Source

1. **Clone Repository**:
   ```bash
   git clone https://github.com/BenjaminDanker/RLStatistics-Bakkes.git
   cd RLStatistics-Bakkes
   ```
2. **Open Solution**:
   - Launch `RLStatistics.sln` in Visual Studio 2019 or newer.
3. **Configure SDK Paths**:
   - Install BakkesMod SDK.
   - In Project Properties, set include/lib directories to point to the BakkesMod SDK folders.
4. **Build**:
   - Select `Release` configuration and build the solution.
5. **Deploy**:
   - Copy the generated `RLStatistics.dll` to your `plugins` folder as described above.

## Contributing

Contributions are welcome! To propose a feature or fix a bug:

1. Fork the repository.
2. Create a new branch: `feature/my-feature` or `bugfix/issue-#`.
3. Commit your changes and push to your fork.
4. Open a Pull Request.

Please adhere to the existing code style and include tests where applicable.

## License

This project is licensed under the [MIT License](LICENSE).
