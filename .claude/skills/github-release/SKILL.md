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

## Mandatory Rule: Release Notes Are Always Published

**The body of every GitHub Release must be the notes produced by `scripts/generate_release_notes.ps1`.**
Never leave GitHub's auto-generated notes (`generate_release_notes: true`) or an empty body, and never leave the generated notes only in the local `dist/` folder.

- **GitHub Actions mode:** `.github/workflows/release.yml` generates `dist/RELEASE_NOTES.md` after packaging (so it embeds the checksums of the published artifacts), publishes it with `body_path`, and fails the run if the published body does not contain it.
- **GitHub CLI mode:** `gh release create ... --notes-file dist/RELEASE_NOTES.md`.
- **Both modes:** `scripts/publish_release.ps1` verifies the published body at the end; a release is not finished until that check passes (see *Post-Release Verification*).
- The checksums in the notes must be those of the **published** assets. Artifacts built in CI differ from local ones, so never publish locally generated notes on a CI-built release. To fix or regenerate the notes of an existing release, first download its `SHA256SUMS.txt`:

```powershell
gh release download vX.Y.Z -p SHA256SUMS.txt -D dist --clobber
pwsh -NoProfile -File scripts\generate_release_notes.ps1 -Version vX.Y.Z -ToCommit vX.Y.Z
gh release edit vX.Y.Z --notes-file dist\RELEASE_NOTES.md
```

`generate_release_notes.ps1` lists the commits since the **previous** version tag: it skips the tag being released even when it already exists and points to `HEAD`, which is always the case in CI.

## Quick Summary

The project provides three automated scripts in `scripts/`:
1. `scripts/package_release.ps1`: Builds the portable ZIP and computes `SHA256SUMS.txt`.
2. `scripts/generate_release_notes.ps1`: Parses Conventional Commits into categorized Markdown release notes (UTF-8 without BOM, published as-is).
3. `scripts/publish_release.ps1`: Complete orchestrator combining pre-flight checks, building, testing, packaging, release notes, publication and verification of the published notes.

Run the scripts with PowerShell 7 (`pwsh`). Launching Windows PowerShell 5.1 (`powershell.exe`) from a PowerShell 7 console inherits a `PSModulePath` that breaks `Get-FileHash`. Build steps need the MSVC environment (`vcvars64.bat`) loaded.

---

## Release Procedures

### Before Publishing

1. Choose the version with SemVer from the commits since the last tag (`feat` → minor, only `fix` → patch). Tags that already exist (`git ls-remote --tags origin`) cannot be reused.
2. Bump the version in `CMakeLists.txt` (`project(... VERSION)`), `res/pluma.rc` (`FILEVERSION`, `PRODUCTVERSION`, `FileVersion`, `ProductVersion`) and `res/pluma.manifest`, and commit it (`chore(release): bump version to X.Y.Z`).
3. Everything to release must be committed and pushed to `master`: the release is built from the tagged commit.
4. Publishing is outward-facing: confirm the version and mode with the user before pushing a tag or creating a release.

### Mode A: Full Orchestrated Release (Recommended)

Run the release script specifying the desired version and publication mode:

```powershell
# 1. Simulación sin subir a GitHub (dry-run):
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\publish_release.ps1 -Version vX.Y.Z -Mode dry-run

# 2. Publicación mediante Git Tag + GitHub Actions (espera al workflow y verifica las notas publicadas):
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\publish_release.ps1 -Version vX.Y.Z -Mode github-actions

# 3. Publicación directa usando GitHub CLI (gh) (publica dist\RELEASE_NOTES.md y lo verifica):
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\publish_release.ps1 -Version vX.Y.Z -Mode gh-cli
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
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\package_release.ps1 -Version vX.Y.Z
```
This produces:
- `dist/pluma-vX.Y.Z-windows-x64.zip`
- `dist/SHA256SUMS.txt`

#### Step 3: Generate Categorized Release Notes
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\generate_release_notes.ps1 -Version vX.Y.Z
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

**Via Git Tag (triggers `.github/workflows/release.yml`, which regenerates and publishes the notes in CI):**
```powershell
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin vX.Y.Z
```

**Via GitHub CLI (`gh`):**
```powershell
gh release create vX.Y.Z dist\pluma-vX.Y.Z-windows-x64.zip dist\SHA256SUMS.txt --title "Pluma vX.Y.Z" --notes-file dist\RELEASE_NOTES.md
```

---

## Post-Release Verification

A release is complete only when all of these pass:

1. The workflow finished successfully (GitHub Actions mode): `gh run list --workflow release.yml --limit 1` / `https://github.com/mbridge1eafit/Pluma-Editor/actions`.
2. **The release body is the generated notes**, with the checksum of the published ZIP:
   ```powershell
   gh release view vX.Y.Z --json body --jq .body
   ```
   It must start with `# Pluma vX.Y.Z` and contain the hash from the published `SHA256SUMS.txt`. If not, fix it with the commands in *Mandatory Rule* above.
3. Both the `.zip` and `SHA256SUMS.txt` assets are downloadable and the hashes match:
   ```powershell
   gh release download vX.Y.Z -p "*.zip" -D $env:TEMP --clobber
   (Get-FileHash "$env:TEMP\pluma-vX.Y.Z-windows-x64.zip" -Algorithm SHA256).Hash.ToLower()
   ```
4. The release page shows it as *Latest*: `https://github.com/mbridge1eafit/Pluma-Editor/releases`.
