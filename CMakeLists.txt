# src/apparition
cmake_minimum_required (VERSION 3.13)

cmake_policy(SET CMP0167 NEW)

PROJECT(viessmann)

add_definitions(-pthread)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "x64")

set(Boost_ARCHITECTURE "-x64")
set(BOOST_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(BOOST_USE_STATIC_RUNTIME OFF)
#set(BOOST_INCLUDEDIR "/usr/local/include/boost")

find_package(Boost ${TF_BOOST_VERSION} REQUIRED COMPONENTS program_options system date_time filesystem log log_setup json url serialization)

#message("boost lib: ${Boost_LIBRARIES}")

set(
  file_hpp
    config.hpp
    convert_json.hpp
    data_structures.hpp
    pkce.hpp
    viessmann.hpp
  )

set(
  file_cpp
    config.cpp
    convert_json.cpp
    main.cpp
    pkce.cpp
    viessmann.cpp
  )

add_executable(
  ${PROJECT_NAME}
    ${file_hpp}
    ${file_cpp}
  )

target_compile_definitions(${PROJECT_NAME} PUBLIC BOOST_LOG_DYN_LINK )

target_compile_definitions( ${PROJECT_NAME} PUBLIC SOL_LUAJIT=1 )
target_compile_definitions( ${PROJECT_NAME} PUBLIC SOL_ALL_SAFETIES_ON=1 )

target_include_directories(
  ${PROJECT_NAME} PUBLIC
    /usr/local/include
    /usr/local/include/luajit-2.1
  )

target_link_directories(
  ${PROJECT_NAME} PUBLIC
    /usr/local/lib
  )

# https://stackoverflow.com/questions/70101198/paho-mqtt-library-names
target_link_libraries(
  ${PROJECT_NAME}
    pthread
    #libou_mqtt.a
    ou_rest.a
    ou_anomalon.a
    ou_mqtt.a
    paho-mqtt3c.a
    fmt.a
    luajit-5.1
    ${Boost_LIBRARIES}
    sodium
    crypto
    ssl
    fmt
  )

