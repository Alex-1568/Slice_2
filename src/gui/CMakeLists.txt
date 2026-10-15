add_library(ar_gui STATIC MainWindow.hpp MainWindow.cpp ExportFormatting.hpp ExportFormatting.cpp)
target_link_libraries(ar_gui PUBLIC
    ar_core ar_random ar_generators ar_identity_generators ar_plugin_runtime
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
target_include_directories(ar_gui PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(ar_gui PUBLIC cxx_std_20)
ar_enable_warnings(ar_gui)
ar_enable_sanitizers(ar_gui)

add_executable(account_recommendations ${PROJECT_SOURCE_DIR}/app/main.cpp)
target_link_libraries(account_recommendations PRIVATE ar_gui)
ar_enable_warnings(account_recommendations)
ar_enable_sanitizers(account_recommendations)
