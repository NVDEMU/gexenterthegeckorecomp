# Build setup

The first workflow run needs one GitHub credential and Dropbox OAuth credentials. These are secrets, not files: do not commit them into either repository.

## 1. Give the public workflow read access to the private assets repo

Create a fine-grained GitHub personal access token that grants **read-only Contents access** to only `NVDEMU/gexprivateassets`. Add it as this secret in `NVDEMU/gexenterthegeckorecomp`:

- `GEX_ASSETS_TOKEN`

The token is used only by `actions/checkout` to read the private CUE sheet and asset manifest. The workflow runs on pushes to `main` and manual dispatch; it deliberately does not run on pull requests, so untrusted PR code cannot access the asset credentials.

## 2. Give Actions access to the Dropbox file

Create a Dropbox API app with the **files.content.read** scope and authorize it for the Dropbox account that contains `/Gex - Enter the Gecko.bin`. Use an OAuth refresh token (offline access), not a short-lived access token. Add these three secrets to the public repository:

- `DROPBOX_APP_KEY`
- `DROPBOX_APP_SECRET`
- `DROPBOX_REFRESH_TOKEN`

At build time the workflow exchanges the refresh token for a short-lived access token, downloads the BIN directly from Dropbox, verifies its expected 266,662,704-byte size and Dropbox content hash, and deletes the temporary access token from the step environment. It never prints the access token or refresh token.

## 3. Run a build

After saving all four secrets, go to **Actions → Gex Nightly → Run workflow**. A successful run should publish or update the `Nightly` release with one zip for Windows, one for macOS Apple Silicon, and one for macOS Intel.

A build failure at the download step usually means the Dropbox credentials/scope or source path need checking. A failure during disc probing or generation is a game-input/toolchain issue; include the failing log section when debugging. A successful native compile still needs actual game testing in each platform.
