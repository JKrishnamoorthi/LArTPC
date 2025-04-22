cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(LArTPC)

# Point CMake to your Geant4 installation explicitly
set(Geant4_DIR "/home/krish/products/GEANT4/geant4.10.04.p02-install/lib/Geant4-10.4.2")

find_package(Geant4 REQUIRED)

add_executable(LArTPC LArTPC.cc)
target_link_libraries(LArTPC ${Geant4_LIBRARIES})
target_include_directories(LArTPC PUBLIC ${Geant4_INCLUDE_DIRS})

# Optional: install target
install(TARGETS LArTPC DESTINATION bin)
