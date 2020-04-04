cmake_minimum_required(VERSION 3.7)

project(scream)

add_executable(${PROJECT_NAME} scream.c network.c shmem.c raw.c)

# find pulseaudio
find_package(PulseAudio)
if (PULSEAUDIO_FOUND)
  include_directories(${PULSEAUDIO_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} pulse pulse-simple)
  target_sources(${PROJECT_NAME} PRIVATE pulseaudio.c)
  option(PULSEAUDIO_ENABLE "Enable PulseAudio" ON)
endif (PULSEAUDIO_FOUND)

# find ALSA
find_package(PkgConfig)
pkg_check_modules(PC_ALSA alsa)

if (PC_ALSA_FOUND)
  include_directories(${PC_ALSA_INCLUDEDIR})
  target_link_libraries(${PROJECT_NAME} ${PC_ALSA_LIBRARIES})
  target_sources(${PROJECT_NAME} PRIVATE alsa.c)
  option(ALSA_ENABLE "Enable ALSA" ON)
endif (PC_ALSA_FOUND)

configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
