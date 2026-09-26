# Touch Calibration

The XPT2046 reports raw 12-bit ADC values, not pixels. Mapping them to the
screen needs the raw value at each edge of the panel, stored as `TOUCH_CAL_*`
in `include/touch_logic.h`.

## Why this has its own procedure

Getting the range wrong is **invisible in the middle of the screen**. A linear
map with symmetric endpoint errors is still exactly right at the centre, so a
centre press proves nothing. The error only appears at the edges, where presses
clamp and silently miss whatever is there.

That is how #9 survived: `TOUCH_CAL_MAXY` was 2830 against a real 3402, so every
press on the top of the screen clamped to `sy = 0` and missed a button starting
at `y = 10`. It looked like unreliable touch hardware for an entire session.

**Measure the edges. Never extrapolate them.**

## Procedure

1. Flash the bring-up harness:

   ```bash
   pio run -e floodtest -t upload --upload-port /dev/cu.usbmodem*
   ```

2. Open a serial monitor at 115200 (`pio device monitor -e floodtest`).

3. Press firmly into **all four edges** of the glass, right at the bezel — top,
   bottom, left, right. Drag along each edge to be sure you reach the extremes.
   The harness tracks the running minimum and maximum as you go.

4. Send `c`. It prints the four constants ready to paste:

   ```
   #define TOUCH_CAL_MINX 780
   #define TOUCH_CAL_MAXX 3298
   #define TOUCH_CAL_MINY 858
   #define TOUCH_CAL_MAXY 3402
   ```

   Send `r` to start over if you want a clean run.

5. Paste them into `include/touch_logic.h`.

6. Run the host tests — they share those constants, so they will tell you if a
   known-good press no longer lands on the button:

   ```bash
   pio test -e native
   ```

7. Reflash the display firmware:

   ```bash
   pio run -e esp32s3 -t upload --upload-port /dev/cu.usbmodem*
   ```

## Notes

- Both axes run **opposite** to the display: a low raw value is the bottom/right
  of the screen, a high one is the top/left. `touch_logic.h` handles the
  inversion; the constants are plain observed minima and maxima.
- A finger has width, so pressing "the edge" lands slightly inside it. Erring a
  little wide is harmless — mapped values are clamped.
- The harness also toggles the flood relay (`1`, `0`, `p`), which is the only
  way to control the light bar if the touchscreen is unavailable.
