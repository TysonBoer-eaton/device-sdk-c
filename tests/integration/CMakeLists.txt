cmake_minimum_required(VERSION 3.21)
#project(derms-modbussrv-unit)

#include(../../deps/FetchGTest.cmake)

#set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
#set(HEADER_FILES ${PROJECT_INCLUDE_DIR}/IServiceNotifier.h 
#                 ${PROJECT_INCLUDE_DIR}/IModbusServerDrv.h 
#                 ${PROJECT_INCLUDE_DIR}/TypeConvHelpers.h
#                 mocks/ModbusManagerMock.h
#                 mocks/ModbusServerChannelMock.h)

#LIBMICROHTTP REQUIRED)
#find_package (CURL REQUIRED)
#find_package (LIBYAML REQUIRED)
#find_package (LIBUUID REQUIRED)
#find_package (LIBCBOR REQUIRED)
#find_package (LIBPAHO REQUIRED)
#find_package (IOT REQUIRED)
#${LINK_LIBRARIES} ${IOT_LIBRARIES})
#set (INCLUDE_DIRS ${INCLUDE_DIRS} ${IOT_INCLUDE_DIRS}


add_executable(sdkc_command_client_test
    ${CMAKE_SOURCE_DIR}/integration/command_client_test.c
)

target_include_directories(sdkc_command_client_test PUBLIC
    ${DEVSDK_DIR}
    ${DEVSDK_INCLUDE}
    ${INCLUDE_DIRS}
    ${IOT_INCLUDE_DIRS}
)

target_link_libraries(sdkc_command_client_test PRIVATE
    ${LINK_LIBRARIES} ${IOT_LIBRARIES} ${DEVSDK_LIB}
)

#add_executable (edgex-modbussrvdrv-tests
#    ModbusSrvDrvTests.cpp
#    mocks/ModbusManagerMock.cpp
#    mocks/ModbusServerChannelMock.cpp
    #mocks/MemNotifyDatabaseMock.cpp
    #mocks/SingleStoreDatabaseMock.cpp
#    ${PROJECT_SOURCE_DIR}/src/db/MemNotifySession.cpp
#    ${PROJECT_SOURCE_DIR}/src/db/SingleStoreDatabase.cpp
#    ${PROJECT_SOURCE_DIR}/src/ModbusServerDrv.cpp
#    ${PROJECT_SOURCE_DIR}/src/DataPoint.cpp
#    ${PROJECT_SOURCE_DIR}/src/TypeConvHelpers.cpp
#    ${HEADER_FILES}
#)

#target_include_directories(edgex-modbussrvdrv-tests PRIVATE ${PROJECT_INCLUDE_DIR} 
#                                                            ${PROJECT_SOURCE_DIR}/src/
#                                                            ${PROJECT_SOURCE_DIR}/src/db/
#                                                            ${IOT_INCLUDE_DIR})

#target_link_libraries(edgex-modbussrvdrv-tests PRIVATE
#    $<BUILD_INTERFACE:modbus-cpp>
#    EDGEX_CSDK
#    gtest_main
#    ${IOT_LIBRARY}
#)

#add_executable(datapoint-unittests
#    DataPointTest.cpp
#    ${PROJECT_SOURCE_DIR}/src/DataPoint.cpp
#    ${PROJECT_SOURCE_DIR}/src/TypeConvHelpers.cpp
#    ${HEADER_FILES}
#S)
