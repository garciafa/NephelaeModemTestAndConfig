cmake_minimum_required(VERSION 3.00)
project(NephelaeModemConfig)

set(CMAKE_CXX_STANDARD 17)

find_package (Boost REQUIRED system regex)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIRS})
endif (Boost_FOUND)

add_executable(NephelaeModemConfig src/modem_config.cpp)
target_link_libraries(NephelaeModemConfig pthread ${Boost_LIBRARIES})

add_executable(MasterNephelaeModemConfig src/master_modem_config.cpp)
target_link_libraries(MasterNephelaeModemConfig pthread ${Boost_LIBRARIES})

add_executable(ModemThroughputSend src/modem_throughtput_sender.cpp)
target_link_libraries(ModemThroughputSend pthread ${Boost_LIBRARIES})

add_executable(ModemThroughputReceive src/modem_throughput_receiver.cpp)
target_link_libraries(ModemThroughputReceive pthread ${Boost_LIBRARIES})

add_executable(ModemPingSend src/modem_ping_send.cpp)
target_link_libraries(ModemPingSend pthread ${Boost_LIBRARIES})

add_executable(ModemPingReceive src/modem_ping_receiver.cpp)
target_link_libraries(ModemPingReceive pthread ${Boost_LIBRARIES})
