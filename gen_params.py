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
    cropping = settings["cropping"]
    print(f'constexpr uint8_t top_cropping = {cropping["top"]};')
    print(f'constexpr uint8_t bottom_cropping = {cropping["bottom"]};')
    print(f'constexpr uint8_t left_cropping = {cropping["left"]};')
    print(f'constexpr uint8_t right_cropping = {cropping["right"]};')
    print()


if __name__ == '__main__':
    main()
