cmake_minimum_required(VERSION 3.21)
project(ClientOpsStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Sql Concurrent Svg)

add_executable(ClientOpsStudio
    src/main.cpp
    src/app/AiConfigDialog.h
    src/app/AiConfigDialog.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/domain/LogSignals.h
    src/domain/Models.h
    src/domain/SignalExtractor.h
    src/domain/SignalExtractor.cpp
    src/domain/DiagnosisEngine.h
    src/domain/DiagnosisEngine.cpp
    src/infra/HttpClient.h
    src/infra/HttpClient.cpp
    src/infra/LogParser.h
    src/infra/LogParser.cpp
    src/infra/SqliteStore.h
    src/infra/SqliteStore.cpp
    src/infra/AiAdvisor.h
    src/infra/AiAdvisor.cpp
    src/infra/MarkdownExporter.h
    src/infra/MarkdownExporter.cpp
    resources/resources.qrc
)

target_include_directories(ClientOpsStudio PRIVATE src)

target_link_libraries(ClientOpsStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Svg
)

if (WIN32)
    target_compile_definitions(ClientOpsStudio PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()

add_executable(ClientOpsStudioTests
    tests/unit_tests.cpp
    src/domain/LogSignals.h
    src/domain/Models.h
    src/domain/SignalExtractor.h
    src/domain/SignalExtractor.cpp
    src/domain/DiagnosisEngine.h
    src/domain/DiagnosisEngine.cpp
    src/infra/LogParser.h
    src/infra/LogParser.cpp
    src/infra/SqliteStore.h
    src/infra/SqliteStore.cpp
)

target_include_directories(ClientOpsStudioTests PRIVATE src)
target_link_libraries(ClientOpsStudioTests PRIVATE Qt6::Core Qt6::Concurrent Qt6::Sql)
target_compile_definitions(ClientOpsStudioTests PRIVATE
    TEST_LOG_FILE="${CMAKE_SOURCE_DIR}/tests/data/log_parser_minimal.log"
    TEST_TIMEOUT_LOG_FILE="${CMAKE_SOURCE_DIR}/tests/data/scenario_order_timeout.log"
    TEST_DEADLOCK_LOG_FILE="${CMAKE_SOURCE_DIR}/tests/data/scenario_query_deadlock.log"
    TEST_CALLBACK_LOG_FILE="${CMAKE_SOURCE_DIR}/tests/data/scenario_payment_callback.log"
)
add_test(NAME ClientOpsStudioTests COMMAND ClientOpsStudioTests)

get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
if (NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
endif()
if (_qt_core_location)
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    set_tests_properties(ClientOpsStudioTests PROPERTIES
        ENVIRONMENT "PATH=${_qt_bin_dir};$ENV{PATH}"
    )
endif()

add_executable(LogParserBench
    tests/log_parser_bench.cpp
    src/domain/Models.h
    src/infra/LogParser.h
    src/infra/LogParser.cpp
)

target_include_directories(LogParserBench PRIVATE src)
target_link_libraries(LogParserBench PRIVATE Qt6::Core Qt6::Concurrent)
