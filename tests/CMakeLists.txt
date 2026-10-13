set(CMAKE_AUTOMOC ON)

add_library(ar_test_support INTERFACE)
target_include_directories(ar_test_support INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_link_libraries(ar_test_support INTERFACE ar_core ar_random ar_random_testing ar_generators ar_identity_generators Qt6::Test Threads::Threads)

# ar_add_test(<name> <label>...) -> tests/unit/<name>.cpp
function(ar_add_test name)
    add_executable(${name} unit/${name}.cpp)
    target_link_libraries(${name} PRIVATE ar_test_support)
    target_compile_definitions(${name} PRIVATE AR_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    ar_enable_warnings(${name})
    ar_enable_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${ARGN}")
endfunction()

ar_add_test(tst_common unit)
ar_add_test(tst_random unit)
ar_add_test(tst_schema unit)
ar_add_test(tst_registry unit)
ar_add_test(tst_sensitive unit)
ar_add_test(tst_password_unit unit)
ar_add_test(tst_password_generator integration)
ar_add_test(tst_password_properties property)
ar_add_test(tst_security security)
ar_add_test(tst_entropy_semantics unit)
ar_add_test(tst_robustness robustness)
ar_add_test(tst_plugin_boundary unit)
ar_add_test(tst_generator_boundary integration security)
ar_add_test(tst_template_engine unit security)
ar_add_test(tst_identity_generators integration property security)

# Helper process for real-syscall fault injection (Linux x86-64; exits 77 elsewhere).
add_executable(seccomp_getrandom_probe fixtures/seccomp_getrandom_probe.cpp)
target_link_libraries(seccomp_getrandom_probe PRIVATE ar_random ar_generators Qt6::Core)
ar_enable_sanitizers(seccomp_getrandom_probe)
ar_add_test(tst_secure_random security)
target_compile_definitions(tst_secure_random PRIVATE AR_SECCOMP_PROBE="$<TARGET_FILE:seccomp_getrandom_probe>")
add_dependencies(tst_secure_random seccomp_getrandom_probe)

# The TSan verdict logic is itself under test (and under mutation testing).
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_test(NAME tsan_checker_selftest COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/check_tsan.py" --self-test)
    set_tests_properties(tsan_checker_selftest PROPERTIES LABELS "tooling")

    # Compiler-negative probes ensure untrusted subclasses cannot copy the IRandomProvider base.
    add_test(NAME irandom_provider_copy_ctor_boundary
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/expect_compile_failure.py"
                --compiler "${CMAKE_CXX_COMPILER}"
                --source "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/irandom_provider_copy_ctor_attack.cpp"
                --include-dir "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME irandom_provider_copy_assign_boundary
        COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/tools/expect_compile_failure.py"
                --compiler "${CMAKE_CXX_COMPILER}"
                --source "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/irandom_provider_copy_assign_attack.cpp"
                --include-dir "${PROJECT_SOURCE_DIR}/src")
    set_tests_properties(irandom_provider_copy_ctor_boundary irandom_provider_copy_assign_boundary
        PROPERTIES LABELS "security;compile-fail")
endif()

# Plain (non-QtTest) executable so ThreadSanitizer sees only project + Qt Core code,
# without Qt Test's uninstrumented watchdog thread. See docs/VERIFICATION.md.
add_executable(concurrency_check concurrency/concurrency_check.cpp)
target_link_libraries(concurrency_check PRIVATE ar_core ar_random ar_random_testing ar_generators Threads::Threads)
target_include_directories(concurrency_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
ar_enable_warnings(concurrency_check)
ar_enable_sanitizers(concurrency_check)
add_test(NAME concurrency_check COMMAND concurrency_check)
set_tests_properties(concurrency_check PROPERTIES LABELS "concurrency")
