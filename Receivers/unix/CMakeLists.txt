cmake_minimum_required(VERSION 3.7)

project(scream LANGUAGES C)

add_executable(${PROJECT_NAME} scream.c network.c shmem.c raw.c)

# find pulseaudio
option(PULSEAUDIO_ENABLE "Enable PulseAudio" ON)
if (PULSEAUDIO_ENABLE)
  find_package(PkgConfig)
  pkg_check_modules(PULSEAUDIO libpulse-simple)
  if (PULSEAUDIO_FOUND)
    include_directories(${PULSEAUDIO_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PUBLIC ${PULSEAUDIO_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} ${PULSEAUDIO_LIBRARIES})
    target_sources(${PROJECT_NAME} PRIVATE pulseaudio.c)
  else ()
    set(PULSEAUDIO_ENABLE OFF)
  endif ()
endif ()

# find ALSA
option(ALSA_ENABLE "Enable ALSA" ON)
if (ALSA_ENABLE)
  find_package(PkgConfig)
  pkg_check_modules(PC_ALSA alsa)
  if (PC_ALSA_FOUND)
    include_directories(${PC_ALSA_INCLUDEDIR})
    target_link_libraries(${PROJECT_NAME} ${PC_ALSA_LIBRARIES})
    target_sources(${PROJECT_NAME} PRIVATE alsa.c)
  else ()
    set(ALSA_ENABLE OFF)
  endif ()
endif ()

configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
