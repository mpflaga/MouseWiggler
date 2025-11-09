# MouseWiggler - Anti Screen Saver

An intelligent, light-activated Arduino-based mouse wiggler that prevents screen savers from activating when room lights are on, while allowing them when lights are off.

[![Arduino](https://img.shields.io/badge/Arduino-Compatible-00979D?logo=arduino)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/License-See%20Disclaimer-blue)](#disclaimer)
[![Documentation](https://img.shields.io/badge/Docs-Doxygen-brightgreen)](docs/html/index.html)

## 📖 Overview

MouseWiggler is a hardware solution that automatically detects room lighting conditions using a photocell sensor and intelligently controls mouse movement to prevent screen savers. Unlike simple timer-based wigglers, this project uses a sophisticated **dual-criteria rate-of-change detection algorithm** to distinguish between:

- ✅ **Sudden changes** (light switch on/off) → Triggers wiggle enable/disable
- ❌ **Gradual changes** (sunrise, sunset, clouds) → Ignored

This ensures the system responds to intentional lighting changes while ignoring natural ambient light variations.

## 🎯 Features

- **Intelligent Light Detection**: Dual-criteria algorithm filters gradual ambient changes
- **Randomized Wiggle Pattern**: Variable timing and direction to evade monitoring software detection
- **Minimal Mouse Movement**: Imperceptible ±1 pixel movements prevent screen saver activation
- **Visual Indicator**: Optional LED shows when wiggling is active
- **Interrupt-Driven**: Efficient timer-based architecture with no polling
- **Tunable Parameters**: Easy-to-adjust thresholds for different environments
- **Well-Documented**: Complete Doxygen documentation with detailed algorithm explanations

## 🔧 Hardware Requirements

### Components

- **Arduino Micro Pro** (or compatible with HID support)
- **CDS Photocell** (Light-Dependent Resistor)
- **2.2kΩ Resistor**
- **LED** (optional, for status indication)
- **220Ω Resistor** (optional, for LED current limiting)

### Circuit Diagram

```
        VCC (A1)
         │
         ├─── [Photocell] ─── A0 (Sense)
         │
         └─── [2.2kΩ] ─────── GND (D15)

         LED Indicator (Optional):
         D2 ─── [220Ω] ─── [LED] ─── GND
```

See [schematic.png](schematic.png) and [layout.png](layout.png) for detailed circuit diagrams.

### Pin Connections

| Component | Arduino Pin |
|-----------|-------------|
| Photocell VCC | A1 (Digital High) |
| Photocell Sense | A0 (Analog Input) |
| Photocell/Resistor GND | D15 (Digital Low) |
| LED Indicator (optional) | D2 (with resistor to GND) |

## 💡 How It Works

### Detection Algorithm

The system uses a **dual-criteria approach** to detect meaningful light changes:

1. **Signal Averaging**: Continuously samples the photocell at 100 Hz, averaging 100 samples per second
2. **Percentage Change**: Calculates immediate change from previous measurement
3. **Rate of Change**: Tracks change velocity over a 5-second history window
4. **Dual Criteria**: Requires BOTH conditions to trigger state change:
   - **Magnitude**: Change > 15% (configurable via `SUDDEN_CHANGE_THRESHOLD`)
   - **Rate**: Rate > 5% per sample period (configurable via `CHANGE_RATE_THRESHOLD`)

### Example Scenarios

| Scenario | Magnitude | Rate | Action |
|----------|-----------|------|--------|
| Light switch ON | High | High | ✅ Enable wiggling |
| Light switch OFF | High | High | ✅ Disable wiggling |
| Sunset/Sunrise | High | Low | ❌ Ignore (gradual) |
| Clouds passing | Medium | Low | ❌ Ignore (gradual) |
| Random noise | Low | Variable | ❌ Ignore (insignificant) |

### Mouse Wiggle Pattern

When lights are detected, the system performs randomized movements to evade detection:

**Timing Randomization:**
- Base period: 10 seconds (configurable via `wigglePeriod`)
- Random variation: ±3 seconds (configurable via `wigglePeriodVariation`)
- Actual wiggle interval: 7-13 seconds (randomly varies each time)
- Minimum enforced period: 3 seconds

**Movement Randomization:**
- Randomly selects one of four diagonal directions: (+1,+1), (+1,-1), (-1,+1), or (-1,-1)
- Immediately reverses the movement to return cursor to original position
- Direction changes randomly with each wiggle

**Benefits:**
- Timing variation prevents pattern recognition by monitoring software
- Random directions avoid predictable movement signatures
- Net effect: cursor returns to original position (invisible to user)
- Result: Screen saver prevented while evading automated detection

## 🚀 Installation

### Prerequisites

1. **Arduino IDE** - [Download here](https://www.arduino.cc/en/software)
2. **Required Libraries**:
   - TimerOne
   - TimerThree
   - Mouse (built-in with Arduino HID boards)

### Install Libraries

```bash
# Via Arduino IDE Library Manager:
Sketch → Include Library → Manage Libraries...
Search for: "TimerOne" and "TimerThree"
```

Or via command line:
```bash
arduino-cli lib install TimerOne
arduino-cli lib install TimerThree
```

### Upload Sketch

1. Open `MouseWiggler.ino` in Arduino IDE
2. Select your board: **Tools → Board → Arduino Leonardo** (or Micro)
3. Select your port: **Tools → Port**
4. Click **Upload** ⬆️

## ⚙️ Configuration

### Tuning Detection Sensitivity

Edit these values in [MouseWiggler.ino](MouseWiggler.ino#L57-L65) to match your environment:

```cpp
// Percentage change required to consider a light change "significant"
#define SUDDEN_CHANGE_THRESHOLD  15  // Default: 15%

// Rate of change to distinguish sudden vs gradual changes
#define CHANGE_RATE_THRESHOLD    5   // Default: 5% per sample period
```

**Adjust if needed:**
- **Too sensitive** (triggers on clouds): Increase both thresholds
- **Not sensitive enough** (misses light switch): Decrease both thresholds
- **Getting false triggers**: Increase `CHANGE_RATE_THRESHOLD`

### Other Parameters

```cpp
#define wigglePeriod           10  // Base seconds between wiggles (default: 10s)
#define wigglePeriodVariation   3  // Random variation ±seconds (default: 3s)
#define SizeOfAnalogArray     100  // Sample buffer size (default: 100 samples)
#define HISTORY_SIZE            5  // History depth for rate calculation (default: 5)
```

**Randomization Tuning:**
- **Increase `wigglePeriodVariation`** for more unpredictable timing (harder to detect)
- **Decrease `wigglePeriodVariation`** for more consistent timing
- **Actual range**: (wigglePeriod - variation) to (wigglePeriod + variation) seconds

## 🔍 Debugging

### Serial Monitor

Enable serial debugging to see real-time detection data:

1. Open **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Observe output:

```
(1000ms) ADC Prior: 45230  ADC Current: 45180  Diff: -50  Change%: -1  Rate%: 0
(2000ms) ADC Prior: 45180  ADC Current: 45200  Diff: 20   Change%: 0   Rate%: 0
(3000ms) ADC Prior: 45200  ADC Current: 67800  Diff: 22600 Change%: 50 Rate%: 10
Lights Detected ON (sudden change)
Need to wiggle the mouse (next in 12s)
Need to wiggle the mouse (next in 8s)
Need to wiggle the mouse (next in 11s)
```

Note the randomized intervals between wiggles (12s, 8s, 11s instead of constant 10s).

### LED Indicator

The optional LED on pin D2:
- **ON** = Light detected, wiggling active
- **OFF** = Dark, screen saver allowed

## 📚 Documentation

Complete documentation is available:

- **[HTML Documentation](docs/html/index.html)** - Full interactive API docs (Doxygen)
- **[Markdown Documentation](docs/MouseWiggler.md)** - Quick reference
- **[Randomization Guide](RANDOMIZATION.md)** - Detailed explanation of evasion features
- **[Documentation Guide](README_DOCS.md)** - How to regenerate docs

### Generate Documentation

**Windows:**
```batch
generate_docs.bat
```

**Linux/macOS:**
```bash
chmod +x generate_docs.sh
./generate_docs.sh
```

## 🎓 Algorithm Details

### Rate-of-Change Detection

Traditional approaches use simple threshold detection:
```
if (current > previous + threshold) → trigger
```

**Problem**: Can't distinguish between:
- Fast change: Light switch (want to detect)
- Slow change: Sunset (want to ignore)

**MouseWiggler Solution**: Tracks both magnitude AND velocity:

```cpp
percentChange = (current - previous) / previous * 100
rateOfChange = (current - oldest) / (oldest * historySize) * 100

if (percentChange > 15% AND rateOfChange > 5%) {
    // Sudden change detected!
}
```

This creates a "velocity filter" that only responds to rapid transitions.

### Circular Buffers

The system uses two circular buffers:
1. **AnalogArray[100]**: Raw ADC samples for noise reduction
2. **sumHistory[5]**: Historical averages for rate calculation

Benefits:
- Constant memory usage (no dynamic allocation)
- O(1) insertion time
- Efficient for embedded systems

## 🛠️ Troubleshooting

### Issue: Wiggler doesn't respond to light changes
- Check photocell connections (use multimeter to verify resistance changes)
- Monitor serial output to see ADC values
- Ensure photocell has adequate light difference (room light vs dark)
- Try decreasing `SUDDEN_CHANGE_THRESHOLD`

### Issue: Too many false triggers
- Increase `CHANGE_RATE_THRESHOLD` to filter slow changes
- Increase `HISTORY_SIZE` for more aggressive rate filtering
- Ensure stable lighting (avoid flickering fluorescent lights)

### Issue: Mouse not recognized
- Verify board has HID support (Leonardo, Micro, etc.)
- Check USB connection
- Ensure `Mouse.begin()` is being called

### Issue: LED always on/off
- Verify LED polarity (cathode to GND)
- Check resistor value (220Ω recommended)
- Monitor serial output to see detected light status

## 🔬 Technical Specifications

| Parameter | Value | Unit |
|-----------|-------|------|
| Sampling Rate | 100 | Hz |
| ADC Resolution | 10 | bits (0-1023) |
| Analysis Period | 1 | second |
| History Depth | 5 | seconds |
| Base Wiggle Period | 10 | seconds |
| Wiggle Period Range | 7-13 | seconds (randomized) |
| Mouse Movement | ±1 | pixels |
| Movement Directions | 4 | diagonals (random) |
| Default Threshold | 15 | % change |
| Default Rate | 5 | % per sample |
| Random Seed Sources | ADC noise + millis() | - |

## 📂 Repository Structure

```
MouseWiggler/
├── MouseWiggler.ino      # Main Arduino sketch with randomization
├── README.md             # This file - project overview
├── README_DOCS.md        # Documentation generation guide
├── RANDOMIZATION.md      # Detailed randomization feature guide
├── Doxyfile              # Doxygen configuration
├── generate_docs.bat     # Windows doc generator
├── generate_docs.sh      # Linux/macOS doc generator
├── ino2md.py             # .ino to Markdown converter
├── xml2md.py             # Doxygen XML to Markdown converter
├── schematic.png         # Circuit schematic
├── layout.png            # PCB layout
├── sketch.json           # Arduino project metadata
└── docs/                 # Generated documentation
    ├── MouseWiggler.md   # API reference (auto-generated)
    ├── html/             # HTML documentation (auto-generated)
    └── xml/              # Doxygen XML files (auto-generated)
```

## 🤝 Contributing

Contributions are welcome! Areas for improvement:

- [x] Randomized wiggle timing and direction (✅ Implemented!)
- [ ] Adaptive threshold learning based on environment
- [ ] EEPROM storage of calibration values
- [ ] Web interface for configuration
- [ ] Support for multiple photocells (multi-room)
- [ ] Power-saving sleep modes
- [ ] RGB LED for multi-state indication
- [ ] Variable movement magnitude (e.g., 1-3 pixels randomized)

## 📜 Disclaimer

The use of this software is done at your own discretion and risk and with agreement that you will be solely responsible for any damage to your computer system or loss of data that results from such activities. You are solely responsible for adequate protection, security and backup of data and equipment used in connection with any of this software, and we will not be liable for any damages that you may suffer in connection with using, modifying or distributing any of this software.

**Note**: This device simulates mouse input. Ensure its use complies with your organization's IT policies.

## 👤 Author

**Michael P. Flaga**
- Created: August 31, 2019
- Updated: November 9, 2025

## 🙏 Acknowledgments

- Arduino community for TimerOne and TimerThree libraries
- Doxygen project for documentation tools
- All contributors to the Arduino ecosystem

---

**Questions?** Check the [documentation](docs/MouseWiggler.md) or open an issue!

**Like this project?** Give it a ⭐ on GitHub!
