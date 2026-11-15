cmake_minimum_required(VERSION 3.16)
project(PopDeck CXX RC)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT WIN32)
  message(FATAL_ERROR "PopDeck Windows icindir. (Tasinabilir cekirdek testleri icin tests/ klasorune bak.)")
endif()
if(MSVC)
  add_compile_options(/utf-8 /EHsc /permissive- /W3)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()
set(SRC
  src/main.cpp src/app_ui.cpp src/app_logic.cpp src/ui.cpp src/player.cpp src/oynatici.cpp src/hotkeys.cpp
  src/wsys.cpp src/win_codec.cpp src/util.cpp src/lang.cpp src/settings.cpp src/files.cpp src/deck.cpp src/keys.cpp
  src/gif.cpp src/scale.cpp src/image.cpp src/qr.cpp src/http.cpp src/upload.cpp src/web.cpp src/netinfo.cpp
  src/embedded_data.cpp)
if(MSVC)
  set(RES popdeck.rc)
else()
  set(RES popdeck.rc)
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} -O coff")
endif()
add_executable(PopDeck WIN32 ${SRC} ${RES})
target_include_directories(PopDeck PRIVATE src)
target_compile_definitions(PopDeck PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(PopDeck PRIVATE gdiplus gdi32 user32 kernel32 comdlg32 comctl32 shell32 ole32 oleaut32 uuid
  windowscodecs ws2_32 winmm psapi advapi32 dwmapi)
if(NOT MSVC)
  target_link_options(PopDeck PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()
# diller/ ve main/ klasorlerini exe'nin yanina kopyala
add_custom_command(TARGET PopDeck POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/diller $<TARGET_FILE_DIR:PopDeck>/diller
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/main $<TARGET_FILE_DIR:PopDeck>/main)
