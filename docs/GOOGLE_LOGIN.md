# Google / YouTube sign-in in YouTube Max’d 1.2.0

The embedded YouTube.js TV flow requires **no developer or user Google Cloud
project**. It has no Cloud hosting bill or shared public Data API quota;
YouTube's own service limits still apply.

1. Open **Sign in to YouTube** from the menu or Accounts.
2. The app shows an activation address, code and QR code.
3. Open that address on your phone/computer, check that it is Google's real
   sign-in page, select the intended account and approve access.
4. The app polls at the requested interval and adds the account as a profile
   alongside Guest. The password is entered on Google's website.

No companion computer, developer OAuth JSON or copied browser cookie is
required. Old prototype Data API tokens are not imported; sign in again.

Tokens live in `/data/youtube-maxd/accounts/oauth-profiles.json`, in plain text
with restricted file modes, without hardware-backed encryption. Keep account
files out of shared archives and bug reports. Releases contain no credentials.

**Remove selected account** deletes the local profile and retires its sessions.
It does not revoke Google authorization. Also remove the relevant **YouTube
on TV** connection at <https://myaccount.google.com/permissions> to revoke it.
If the authorization expires or is revoked, use a new activation code.

Broader account feeds/actions still need acceptance on the target console.
Writes retain upstream's explicit confirmation flow. Account-required video
playback is off by default; signing in does not promise private, age-restricted,
paid or members-only playback. Internal endpoints can change or refuse a client.
