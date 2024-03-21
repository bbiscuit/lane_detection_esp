# Esp32 Hello OpenCV

This is a basic example to test that OpenCV works correctly on the ESP32. The project only creates some matrices and apply basic operations on them.

# Timing Statistics

These statistics are taken running at a tick-rate of 1,000 Hz, which is reflected in the
milliseconds calculation. These are also taken without using the optional `crop_row`
parameter, so speeds may be faster than here recorded.

| Routine | Ticks | Milliseconds |
| ------- | ----- | ------------ |
| `thresh_and_disp` | 36 | 36 ms |
| `get_lane_center` | 1 | 1 ms |
