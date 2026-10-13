// Signed-in playback. OFF unless the host sets `account_playback`; health
// says which. SPDX-License-Identifier: GPL-3.0-or-later
//
// Every profile resolves videos the way Guest does: the `VISIONOS` client
// without credentials, whose links need no signature and were measured live.
// An account's own session is the TV client (the only one the device-code
// sign-in authorizes). Its player answers carry signature ciphers that need
// YouTube's player script, which this engine refuses by default (ten seconds
// and hundreds of megabytes), and the TV script's challenges are the part the
// PC companion never proved either. So the account session is asked only
// here, only for a video the Guest path answered "sign in" for (age-restricted,
// private, members-only, purchased), and only when the switch is on. Nothing
// of this has run against a real account.
//
// The bearer token stays inside the account session's API requests; media,
// caption and storyboard addresses are read without it.
import { config, libraryExecute, mediaFetch, prepare, rawPlayerFacts } from './core.js';
import { ServiceError, resolverError } from './errors.js';
import { extract } from './resolve.js';

export const accountPlayback = () => config.account_playback === true;
export const needsAccount = (profile, error) => accountPlayback() && profile !== 'guest' && error instanceof ServiceError && error.code === 'authentication_required';

export async function accountExtract(profile, id) {
  const yt = await libraryExecute({ profile }, 'client');
  if (yt.session?.logged_in !== true) throw resolverError('authentication_required');
  // With the player script allowed, ciphered links can be solved; without it
  // only an answer that already carries direct links or a playlist is usable.
  if (config.allow_player_script) await prepare(yt, 'TV');
  const source = await extract(yt, id, { order: ['TV'], fetch: mediaFetch, facts: rawPlayerFacts });
  if (!source.player && !source.hls.length && !source.list.some(format => format.url)) throw resolverError('extraction_failed');
  return { ...source, account: true };
}
