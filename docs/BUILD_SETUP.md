# Build setup

The simplest setup uses **one GitHub token and one Dropbox shared-link secret**. You do not need a Dropbox API app or refresh token when using a shared link.

## 1. Create the private-asset checkout token

Open [GitHub fine-grained token creation](https://github.com/settings/personal-access-tokens/new).

Set:
- **Token name:** `Gex private assets read`
- **Resource owner:** `NVDEMU`
- **Repository access:** `Only select repositories` → `gexprivateassets`
- **Repository permissions → Contents:** `Read-only`
- Leave other permissions at their default; GitHub automatically includes Metadata read-only.

Generate the token and copy it immediately. In [the public repo's Actions secrets](https://github.com/NVDEMU/gexenterthegeckorecomp/settings/secrets/actions/new), create a secret named exactly `GEX_ASSETS_TOKEN` and paste the token as its value.

This token only needs to read `NVDEMU/gexprivateassets`. Do not commit it to a file, workflow, README, issue, or chat. GitHub only shows the token value once.

## 2. Easiest Dropbox setup: a shared-link secret (no refresh token)

This is easiest, but note the trade-off: **anyone who obtains the Dropbox share URL can download the BIN**. The URL will be stored as a GitHub Actions secret and must not be posted publicly.

1. In Dropbox, find `/Gex - Enter the Gecko.bin`.
2. Click **Share → Create link** (or **Copy link** if one already exists). Set link access to **Anyone with the link** and permission to **Can view** — do not enable editing.
3. Copy the full URL, including any `rlkey` query parameter.
4. In [the public repo's Actions secrets](https://github.com/NVDEMU/gexenterthegeckorecomp/settings/secrets/actions/new), add a secret named exactly `DROPBOX_SHARED_URL` and paste the full link.

The workflow converts the link to a direct download, checks the file size and content hash against the private manifest, and removes the downloaded BIN before packaging. Never place this URL into the public workflow or README. You can revoke it from Dropbox by deleting the shared link.

## 3. More private alternative: Dropbox OAuth

Use this only if you don't want an “anyone with the link” URL.

1. Create an app in the [Dropbox App Console](https://www.dropbox.com/developers/apps) with **Scoped access** and the **files.content.read** permission; submit the permission change.
2. Configure a redirect URI you control for the authorization-code flow. Use the app key and request offline access with `token_access_type=offline`.
3. Authorize the app and exchange the returned authorization code at Dropbox's `/oauth2/token` endpoint using the app key, app secret and matching redirect URI. The response includes a `refresh_token` when offline access was requested. Follow Dropbox's [official offline-access guide](https://dropbox.tech/developers/using-oauth-2-0-with-offline-access).
4. Add these three secrets to the public repository: `DROPBOX_APP_KEY`, `DROPBOX_APP_SECRET`, and `DROPBOX_REFRESH_TOKEN`.

When `DROPBOX_SHARED_URL` is set, the workflow uses it and does not require the three OAuth secrets. If it is absent, the workflow requires all three OAuth secrets.

## 4. Run the build

After saving `GEX_ASSETS_TOKEN` and either `DROPBOX_SHARED_URL` or the three OAuth secrets, open [Actions → Gex Nightly](https://github.com/NVDEMU/gexenterthegeckorecomp/actions) and choose **Run workflow**.

A download error usually means the Dropbox share link was copied incorrectly, expired/revoked, or the app permissions are wrong. A generation/build error is a separate PSXRecomp or game-disc issue. A successful compilation still needs actual gameplay testing.
