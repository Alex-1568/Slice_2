# Built-in synthetic name presets

Slice 2 embeds small offline sample lists in `src/generators/identity/data/NameDataProvider.cpp`.
The lists are for synthetic demos and test fixtures; they are not sourced from a person directory,
are not intended to represent population distributions, and are never refreshed over the network.

Locales: `en-US`, `en-GB`, `id-ID`, and `neutral`. A future data-pack loader can implement
`INameDataProvider` without modifying generator classes. Any external data pack should be local,
size-limited, schema-validated, and treated as untrusted input.
