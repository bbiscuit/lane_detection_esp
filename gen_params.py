"""Generates a header of thresholding values based upon those found in the debugger.
This just prints the header to stdout, where it can be redirected to a file."""

import json


def main():
    """The main routine."""
    # Load settings.
    with open('debugger_settings.json', 'r') as f:
        settings = json.load(f)

    # Print the preamble.
    print('///////////////////////////////////////////////////////////////////////////////////////////////////')
    print('/// A generated file which contains constants determined in the Python debugging tool.')
    print('///')
    print('/// Author: Andrew Huffman')
    print('///////////////////////////////////////////////////////////////////////////////////////////////////')
    print()
    print('#pragma once')
    print()
    print('#include <stdint.h>')
    print()

    # Print the min colors.
    print(f'constexpr uint8_t outside_thresh_min_hue = {settings["outside_thresh"]["thresh_color_min"]["hue"]};')
    print(f'constexpr uint8_t outside_thresh_min_sat = {settings["outside_thresh"]["thresh_color_min"]["saturation"]};')
    print(f'constexpr uint8_t outside_thresh_min_val = {settings["outside_thresh"]["thresh_color_min"]["value"]};')
    print()

    # Print the max colors.
    print(f'constexpr uint8_t outside_thresh_max_hue = {settings["outside_thresh"]["thresh_color_max"]["hue"]};')
    print(f'constexpr uint8_t outside_thresh_max_sat = {settings["outside_thresh"]["thresh_color_max"]["saturation"]};')
    print(f'constexpr uint8_t outside_thresh_max_val = {settings["outside_thresh"]["thresh_color_max"]["value"]};')
    print()

    # Print the cropping parameter.
    outside_cropping = settings['outside_thresh']["cropping"]
    print(f'constexpr uint8_t outside_cropping_top = {outside_cropping["top"]};')
    print(f'constexpr uint8_t outside_cropping_bottom = {outside_cropping["bottom"]};')
    print(f'constexpr uint8_t outside_cropping_left = {outside_cropping["left"]};')
    print(f'constexpr uint8_t outside_cropping_right = {outside_cropping["right"]};')
    print()

    # Print the min detection area parameters.
    print(f'constexpr uint16_t outside_min_detect_area = {settings["outside_thresh"]["min_detect_area"]};')
    print()

    # Print the min colors.
    print(f'constexpr uint8_t stop_thresh_min_hue = {settings["stop_thresh"]["thresh_color_min"]["hue"]};')
    print(f'constexpr uint8_t stop_thresh_min_sat = {settings["stop_thresh"]["thresh_color_min"]["saturation"]};')
    print(f'constexpr uint8_t stop_thresh_min_val = {settings["stop_thresh"]["thresh_color_min"]["value"]};')
    print()

    # Print the max colors.
    print(f'constexpr uint8_t stop_thresh_max_hue = {settings["stop_thresh"]["thresh_color_max"]["hue"]};')
    print(f'constexpr uint8_t stop_thresh_max_sat = {settings["stop_thresh"]["thresh_color_max"]["saturation"]};')
    print(f'constexpr uint8_t stop_thresh_max_val = {settings["stop_thresh"]["thresh_color_max"]["value"]};')
    print()

    # Print the cropping parameter.
    stop_cropping = settings['stop_thresh']["cropping"]
    print(f'constexpr uint8_t stop_cropping_top = {stop_cropping["top"]};')
    print(f'constexpr uint8_t stop_cropping_bottom = {stop_cropping["bottom"]};')
    print(f'constexpr uint8_t stop_cropping_left = {stop_cropping["left"]};')
    print(f'constexpr uint8_t stop_cropping_right = {stop_cropping["right"]};')
    print()

    # Print the min detection area parameter.
    print(f'constexpr uint16_t stop_min_detect_area = {settings["outside_thresh"]["min_detect_area"]};')
    print()

if __name__ == '__main__':
    main()
