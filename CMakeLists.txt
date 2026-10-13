cmake_minimum_required(VERSION 3.21)

project(AccountRecommendations VERSION 0.2.0 LANGUAGES CXX)

option(AR_BUILD_TESTS "Build the automated test suite" ON)
option(AR_BUILD_BENCHMARKS "Build micro-benchmarks" OFF)
option(AR_BUILD_TOOLS "Build developer tools (CLI demo)" ON)
option(AR_BUILD_GUI "Build the Qt Widgets desktop application" ON)
option(AR_BUILD_RUNTIME_PLUGINS "Build the opt-in Qt runtime plugin loader" ON)
option(AR_BUILD_EXAMPLE_PLUGIN "Build the example native generator plugin" ON)
option(AR_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(AR_ENABLE_SANITIZERS "Enable AddressSanitizer + UBSan" OFF)
option(AR_ENABLE_TSAN "Enable ThreadSanitizer (mutually exclusive with ASan)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

set(AR_QT_COMPONENTS Core)
if(AR_BUILD_GUI)
    list(APPEND AR_QT_COMPONENTS Gui Widgets Concurrent)
endif()
if(AR_BUILD_TESTS)
    list(APPEND AR_QT_COMPONENTS Test)
endif()
# 6.4 is the oldest Qt this code base has actually been compiled and tested with.
find_package(Qt6 6.4 REQUIRED COMPONENTS ${AR_QT_COMPONENTS})
find_package(Threads REQUIRED)

# Helper: every project library gets the same settings.
function(ar_add_library name)
    add_library(${name} STATIC ${ARGN})
    target_compile_features(${name} PUBLIC cxx_std_20)
    target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    ar_enable_warnings(${name})
    ar_enable_sanitizers(${name})
endfunction()

# Shared core is intentional: runtime plugins and the host must operate on the same
# GeneratorRegistry/type implementation. Plugins still require a compatible Qt/C++ ABI.
function(ar_add_shared_library name)
    add_library(${name} SHARED ${ARGN})
    target_compile_features(${name} PUBLIC cxx_std_20)
    target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    set_target_properties(${name} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    ar_enable_warnings(${name})
    ar_enable_sanitizers(${name})
endfunction()

# ---- Layered libraries -------------------------------------------------------
# Dependency direction (enforced by target_link_libraries):
#   ar_generators  -> ar_core            (generators never see random implementations)
#   ar_random      -> ar_core            (production RNG)
#   ar_random_testing -> ar_core         (deterministic RNG; never linked by production targets)
#   tests/tools    -> everything
ar_add_shared_library(ar_core
    src/common/validation/ValidationResult.cpp
    src/domain/random/IRandomProvider.cpp
    src/domain/generator/ConfigurationSchema.cpp
    src/domain/generator/GeneratorMetadata.cpp
    src/domain/generator/IGenerator.cpp
    src/domain/generator/GeneratorRegistry.cpp
    src/domain/security/SensitiveDataPolicy.cpp
    src/domain/plugin/IGeneratorPlugin.cpp)
target_link_libraries(ar_core PUBLIC Qt6::Core)

# Production randomness only. Production targets (tools/, future app/) link THIS library.
ar_add_library(ar_random
    src/infrastructure/random/OperatingSystemEntropy.cpp
    src/infrastructure/random/SecureRandomProvider.cpp)
target_link_libraries(ar_random PUBLIC ar_core)
if(WIN32)
    target_link_libraries(ar_random PRIVATE bcrypt)
endif()

# Seeded, NOT cryptographically secure. Linked by tests and benchmarks only.
# Its include directory (testsupport/) is exported ONLY from this target, so production
# targets cannot include testing/InsecureGenerationContext.hpp or the deterministic RNG.
ar_add_library(ar_random_testing testsupport/testing/DeterministicRandomProvider.cpp)
target_include_directories(ar_random_testing PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/testsupport")
target_link_libraries(ar_random_testing PUBLIC ar_core)

ar_add_library(ar_generators
    src/generators/password/PasswordConfiguration.cpp
    src/generators/password/CharacterPools.cpp
    src/generators/password/PasswordValidator.cpp
    src/generators/password/EntropyEstimator.cpp
    src/generators/password/PasswordAlgorithm.cpp
    src/generators/password/PasswordGenerator.cpp)
target_link_libraries(ar_generators PUBLIC ar_core)

# Slice 2 synthetic identity generators. They depend only on domain contracts, never on
# concrete random-source implementations or GUI targets.
ar_add_library(ar_identity_generators
    src/generators/identity/common/TemplateEngine.cpp
    src/generators/identity/common/IdentityUtilities.cpp
    src/generators/identity/common/IdentityGeneratorBase.cpp
    src/generators/identity/data/NameDataProvider.cpp
    src/generators/identity/generators/UsernameGenerator.cpp
    src/generators/identity/generators/DisplayNameGenerator.cpp
    src/generators/identity/generators/NicknameGenerator.cpp
    src/generators/identity/generators/EmailLikeIdentifierGenerator.cpp
    src/generators/identity/generators/AccountIdGenerator.cpp
    src/generators/identity/generators/ProfileMetadataGenerator.cpp
    src/generators/identity/registerIdentityGenerators.cpp)
target_link_libraries(ar_identity_generators PUBLIC ar_core)

if(AR_BUILD_RUNTIME_PLUGINS)
    ar_add_library(ar_plugin_runtime src/infrastructure/plugins/RuntimePluginManager.cpp)
    target_link_libraries(ar_plugin_runtime PUBLIC ar_core Qt6::Core)
endif()

if(AR_BUILD_EXAMPLE_PLUGIN)
    if(NOT AR_BUILD_RUNTIME_PLUGINS)
        message(FATAL_ERROR "AR_BUILD_EXAMPLE_PLUGIN requires AR_BUILD_RUNTIME_PLUGINS=ON")
    endif()
    add_library(ar_example_generator_plugin MODULE plugins/example/ExamplePlugin.hpp plugins/example/ExamplePlugin.cpp)
    target_link_libraries(ar_example_generator_plugin PRIVATE ar_core Qt6::Core)
    target_include_directories(ar_example_generator_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(ar_example_generator_plugin PRIVATE cxx_std_20)
    set_target_properties(ar_example_generator_plugin PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins")
    ar_enable_warnings(ar_example_generator_plugin)
    ar_enable_sanitizers(ar_example_generator_plugin)
endif()

if(AR_BUILD_GUI)
    if(NOT AR_BUILD_RUNTIME_PLUGINS)
        message(FATAL_ERROR "AR_BUILD_GUI currently requires AR_BUILD_RUNTIME_PLUGINS=ON")
    endif()
    add_subdirectory(src/gui)
endif()

if(AR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(AR_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(AR_BUILD_TOOLS)
    add_subdirectory(tools/cli_demo)
endif()

# Runtime plugin integration test is separate from the GUI so it also runs headlessly.
if(AR_BUILD_TESTS AND AR_BUILD_RUNTIME_PLUGINS AND AR_BUILD_EXAMPLE_PLUGIN)
    add_executable(tst_runtime_plugin tests/unit/tst_runtime_plugin.cpp)
    target_link_libraries(tst_runtime_plugin PRIVATE ar_plugin_runtime ar_core Qt6::Test)
    target_compile_definitions(tst_runtime_plugin PRIVATE
        AR_EXAMPLE_PLUGIN_PATH="$<TARGET_FILE:ar_example_generator_plugin>"
        AR_CORE_LIBRARY_PATH="$<TARGET_FILE:ar_core>")
    add_dependencies(tst_runtime_plugin ar_example_generator_plugin)
    ar_enable_warnings(tst_runtime_plugin)
    ar_enable_sanitizers(tst_runtime_plugin)
    add_test(NAME tst_runtime_plugin COMMAND tst_runtime_plugin)
    set_tests_properties(tst_runtime_plugin PROPERTIES LABELS "integration;plugin;security")
endif()

if(AR_BUILD_TESTS AND AR_BUILD_GUI)
    add_executable(tst_gui_smoke tests/unit/tst_gui_smoke.cpp)
    target_link_libraries(tst_gui_smoke PRIVATE ar_gui Qt6::Test)
    ar_enable_warnings(tst_gui_smoke)
    ar_enable_sanitizers(tst_gui_smoke)
    add_test(NAME tst_gui_smoke COMMAND tst_gui_smoke)
    set_tests_properties(tst_gui_smoke PROPERTIES LABELS "gui;smoke;integration" ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

include(GNUInstallDirs)
install(TARGETS ar_core
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(AR_BUILD_GUI)
    install(TARGETS account_recommendations RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                                         BUNDLE DESTINATION .)
    if(UNIX AND NOT APPLE)
        set_property(TARGET account_recommendations PROPERTY INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    elseif(APPLE)
        set_property(TARGET account_recommendations PROPERTY INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()
if(AR_BUILD_EXAMPLE_PLUGIN)
    install(TARGETS ar_example_generator_plugin
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/plugins
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    if(UNIX AND NOT APPLE)
        set_property(TARGET ar_example_generator_plugin PROPERTY INSTALL_RPATH "$ORIGIN/../../${CMAKE_INSTALL_LIBDIR}")
    elseif(APPLE)
        set_property(TARGET ar_example_generator_plugin PROPERTY INSTALL_RPATH "@loader_path/../../${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()
