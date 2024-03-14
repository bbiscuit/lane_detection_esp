"""Generates a header of thresholding values based upon those found in the debugger.
This just prints the header to stdout, where it can be redirected to a file."""

import json


def main():
    """The main routine."""
    # Load settings.
    with open('debugger_settings.json', 'r') as f:
        settings = json.load(f)
    
    thresh_color_min = settings['thresh_color_min']
    thresh_color_max = settings['thresh_color_max']

    # Print the preamble.
    print('///////////////////////////////////////////////////////////////////////////////////////////////////')
    print('/// A generated file which contains constants for thresholding.')
    print('///')
    print('/// Author: Andrew Huffman')
    print('///////////////////////////////////////////////////////////////////////////////////////////////////')
    print()
    print('#pragma once')
    print()
    print('#include <stdint.h>')
    print()

    # Print the min colors.
    print(f'constexpr uint8_t thresh_min_hue = {thresh_color_min["hue"]};')
    print(f'constexpr uint8_t thresh_min_sat = {thresh_color_min["saturation"]};')
    print(f'constexpr uint8_t thresh_min_val = {thresh_color_min["value"]};')
    print()

    # Print the max colors.
    print(f'constexpr uint8_t thresh_max_hue = {thresh_color_max["hue"]};')
    print(f'constexpr uint8_t thresh_max_sat = {thresh_color_max["saturation"]};')
    print(f'constexpr uint8_t thresh_max_val = {thresh_color_max["value"]};')
    print()


if __name__ == '__main__':
    main()
