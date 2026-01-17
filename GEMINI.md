# Gemini Project Context: Flipper Zero Page Flipper

## 1. Project Overview

This project contains the source code for "Page Flipper," a Flipper Zero application that turns the device into a Bluetooth remote control. It is designed to control presentations, e-readers, or media players by sending keyboard arrow key presses.

The application is written in C and uses the Flipper Zero SDK (`furi`). It leverages the device's built-in Bluetooth Low Energy (BLE) capabilities to emulate a Human Interface Device (HID) keyboard. The core functionality is driven by GPIO interrupts, allowing for control via one or two external momentary switches.

### Functionality
*   The App advertises itself as PageFlipper via BLE.
*   A foot pedal switch is connected betwen GPIO A7 and GND. The following logic is implemented:
    *   A single click sends a right arrow key via the BLE HID.
    *   A double click sends a left arrow key via the BLE HID.
*   Optionally a second foot pedal switch is connected between GPIO A6 and GNC. The following logic is implemented:
    *   A single click sends a left arrow key via the BLE HID.
*   Clicking one of the Flipper Zero keypad keys sends the corresponding cursor key via BLE HID.
*   Clicking OK opens a set of help pages.
*   A long press of the Back button exits the application.

### GUI
*   The application icon is a symbolized page with an arrow pointing to the right.
*   The screen starts with a title line that states the application title.
*   After the app starts the center of the screen states "Connect to PageFlipper..." until the first event is received and the app switches to its normal operation scren (see next point).
*   In normal operation, below the title are four symbolized arrow keys: Big left and right keys with in-between vertically stacked up and down keys.
*   When a key event is emitted the corresponding key icon flashes for a short time.
*   The bottom the screen shows further instructions ('OK' and 'Back' are implemented as icons).
    *   OK: Help
    *   Back: Exit
*   The help screen
    *   It has three pages that are navigates using the keypay left-right buttons.
    *   The Back button exists the help screen
    *   Help screen pages
        1. One foot pedal connected to A7: single press -> page forward, double press -> page backward
        2. Optionally one foot pedal connected to A6: single press -> page backward
        3. Keypad: corresponding key event is sent

### Key Technologies & Concepts:
*   **Language:** C
*   **Platform:** Flipper Zero
*   **Core APIs:** Flipper Zero SDK (`furi`, `furi_hal`, `gui`, `bt`)
*   **Functionality:** Bluetooth HID, GPIO interrupt handling, basic GUI.

Progress of the Application is tracked in a local git repository. The functionality of the application is created one-by-one, after each step proper compilation and installation is verified. Upon verification a git commit is created.

The app stack size is 2048 bytes.

## 2. Building and Running

The application is intended to be compiled as an external Flipper Application (`.fap`).

### Prerequisites
*   A working Flipper Zero development environment.
*   The `page_flipper` directory must be placed inside the `applications_user` folder of the Flipper Zero firmware source tree.

### Build Command
To compile the application, run the Flipper Build Tool (`ufbt`) from the root of the firmware directory:
```bash
./ufbt
```

### Deployment Command
To build, deploy, and launch the application on a connected Flipper Zero:
```bash
./ufbt launch
```

## 3. Development Conventions

*   **File Structure:** The project is self-contained within the `page_flipper` directory.
    *   `application.fam`: Application manifest file defining metadata and dependencies.
    *   `page_flipper_app.c`: Main source file containing all application logic.
    *   `icons`: Directory to store all graphics.
    *   `README.md`: Project documentation.
    *   `GEMINI.md`: Instructions for Gemini.
*   **Naming Conventions:**
    *   Functions and variables use `snake_case` with a `page_flipper_` prefix for top-level symbols (e.g., `page_flipper_app_alloc`).
    *   Types use `PascalCase` with an `App` suffix for the main struct (e.g., `PageFlipperApp`).
    *   Constants are in `UPPER_SNAKE_CASE` (e.g., `SWITCH_PIN_RIGHT`).
*   **Control Flow:** The application is event-driven.
    *   The main loop is managed by `view_dispatcher_run`.
    *   GPIO events are handled asynchronously via hardware interrupt callbacks (`page_flipper_interrupt_callback_right`, `page_flipper_interrupt_callback_left`).
    *   A software timer (`FuriTimer`) is used to distinguish between single and double presses.
*   **Exiting:** The application is exited by pressing the physical "Back" button on the Flipper Zero.
