#!/usr/bin/env bash
#
# Promote a branch (default: dev) to master, stripping the dev-only todo/ folder.
#
# Why this exists: todo/ holds working notes that live ONLY on dev. Git tracks a
# whole tree per commit and has NO per-path merge exclusion (.gitattributes
# merge=ours only resolves content conflicts, it does not stop todo/ from being
# ADDED into master), so the correct pattern is: merge everything, then remove
# todo/ in the same merge commit. This wraps that into one command.
#
# It stops BEFORE pushing so you can review the merge. Usage:
#     scripts/promote-to-master.sh [source-branch]   # default source: dev
#
set -euo pipefail

SRC="${1:-dev}"
cd "$(git rev-parse --show-toplevel)"

# Refuse to run with a dirty tree (untracked build dirs are fine).
if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
    echo "error: working tree has uncommitted tracked changes - commit or stash first" >&2
    exit 1
fi

git checkout master
git pull --ff-only origin master
# --no-ff: always a real merge commit (so we can amend the tree); --no-commit:
# stop so we can strip todo/ before the commit is written.
git merge --no-ff --no-commit "$SRC"
git rm -r --quiet --ignore-unmatch todo
git commit --no-edit

echo
echo "master is now '$SRC' with todo/ stripped. Review the merge, then push:"
echo "    git push origin master"
