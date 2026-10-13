# Slice 2 — Synthetic identity generators

## Scope

The Slice 2 generator layer extends the core framework with an offline name-data provider, a small non-evaluating
template language, shared identity-generator lifecycle handling, and six built-in generators. The desktop GUI and runtime plugin integration are documented separately in `docs/GUI.md` and `docs/PLUGIN_DEVELOPMENT.md`.

| Registry ID | Generator | Output fields |
|---|---|---|
| `username` | Username | `username` |
| `display-name` | Display Name | `displayName` |
| `nickname` | Nickname | `nickname` |
| `email-identifier` | Email-like Identifier | `emailIdentifier` |
| `account-id` | Account ID | `accountId` |
| `profile` | Profile Metadata | flat set of scalar profile fields |

The GUI and runtime plugin integration layer consumes these generators through the same core contracts. Both the CLI and GUI composition roots register built-in identity generators through `GeneratorRegistry`.

## Local name data

`INameDataProvider` is a read-only boundary returning copies of `NameDataset`. The built-in
provider has small embedded offline presets for `en-US`, `en-GB`, `id-ID`, and `neutral`. It does
not open files or use the network. The lists are sample/test data, not statistically representative
population data and not records obtained from a people directory.

The provider interface is injected into the name-driven generators, and implementations are
required to be thread-safe after construction. The typed locale enums in each generator derive from
the canonical `builtInNameLocales()` list; adding a new built-in locale requires updating that list
and supplying its dataset, but not editing each generator schema individually. Replacing or extending
lists for an existing locale can be done through a compatible provider without changing generation
algorithms. A custom provider cannot introduce a new locale key until that key is added to the canonical
schema list.

A future file-based data pack must be local, size-limited, parsed with explicit error handling,
schema-validated, and treated as untrusted input. Such a loader is not part of Slice 2.

## Template language

`TemplateEngine` is deliberately not a general-purpose expression language. It supports only
literal text, caller-supplied `{token}` values, and escaped braces `{{` / `}}`. It has no conditionals,
loops, file access, network access, reflection, or code evaluation. Each generator validates a
pattern against its explicit allowed-token list before rendering. Missing token values and malformed
syntax are errors; rendered output is bounded by `ApplicationLimits`.

Examples:

```text
{first}.{last}{number}
{first}{separator}{last}
acct_{random}
```

The `containsToken()` helper recognizes only parsed, unescaped tokens. For example, `{{random}}`
is literal template text and does not satisfy a requirement that a generated ID contain a changing
`{random}` / `{number}` / `{uuid}` token.

## Generator behavior

### Username

Supports built-in patterns (`first-last`, `firstlast`, `initial-last`, `nickname-number`) and custom
templates. Prefix, suffix, separator, numeric range, append-number behavior, allowed/blocked ASCII
characters, minimum/maximum length, local locale data, custom newline/semicolon-separated words,
batch uniqueness, and bounded retry handling are configurable. Username output is normalised to
lowercase and trimmed to the allowed character rules. If those rules cannot satisfy the minimum
length, generation fails instead of fabricating an invalid result.

### Display name

Supports professional, casual, short, stylized, creative, minimal, classic, neutral, and custom-template styles, plus
optional local custom-name lists, prefix/suffix, capitalization, maximum length, and uniqueness.

### Nickname

Supports casual, short, creative, fantasy, tech, minimal, professional, classic, neutral, and custom-template styles,
with prefixes/suffixes, optional number suffixes/ranges, custom nickname lists, maximum length, and
batch uniqueness.

### Email-like identifier

Creates an email-shaped **synthetic identifier**, not an address verified to exist. Only this
explicit built-in allow-list is accepted:

- `example.com`, `example.net`, `example.org`
- `example.test`, `example.invalid`, `demo.test`, `sample.invalid`

The local part is generated and sanitised locally. The domain is selected separately and cannot be
specified through the local-part template. The generator performs no DNS lookups, mailbox checks,
HTTP requests, or other network operations. It has no deliverability guarantee.

### Account ID

Supports UUID v4, numeric, alphanumeric, prefixed, and custom-template modes. The UUID code sets the
version and variant bits using bytes from the injected `IRandomProvider`; an identifier is **not** an
authentication secret, and this generator does not claim that identifiers are unguessable. The
selected mode's relevant parameters are validated; runtime string limits remain enforced at the
shared identity-generator boundary.

### Profile metadata

Creates a flat record with `firstName`, `lastName`, `displayName`, `username`, `locale`,
`countryCode`, `timezone`, `language`, `department`, `jobTitle`, and `tag`. All values are scalar.
Fields are synthetic fixture metadata and no real-person lookup or enrichment is performed. Locale,
country code, and timezone can be independently configured, so callers who need mutually consistent
values should select compatible settings themselves.

## Shared behavior and safety boundaries

- All six generators implement the existing `IGenerator` contract and register via
  `registerIdentityGenerators()`; the GUI does not need a generator-specific switch statement.
- Generator implementations depend on `ar_core`, never on concrete RNG libraries. Random choices are
  made through the injected `IRandomProvider`. These are synthetic identity generators, not password
  or credential generators, so they do not set `requiresSecureRandom`.
- `IdentityGeneratorBase` centralises schema validation, runtime limits, cancellation, random-source
  failure conversion, output-string limits, and bounded uniqueness retries.
- The public final `IGenerator::generateInto()` validates each output record against metadata before
  it is passed to a destination sink. Unknown keys and object/array values are rejected.
- Batch `generate()` buffers locally and returns no partial records on error or cancellation. Streaming
  `generateInto()` is not transactional: already-delivered valid records cannot be rolled back if a
  later record fails or cancellation occurs.
- Configurable lists are bounded by schema string limits and `ApplicationLimits::maxCustomListSize`.
  A generation request with impossible uniqueness requirements returns a bounded failure rather than
  retrying indefinitely.
- The current provider lists are small and compiled into the program; there is no runtime data-file
  loader and no external dependency on a service.

## Verification state

See `docs/SLICE2_VERIFICATION.md`. The source and test suite have been added, but the current sandbox
has no Qt 6 development package, so a full project configure/build/CTest and mutation run could not be
executed here. Do not treat the existence of test cases or mutation definitions as evidence that they
passed. The earlier Slice 1.1 verification record applies only to its own source checkpoint.
