# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/andrew/esp/esp-idf/tools/kconfig"
  "/home/andrew/esp/ESP-Image-USB/kconfig_bin"
  "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix"
  "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix/tmp"
  "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix/src/mconf-idf-stamp"
  "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix/src"
  "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix/src/mconf-idf-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix/src/mconf-idf-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/andrew/esp/ESP-Image-USB/mconf-idf-prefix/src/mconf-idf-stamp${cfgdir}") # cfgdir has leading slash
endif()
