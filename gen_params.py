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
    print(f'constexpr uint8_t thresh_min_hue = {settings["thresh_color_min"]["hue"]};')
    print(f'constexpr uint8_t thresh_min_sat = {settings["thresh_color_min"]["saturation"]};')
    print(f'constexpr uint8_t thresh_min_val = {settings["thresh_color_min"]["value"]};')
    print()

    # Print the max colors.
    print(f'constexpr uint8_t thresh_max_hue = {settings["thresh_color_max"]["hue"]};')
    print(f'constexpr uint8_t thresh_max_sat = {settings["thresh_color_max"]["saturation"]};')
    print(f'constexpr uint8_t thresh_max_val = {settings["thresh_color_max"]["value"]};')
    print()

    # Print the cropping parameter.
    print(f'constexpr uint8_t crop_row = {settings["crop_row"]};')
    print()


if __name__ == '__main__':
    main()
