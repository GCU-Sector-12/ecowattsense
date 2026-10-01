# Shared

Things the client and the server must agree on.

- `report_schema.json`: the report format. This is the contract between client and server. A change here is a separate pull request and both sides change in the same commit.
- state names: Active, Idle, Unused.
