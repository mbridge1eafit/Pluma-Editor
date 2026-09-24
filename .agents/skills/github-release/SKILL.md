---
name: github-release
description: >-
  Automates the preparation, changelog/release notes generation, packaging, and publishing
  of new versions to GitHub Releases for the Pluma Editor project. Use whenever the user
  asks to release a new version, publish a release, generate release notes, tag a release,
  or automate GitHub releases with checksums.
---

# GitHub Release Automation for Pluma

This skill guides and automates the creation of official GitHub Releases for **Pluma Editor**, adhering to open-source standards with SHA-256 checksums, conventional commit changelogs, and CI/CD automation.

## Quick Summary

The project provides three automated scripts in `scripts/`:
1. `scripts/package_release.ps1`: Builds the portable ZIP and computes `SHA256SUMS.txt`.
2. `scripts/generate_release_notes.ps1`: Parses Conventional Commits into categorized Markdown release notes.
3. `scripts/publish_release.ps1`: Complete orchestrator combining pre-flight checks, building, testing, packaging, release notes, and publication.

---

## Release Procedures

### Mode A: Full Orchestrated Release (Recommended)

Run the release script specifying the desired version and publication mode:

```powershell
# 1. Simulación sin subir a GitHub (dry-run):
powershell -ExecutionPolicy Bypass -File scripts\publish_release.ps1 -Version v0.1.0 -Mode dry-run

# 2. Publicación mediante Git Tag + GitHub Actions:
powershell -ExecutionPolicy Bypass -File scripts\publish_release.ps1 -Version v0.1.0 -Mode github-actions

# 3. Publicación directa usando GitHub CLI (gh):
powershell -ExecutionPolicy Bypass -File scripts\publish_release.ps1 -Version v0.1.0 -Mode gh-cli
```

---

### Mode B: Step-by-Step Manual Workflow

If executing step-by-step:

#### Step 1: Pre-flight Verification
Ensure the working tree is clean and tests pass:
```powershell
git status --short
ctest --preset release --output-on-failure
```

#### Step 2: Package Release and Compute Checksums
```powershell
powershell -ExecutionPolicy Bypass -File scripts\package_release.ps1 -Version v0.1.0
```
This produces:
- `dist/pluma-v0.1.0-windows-x64.zip`
- `dist/SHA256SUMS.txt`

#### Step 3: Generate Categorized Release Notes
```powershell
powershell -ExecutionPolicy Bypass -File scripts\generate_release_notes.ps1 -Version v0.1.0
```
This produces `dist/RELEASE_NOTES.md` automatically grouping commits into:
- 🚀 Features (`feat:`)
- 🐛 Bug Fixes (`fix:`)
- ⚡ Performance (`perf:`)
- 📝 Documentation (`docs:`)
- 🔧 Chores & Maintenance (`chore:`)
- 🔐 SHA-256 Checksums table
- 💡 Windows unblock instructions for end-users

#### Step 4: Publish to GitHub

**Via Git Tag (triggers `.github/workflows/release.yml`):**
```powershell
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

**Via GitHub CLI (`gh`):**
```powershell
gh release create v0.1.0 dist\pluma-v0.1.0-windows-x64.zip dist\SHA256SUMS.txt --title "Pluma v0.1.0" --notes-file dist\RELEASE_NOTES.md
```

---

## Post-Release Verification

1. Verify release page: `https://github.com/mbridge1eafit/Pluma-Editor/releases`
2. If using GitHub Actions, monitor the run at: `https://github.com/mbridge1eafit/Pluma-Editor/actions`
3. Verify that both the `.zip` and `SHA256SUMS.txt` assets are downloadable and hashes match.
