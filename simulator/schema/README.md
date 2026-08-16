# Schema Profiles

The simulator does not require Python changes for a compatible future database
schema. Add a SQL file and a matching `*.profile.json` file in this directory,
then select its profile name when setting up and running the harness.

`agreed_v1.profile.json` is the reference profile. It keeps the agreed SQL
unchanged and describes the schema-specific information that the generic
simulator needs:

- schema name and SQL file;
- the inbound GUI and outbound/state table names;
- each table's ordered columns and cursor/index column;
- which simulator-owned tables may be cleared at run start; and
- how `gui_request` scenario values map to insert columns.

## Add a future profile

1. Add `future_v2.sql`, containing the schema creation SQL.
2. Copy `agreed_v1.profile.json` to `future_v2.profile.json`.
3. Set a unique `name`, the target `schema_name`, and `sql_file`.
4. Update only the JSON table/column mappings and GUI field sources. Supported
   sources are `date`, `time`, `floor`, and `remote`.
5. Run the explicit destructive initialization once:

```bash
./simulator/setup.sh --schema-profile future-v2 --recreate-schema
```

```powershell
.\simulator\setup.ps1 -SchemaProfile future-v2 -RecreateSchema
```

6. Run that same profile:

```bash
./simulator/run.sh --schema-profile future-v2
```

```powershell
.\simulator\run.ps1 -SchemaProfile future-v2
```

`--recreate-schema` drops and recreates only the selected schema inside the
simulator-owned MariaDB instance. It never connects to a developer's existing
server, but it does erase that simulator schema. It is deliberately required
when changing a profile that reuses an existing schema name; setup never
guesses at migrations or alters tables automatically.

Use a new profile rather than editing `agreed_v1.profile.json`; the agreed
profile remains a repeatable test-environment contract.
