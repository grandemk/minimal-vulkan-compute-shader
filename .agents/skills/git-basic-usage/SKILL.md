---
name: git-basic-usage
description: Run git in strict non-interactive batch mode (no pager, no prompts, no editor)
---

# Git batch mode (non-interactive only)

Use this skill whenever running git from the agent.

## Hard rules

- Always run git in batch mode.
- Never use interactive git flows.
- Never rely on pager/editor prompts.

## Mandatory command shape

Prefix git commands with:

```bash
GIT_TERMINAL_PROMPT=0 git --no-pager -c core.pager=cat -c color.ui=never <args>
```

This guarantees:
- no credential prompt (`GIT_TERMINAL_PROMPT=0`)
- no pager (`--no-pager`, `core.pager=cat`)
- plain output easier to parse (`color.ui=never`)

## Forbidden patterns

- `git add -p`
- `git rebase -i`
- `git commit` without `-m`
- `git mergetool`
- any command that opens an editor/pager or waits for input

## Safe examples

Show last commit changes:

```bash
GIT_TERMINAL_PROMPT=0 git --no-pager -c core.pager=cat -c color.ui=never show --name-status --find-renames --stat --summary HEAD
```

Diff working tree:

```bash
GIT_TERMINAL_PROMPT=0 git --no-pager -c core.pager=cat -c color.ui=never diff -- <path>
```

List status (porcelain):

```bash
GIT_TERMINAL_PROMPT=0 git --no-pager -c core.pager=cat -c color.ui=never status --short
```

Single-line history:

```bash
GIT_TERMINAL_PROMPT=0 git --no-pager -c core.pager=cat -c color.ui=never log --oneline -n 20
```
