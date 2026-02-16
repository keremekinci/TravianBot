cmake_minimum_required(VERSION 3.16)
project(TravianChecker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Qml Quick)
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

qt_add_executable(TravianChecker
    main.cpp
    
    # Models
    src/models/Account.cpp src/models/Account.h
    src/models/Village.cpp src/models/Village.h
    src/models/Building.cpp src/models/Building.h
    
    # Parsers
    src/parsers/HtmlParser.cpp src/parsers/HtmlParser.h
    src/parsers/VillageParser.cpp src/parsers/VillageParser.h
    src/parsers/HtmlSelectors.cpp src/parsers/HtmlSelectors.h
    
    # Network
    src/network/TravianDataFetcher.cpp src/network/TravianDataFetcher.h
    src/network/Travianrequestmanager.cpp src/network/Travianrequestmanager.h
    src/network/telegramnotifier.cpp src/network/telegramnotifier.h
    src/network/telegramlogger.cpp src/network/telegramlogger.h
    
    # Managers
    src/managers/BuildQueueManager.cpp src/managers/BuildQueueManager.h
    src/managers/TroopQueueManager.cpp src/managers/TroopQueueManager.h
    src/managers/FarmListManager.cpp src/managers/FarmListManager.h
    
    # UI
    src/ui/TravianUiBridge.cpp src/ui/TravianUiBridge.h
)

set_target_properties(TravianChecker PROPERTIES OUTPUT_NAME "TravianCheckerApp")

qt_add_qml_module(TravianChecker
    URI TravianChecker
    VERSION 1.0
    QML_FILES
        src/ui/Main.qml
)

target_link_libraries(TravianChecker PRIVATE
    Qt6::Core Qt6::Network Qt6::Qml Qt6::Quick ZLIB::ZLIB
)
