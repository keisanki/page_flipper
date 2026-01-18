# Gemini Project Context: Flipper Zero Page Flipper

## 1. Project Overview

This project contains the source code for "Page Flipper," a Flipper Zero application that turns the device into a Bluetooth remote control. It is designed to control presentations, e-readers, or media players by sending keyboard arrow and home key presses.

The application is written in C and uses the Flipper Zero SDK (`furi`). It leverages the device's built-in Bluetooth Low Energy (BLE) capabilities to emulate a Human Interface Device (HID) keyboard. The core functionality is driven by a background polling thread for GPIO stability, allowing for control via one or two external momentary switches connected to Ground.

### Functionality
*   The App advertises itself as **PageFlip** via BLE.
*   A foot pedal switch is connected between GPIO **A7 and GND**. The following logic is implemented:
    *   A single click sends a **Right Arrow** key via BLE HID.
    *   A double click sends a **Left Arrow** key via BLE HID.
*   Optionally, a second foot pedal switch is connected between GPIO **A6 and GND**. The following logic is implemented:
    *   A single click sends a **Left Arrow** key via BLE HID.
    *   A double click sends a **Home** key (First Page) via BLE HID.
*   Clicking one of the Flipper Zero keypad arrow keys sends the corresponding cursor key via BLE HID.
*   Clicking **OK** opens a set of help pages.
*   A **short press** of the **Back** button exits the application.

### GUI
*   **App Icon:** A 10x10 pixel symbol of a page with a small arrow pointing right.
*   **Main Screen:**
    *   Title line: "Page Flipper".
    *   Control Area: Four symbolized arrow keys with 2px thick lines. Left and Right are in large rounded frames; Up and Down are in smaller stacked rounded frames.
    *   Feedback: When a key event is emitted, the corresponding box is inverted for **200ms**.
    *   Separator: A horizontal line at `y=52` separates the control area from the info area.
    *   Info Area: 
        *   Displays "Connect to PageFlip..." until a connection is established.
        *   Displays "BLE not connected." if the connection is lost.
        *   Once connected, shows button icons for OK (Help) and Back (Exit).
*   **Help Screen:**
    *   Three pages navigated using the keypad Left/Right buttons.
    *   Consistent layout with the main screen (moved text up, horizontal separator line).
    *   Page indicator dots at the bottom, aligned with the "Exit" icon.
    *   Help pages:
        1. Foot pedal (A7 to GND): Single -> Forward, Double -> Backward.
        2. Foot pedal (A6 to GND): Single -> Backward, Double -> Home.
        3. Keypad: Arrow keys send corresponding HID keys.

### Key Technologies & Concepts:
*   **Language:** C
*   **Platform:** Flipper Zero
*   **Core APIs:** Furi, FuriHal (GPIO, BT, Timer, Thread), GUI (View Dispatcher, View, Canvas).
*   **Functionality:** Bluetooth HID Keyboard, GPIO polling (10ms interval), UI Feedback Timer.
*   **Stack Size:** 4096 bytes.

Progress is tracked in a local git repository.

## 2. Building and Running

The application is compiled as an external Flipper Application (`.fap`).

### Prerequisites
*   A working Flipper Zero development environment (`ufbt`).

### Build Command
```bash
ufbt
```

### Deployment Command
```bash
ufbt launch
```

## 3. Development Conventions

*   **File Structure:**
    *   `application.fam`: Manifest file defining metadata, `ble_profile` library dependency, and 4KB stack.
    *   `page_flipper_app.c`: Main source file with documented functions and polling logic.
    *   `icons/`: Directory for graphical assets (`Ok_btn_9x9.png`, `Pin_back_arrow_10x8.png`).
    *   `page_flipper.png`: 10x10 application icon.
*   **Documentation:**
    *   Each function must have a descriptive comment at its beginning explaining its purpose, arguments, and return value.
*   **Concurrency:**
    *   A background thread (`page_flipper_worker`) polls GPIO pins PA6 and PA7 every 10ms to ensure stability and avoid interrupt-related hangs on shared pins.
    *   A single-shot `flash_timer` (200ms) handles the UI feedback for keypresses.
*   **Cleanup:** Proper de-initialization of GPIO pins (reset to Analog), Bluetooth (restore default profile), and background threads is performed in `page_flipper_app_free`.