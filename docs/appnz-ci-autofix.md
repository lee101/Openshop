# app.nz CI Autofix

Openshop uses GitHub Actions for CI and keeps the local parity command in
`scripts/ci.sh`.

```bash
bash scripts/ci.sh
```

The app.nz CI monitor reads `.github/ci-autofix.json` when this repository is
registered as a watched project. The defaults are conservative:

- `enabled`: allow the project to be monitored.
- `autoFix`: allow an agent to create a repair branch or PR when CI is red.
- `autoMerge`: disabled by default, so repairs stay reviewable.
- `autoAddCi`: allow an agent to add or repair missing CI wiring.
- `watchGithubActions`: inspect GitHub Actions runs for the configured branch.

The agent is configurable per project. For example, an installation can replace
the default Codex runner with another CLI by setting environment variables such
as `AUTOFIX_AGENT_COMMAND`, `AUTOFIX_AGENT_MODEL`, and
`AUTOFIX_AGENT_REASONING` on the app.nz monitor host.

The repair agent should keep fixes small, reproduce failures locally with
`bash scripts/ci.sh`, and open a pull request unless `autoMerge` is explicitly
enabled for the project.
