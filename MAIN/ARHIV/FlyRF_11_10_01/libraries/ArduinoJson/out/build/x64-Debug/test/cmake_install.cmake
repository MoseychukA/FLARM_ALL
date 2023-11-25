# Install script for directory: D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/test

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/DynamicJsonBuffer/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/IntegrationTests/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/JsonArray/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/JsonBuffer/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/JsonObject/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/JsonVariant/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/JsonWriter/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/Misc/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/Polyfills/cmake_install.cmake")
  include("D:/1 GIT FLARM_ALL/MAIN/ARHIV/FlyRF_11_10_01/libraries/ArduinoJson/out/build/x64-Debug/test/StaticJsonBuffer/cmake_install.cmake")

endif()

