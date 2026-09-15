# Per-function provenance

Every JSON file under `functions/` describes one routine from one exact game
revision. The recommended name is `<revision>_<address>.json`, for example
`ki15d_80001234.json`.

Create a record:

```sh
python3 tools/ki_project.py new-function \
  --id ki15d_80001234 \
  --address 0x80001234 \
  --name provisional_name
```

Validate all records:

```sh
python3 tools/ki_project.py provenance-check provenance/functions
```

`TEMPLATE.json` is documentation and is skipped by the validator. Assembly,
source, and evidence paths must be relative to the repository root.

`reconstruction.source_path` is `null` while a record is only a `stub`: an
identified original routine is not yet a native implementation. Change the
status to `in-progress` and set the repository-relative source path when the
reconstruction file is created. `in-progress` and `implemented` records require
that file, and the validator rejects missing, absolute, or repository-escaping
source paths.
