# Mutation report

Generated 2026-10-11 23:49:37 UTC on Linux-6.18.44-fc-v114-x86_64-with-glibc2.39
cmake version 3.28.3; c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0

| ID | Defect injected | Outcome | Expected to fail | Actually failed |
|---|---|---|---|---|
| M01 | Modulo-biased index instead of rejection sampling | **KILLED** | tst_random | tst_random |
| M02 | Final shuffle removed (required characters stay clustered at the front) | **KILLED** | tst_password_unit, tst_entropy_semantics | tst_entropy_semantics, tst_password_generator, tst_password_unit |
| M03 | Duplicate characters leak when duplicates are forbidden (swap-remove skipped) | **KILLED** | tst_password_generator, tst_password_properties, tst_robustness | tst_entropy_semantics, tst_password_generator, tst_password_properties, tst_robustness |
| M04 | C rand() used for character choice | **KILLED** | tst_security | tst_password_generator, tst_security |
| M05 | Batch uniqueness not enforced | **KILLED** | tst_password_generator | tst_password_generator, tst_robustness |
| M06 | Excluded characters ignored when building pools | **KILLED** | tst_password_unit | tst_entropy_semantics, tst_password_generator, tst_password_properties, tst_password_unit, tst_robustness |
| M07 | Non-CSPRNG provider accepted by security-sensitive generator | **KILLED** | tst_password_generator, tst_security | tst_password_generator, tst_secure_random, tst_security |
| M08 | RandomSourceError swallowed (generation 'succeeds' with a failed RNG) | **KILLED** | tst_password_generator, tst_security | tst_password_generator, tst_secure_random, tst_security |
| M09 | Log channel lets sensitive values through | **KILLED** | tst_sensitive, tst_security | tst_security, tst_sensitive |
| M10 | Minimum category counts ignored during generation | **KILLED** | tst_password_generator, tst_password_properties, tst_robustness | concurrency_check, tst_entropy_semantics, tst_password_generator, tst_password_properties, tst_robustness |
| M11 | Entropy lower bound reported as the upper bound (overstated) | **KILLED** | tst_password_unit, tst_entropy_semantics | tst_entropy_semantics, tst_password_unit |
| M12 | Band classified from the optimistic upper bound | **KILLED** | tst_password_unit | tst_password_unit |
| M13 | Min-entropy lower bound made unsound (one extra fill symbol credited) | **KILLED** | tst_password_unit, tst_entropy_semantics | tst_entropy_semantics, tst_password_unit |
| M14 | Workspace channel stops omitting sensitive values | **KILLED** | tst_sensitive | tst_sensitive |
| M15 | Undeclared output fields fail open instead of closed | **KILLED** | tst_sensitive, tst_security | tst_security, tst_sensitive |
| M16 | Parameter-count ceiling removed | **KILLED** | tst_robustness | tst_robustness |
| M17 | Generated-length ceiling ignores ApplicationLimits | **KILLED** | tst_robustness | tst_robustness |
| M18 | Cancellation never observed | **KILLED** | tst_password_generator, tst_robustness, concurrency_check | concurrency_check, tst_password_generator, tst_robustness |
| M19 | Batch ceiling not enforced | **KILLED** | tst_password_generator, tst_robustness, tst_security | tst_password_generator, tst_robustness, tst_security |
| M20 | Provably impossible unique batches no longer rejected up front | **KILLED** | tst_password_unit, tst_robustness | tst_password_unit, tst_robustness |
| M21 | Dangerous Unicode accepted as custom characters | **KILLED** | tst_password_unit, tst_robustness | tst_password_unit, tst_robustness, tst_security |
| M22 | Plugin API version not checked | **KILLED** | tst_plugin_boundary | tst_plugin_boundary |
| M23 | Secure provider ignores an entropy-source failure and returns the (zero) buffer | **KILLED** | tst_secure_random | tst_secure_random |
| M24 | Registry accepts duplicate generator ids | **KILLED** | tst_registry, tst_plugin_boundary | tst_plugin_boundary, tst_registry |
| M25 | Default (production) SecureRandomProvider is no longer attested as secure | **KILLED** | tst_secure_random | concurrency_check, tst_password_generator, tst_password_properties, tst_random, tst_robustness, tst_secure_random, tst_security |
| M26 | Real OS backend silently reports success when getrandom fails | **KILLED** | tst_secure_random | tst_secure_random |
| M27 | Export/Clipboard release sensitive values without authorization | **KILLED** | tst_sensitive | tst_sensitive |
| M28 | Authorization is not bound to its channel | **KILLED** | tst_sensitive | tst_sensitive |
| M29 | requiresConfirmation ignores undeclared record fields | **KILLED** | tst_sensitive | tst_sensitive |
| M30 | Production GenerationContext permits insecure randomness | **KILLED** | tst_password_generator, tst_secure_random | tst_password_generator, tst_secure_random, tst_security |
| M31 | Provider built around an injected source is attested as secure | **KILLED** | tst_secure_random | tst_secure_random |
| M32 | Attestation flag defaults to true for every provider | **KILLED** | tst_secure_random, tst_password_generator, tst_security | tst_password_generator, tst_random, tst_secure_random, tst_security |
| M33 | Nested objects/arrays are no longer treated as sensitive as a whole | **KILLED** | tst_sensitive | tst_sensitive |
| M34 | validateRecord stops rejecting nested values | **KILLED** | tst_sensitive | tst_sensitive |
| M35 | TSan checker accepts the watchdog name anywhere in the report (old, loose rule) | **KILLED** | tsan_checker_selftest | tsan_checker_selftest |
| M36 | TSan checker accepts non-data-race report kinds as known | **KILLED** | tsan_checker_selftest | tsan_checker_selftest |

**36 of 36 mutations killed as expected.**
