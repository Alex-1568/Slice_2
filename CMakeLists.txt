cmake_minimum_required(VERSION 3.21)

project(AccountRecommendations VERSION 0.1.0 LANGUAGES CXX)

option(AR_BUILD_TESTS "Build the automated test suite" ON)
option(AR_BUILD_BENCHMARKS "Build micro-benchmarks" OFF)
option(AR_BUILD_TOOLS "Build developer tools (CLI demo)" ON)
option(AR_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(AR_ENABLE_SANITIZERS "Enable AddressSanitizer + UBSan" OFF)
option(AR_ENABLE_TSAN "Enable ThreadSanitizer (mutually exclusive with ASan)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

set(AR_QT_COMPONENTS Core)
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

# ---- Layered libraries -------------------------------------------------------
# Dependency direction (enforced by target_link_libraries):
#   ar_generators  -> ar_core            (generators never see random implementations)
#   ar_random      -> ar_core            (production RNG)
#   ar_random_testing -> ar_core         (deterministic RNG; never linked by production targets)
#   tests/tools    -> everything
ar_add_library(ar_core
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
