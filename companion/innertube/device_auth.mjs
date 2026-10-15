// Bounded OAuth operations using pinned YouTube.js; no automatic polling timer.
// SPDX-License-Identifier: GPL-3.0-or-later
import { LibraryError } from './library.mjs';

const scalar = (value, max) => typeof value === 'string' && value.length > 0 &&
  value.length <= max && !/[^\x21-\x7e]/.test(value);
export function validCredentials(value) {
  return value && typeof value === 'object' && !Array.isArray(value) &&
    Object.keys(value).sort().join(',') === 'access_token,client,expiry_date,refresh_token' &&
    scalar(value.access_token, 8192) && scalar(value.refresh_token, 8192) &&
    typeof value.expiry_date === 'string' && value.expiry_date.length <= 40 &&
    Number.isFinite(Date.parse(value.expiry_date)) && validClient(value.client);
}
function validClient(client) {
  return client && typeof client === 'object' && !Array.isArray(client) &&
    Object.keys(client).sort().join(',') === 'client_id,client_secret' &&
    scalar(client.client_id, 512) && scalar(client.client_secret, 512);
}
export async function createProfileClient(profile, Innertube, fetch, onRefresh = () => {}, locale = null) {
  if (profile.oauth && (!validCredentials(profile.oauth) || profile.cookie || profile.account_index || profile.channel))
    throw new LibraryError('invalid_profiles', 'The OAuth profile is invalid.');
  const client = await Innertube.create({ cookie: profile.cookie || undefined,
    client_type: profile.oauth ? 'TVHTML5' : 'WEB',
    account_index: profile.account_index ?? 0, on_behalf_of_user: profile.channel || undefined,
    retrieve_player: false, retrieve_innertube_config: !profile.oauth, generate_session_locally: true, fetch,
    ...(locale?.lang ? { lang: locale.lang } : {}), ...(locale?.location ? { location: locale.location } : {}) });
  if (profile.oauth) {
    client.session.on('update-credentials', ({ credentials }) => {
      if (!validCredentials(credentials)) throw new LibraryError('auth_response', 'The refreshed credentials are invalid.', 502);
      onRefresh(profile.id, structuredClone(credentials));
    });
    await client.session.signIn(structuredClone(profile.oauth));
  }
  return client;
}
export class DeviceAuthorization {
  constructor(createClient, fetch, clock = () => Date.now()) {
    this.createClient = createClient; this.fetch = fetch; this.clock = clock;
  }
  async execute(input) {
    if (input.action === 'start') {
      const client = await this.createClient();
      const oauth = client.session.oauth;
      const identity = await oauth.getClientID();
      if (!validClient(identity)) throw new LibraryError('auth_response', 'The sign-in response is invalid.', 502);
      oauth.client_id = identity;
      const response = await oauth.getDeviceAndUserCode();
      return { client: identity, device_code: response.device_code, user_code: response.user_code,
        verification_url: response.verification_url, expires_in: response.expires_in, interval: response.interval };
    }
    if (input.action !== 'poll' || !validClient(input.client) || !scalar(input.device_code, 8192))
      throw new LibraryError('auth_request', 'The sign-in request is invalid.');
    const response = await this.fetch('https://www.youtube.com/o/oauth2/token', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ...input.client, code: input.device_code,
        grant_type: 'http://oauth.net/grant_type/device/1.0' }) });
    let data;
    try { data = await response.json(); }
    catch { throw new LibraryError('auth_response', 'The sign-in response is invalid.', 502); }
    if (data.error === 'authorization_pending') return { state: 'pending', slow_down: false };
    if (data.error === 'slow_down') return { state: 'pending', slow_down: true };
    if (data.error === 'access_denied') return { state: 'denied' };
    if (data.error === 'expired_token') return { state: 'expired' };
    if (!response.ok || data.error || !Number.isInteger(data.expires_in) || data.expires_in < 1 || data.expires_in > 86400)
      throw new LibraryError('auth_response', 'YouTube could not complete sign-in.', 502);
    const credentials = { access_token: data.access_token, refresh_token: data.refresh_token,
      expiry_date: new Date(this.clock() + data.expires_in * 1000).toISOString(), client: input.client };
    if (!validCredentials(credentials)) throw new LibraryError('auth_response', 'The sign-in credentials are invalid.', 502);
    // A display-name lookup must never turn valid redeemed credentials into a failed sign-in.
    let timer, name;
    try {
      name = await Promise.race([(async () => {
        const client = await this.createClient();
        await client.session.signIn(credentials);
        const entries = (await client.account.getInfo()).contents?.contents ?? [];
        const selected = entries.find(entry => entry.is_selected) ?? (entries.length === 1 ? entries[0] : null);
        const label = selected?.account_name?.toString().trim();
        return typeof label === 'string' && Array.from(label).length >= 1 && Array.from(label).length <= 150 &&
          !/[\x00-\x1f\x7f]/.test(label) ? label : undefined;
      })(), new Promise(resolve => { timer = setTimeout(resolve, 5000); })]);
    } catch { /* Keep the caller's label if account information is unavailable. */ }
    finally { clearTimeout(timer); }
    return { state: 'authorized', credentials, ...(name ? { name } : {}) };
  }
}
