# Slice 2 change list

## Added

- `src/generators/identity/common/TemplateEngine.hpp/.cpp`: non-evaluating, size-bounded `{token}` template engine with escaped braces and token detection.
- `src/generators/identity/common/IdentityUtilities.hpp/.cpp`: name selection, random digits/alphabet, UUID v4 formatting, custom list parsing, Unicode-safe built-in-data validation, and username/email local-part sanitisation.
- `src/generators/identity/common/IdentityGeneratorBase.hpp/.cpp`: common schema/runtime-limit validation, cancellation, bounded uniqueness, RNG error conversion, and record construction lifecycle.
- `src/generators/identity/data/NameDataProvider.hpp/.cpp`: thread-safe-by-contract local data provider interface and built-in `en-US`, `en-GB`, `id-ID`, and `neutral` presets.
- `src/generators/identity/generators/UsernameGenerator.*`.
- `src/generators/identity/generators/DisplayNameGenerator.*`.
- `src/generators/identity/generators/NicknameGenerator.*`.
- `src/generators/identity/generators/EmailLikeIdentifierGenerator.*`.
- `src/generators/identity/generators/AccountIdGenerator.*`.
- `src/generators/identity/generators/ProfileMetadataGenerator.*`.
- `src/generators/identity/registerIdentityGenerators.hpp/.cpp`.
- `resources/name_data/README.md`.
- `tests/unit/tst_template_engine.cpp` and `tests/unit/tst_identity_generators.cpp`.
- `docs/SLICE2.md`, `docs/SLICE2_VERIFICATION.md`, and this file.

## Changed

- Root CMake adds `ar_identity_generators`, linked only to `ar_core`.
- Test CMake adds the Slice 2 unit/integration/property/security test executables.
- CLI demo registers the identity generators alongside the password generator.
- Display-name `classic` and `neutral` styles now have explicit behavior and do not accidentally render the custom template.
- Mutation catalogue extends the existing entries with Slice 2-specific regressions; it is not reported as executed until a green Qt build baseline is available.
- README and existing design/audit references describe the Slice 2 source addition and verification caveat.

## Not in scope of the identity-generator layer

Persistence, file-based name-data loading, network lookups, account registration, and authentication workflows remain unimplemented. GUI and runtime plugin integration are now in the broader source tree and have separate documents.
