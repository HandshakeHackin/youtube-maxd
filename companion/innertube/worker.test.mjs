// Original behavior/failure tests; no network or account use. GPL-3.0-or-later
import test from 'node:test';
import assert from 'node:assert/strict';
import { Library, normalizeVideo, imageUrl } from './library.mjs';
import { YTNodes, YT, Innertube, Log, Parser } from 'youtubei.js';
import { DeviceAuthorization, createProfileClient, validCredentials } from './device_auth.mjs';

Log.setLevel(Log.Level.NONE);
const chatMessage = (id, author = 'author-private-id', message = 'Chat <b>literal</b>') => ({
  liveChatTextMessageRenderer: { id, authorName: {simpleText:'Viewer'}, authorExternalChannelId: author,
    authorPhoto: {thumbnails:[{url:'https://yt3.ggpht.com/avatar',width:32,height:32}]},
    message: {runs:[{text:message}]}, timestampUsec:'123000000', timestampText:{simpleText:'now'} }
});
const chatResponse = (actions, token = 'private-chat-next', extra = {}) => ({ continuationContents: {
  liveChatContinuation: { actions, continuations:[{timedContinuationData:{continuation:token,timeoutMs:2000}}], ...extra }
}});

test('chat snapshots are bounded and retain paid-message text without private menu endpoints', async () => {
  let batch = Array.from({length:300}, (_,i) => ({addChatItemAction:{item:chatMessage(String(i))}}));
  batch.push({addChatItemAction:{item:{liveChatPaidMessageRenderer:{id:'paid',authorName:{simpleText:'Supporter'},
    authorPhoto:{thumbnails:[]},authorExternalChannelId:'private-author',message:{simpleText:'Thanks <script>literal</script>'},
    purchaseAmountText:{simpleText:'$5.00'},timestampUsec:'1000000',contextMenuEndpoint:{},
    contextMenuAccessibility:{accessibilityData:{label:'private-menu'}}}}}});
  const actions = {async execute() {return Parser.parseResponse(chatResponse(batch));}};
  const client = {async getInfo(id) {return {basic_info:{id},livechat:{continuation:'initial'},actions};}};
  const library = new Library(async () => client);
  const result = await library.execute({action:'live_chat',id:'abcdefghijk'});
  assert.equal(result.items.length,24);
  assert.equal(Array.from(library.chats.contexts)[0].messages.size,256);
  assert.equal(result.items.at(-1).amount,'$5.00');
  assert.equal(result.items.at(-1).content,'Thanks <script>literal</script>');
  assert.equal(JSON.stringify(result).includes('private'),false);
  await library.execute({action:'chat_close',cursor:result.cursor});
});

test('pinned live chat transport parses text, replacements and author moderation with private cursors', async () => {
  let now = 0; const requests = [];
  const responses = [chatResponse([{addChatItemAction:{item:chatMessage('one')}}]),
    chatResponse([{replaceChatItemAction:{targetItemId:'one',replacementItem:chatMessage('two')}},
      {addChatItemAction:{item:chatMessage('other','different-author')}}]),
    chatResponse([{markChatItemsByAuthorAsDeletedAction:{externalChannelId:'author-private-id',deletedStateMessage:{simpleText:'Removed'}}}])];
  const client = await Innertube.create({retrieve_player:false,retrieve_innertube_config:false,generate_session_locally:true,
    fetch:async (input, init) => { const request = new Request(input,init); requests.push(request);
      assert.equal(new URL(request.url).pathname, '/youtubei/v1/live_chat/get_live_chat');
      const body = await request.clone().json(); assert.ok(body.continuation.startsWith('private-chat-'));
      return new Response(JSON.stringify(responses.shift()), {headers:{'Content-Type':'application/json'}}); }});
  client.getInfo = async id => ({basic_info:{id},livechat:{continuation:'private-chat-initial'},actions:client.actions});
  const library = new Library(async () => client, [{id:'two',cookie:'synthetic'}], () => now);
  const first = await library.execute({action:'live_chat',id:'abcdefghijk'});
  assert.equal(first.items[0].content, 'Chat <b>literal</b>'); assert.equal(first.items[0].art_url, 'https://yt3.ggpht.com/avatar');
  assert.equal(JSON.stringify(first).includes('private'), false);
  assert.equal(first.items[0].author_id, undefined);
  const early = await library.execute({action:'chat_poll',cursor:first.cursor});
  assert.equal(early.cursor, first.cursor); assert.equal(requests.length, 1);
  await assert.rejects(library.execute({action:'chat_poll',cursor:first.cursor,profile:'two'}), {code:'cursor_expired'});
  await assert.rejects(library.execute({action:'continue',cursor:first.cursor}), {code:'cursor_expired'});
  now = 2000;
  const second = await library.execute({action:'chat_poll',cursor:first.cursor});
  assert.deepEqual(second.items.map(i => i.id), ['two','other']);
  assert.deepEqual(await library.execute({action:'chat_poll',cursor:first.cursor}), second);
  assert.equal(requests.length, 2);
  now = 4000;
  const third = await library.execute({action:'chat_poll',cursor:second.cursor});
  assert.deepEqual(third.items.map(i => i.id), ['other']);
  await library.execute({action:'chat_close',cursor:first.cursor});
  await assert.rejects(library.execute({action:'chat_poll',cursor:third.cursor}), {code:'cursor_expired'});
  assert.equal(library.chats.contexts.size, 0);
});

test('chat mode selection, failures, capacity and expiry do not spawn background pollers', async () => {
  let now = 0, calls = 0, replay = false, oversized = false;
  const client = {async getInfo(id) { return {basic_info:{id},livechat:{continuation:'initial',is_replay:replay},actions:{
    async execute(path, options) {
      calls++; assert.equal(path,'live_chat/get_live_chat');
      if (oversized) return {continuation_contents:{type:'liveChatContinuation',actions:new Array(4097).fill({}),continuation:{token:'next'}}};
      const extra = options.continuation === 'initial' ? {header:{liveChatHeaderRenderer:{viewSelector:{sortFilterSubMenuRenderer:{
        subMenuItems:[{title:'Top chat',selected:true,continuation:{reloadContinuationData:{continuation:'top'}}},
          {title:'Live chat',selected:false,continuation:{reloadContinuationData:{continuation:'live'}}}] }}}}} : {};
      return Parser.parseResponse(chatResponse([{addChatItemAction:{item:chatMessage(options.continuation)}}], 'next', extra));
    }}}; }};
  const library = new Library(async () => client, [], () => now);
  const first = await library.execute({action:'live_chat',id:'abcdefghijk',chat_mode:'live'});
  assert.equal(first.items[0].id,'live'); assert.equal(calls,2); assert.equal(first.chat_mode,'live');
  now = 2000; oversized = true;
  await assert.rejects(library.execute({action:'chat_poll',cursor:first.cursor}), {code:'chat_response'});
  assert.equal(library.chats.cursors.get(first.cursor).context.messages.size,1);
  oversized = false; replay = true;
  await assert.rejects(library.execute({action:'live_chat',id:'abcdefghijk'}), {code:'chat_replay_pending'});
  replay = false;
  for (let i=1; i<16; i++) await library.execute({action:'live_chat',id:'abcdefghijk'});
  await assert.rejects(library.execute({action:'live_chat',id:'abcdefghijk'}), {code:'chat_busy'});
  now = 1802001;
  await assert.rejects(library.execute({action:'chat_poll',cursor:first.cursor}), {code:'cursor_expired'});
  assert.equal(library.chats.contexts.size,0);
});
const oauthFixture = { access_token: 'fixture-access', refresh_token: 'fixture-refresh',
  expiry_date: '2099-01-01T00:00:00.000Z', client: { client_id: 'fixture-client', client_secret: 'fixture-secret' } };

test('actual pinned comment votes use scoped TV endpoints and one-time references', async () => {
  let clock = 0;
  const requests = [];
  const profile = {id:'one', name:'TV', cookie:'', oauth:oauthFixture};
  const client = await createProfileClient(profile, Innertube, async (input, init) => {
    const request = new Request(input, init);
    const data = JSON.parse(await request.text());
    assert.equal(new URL(request.url).pathname, '/youtubei/v1/comment/perform_comment_action');
    assert.equal(request.headers.get('Authorization'), 'Bearer fixture-access');
    assert.equal(request.headers.get('Cookie'), null);
    assert.equal(data.context.client.clientName, 'TVHTML5');
    requests.push(data.actions);
    return new Response('{}', {status:200, headers:{'Content-Type':'application/json'}});
  });
  const comment = new YTNodes.CommentView({commentId:'fixture-comment',toolbarStateKey:'private-state-key'});
  comment.content = 'Comment <b>literal</b>'; comment.author = {name:'Author'};
  comment.like_command = new YTNodes.NavigationEndpoint({performCommentActionEndpoint:{actions:['private-like-action']}});
  comment.dislike_command = new YTNodes.NavigationEndpoint({performCommentActionEndpoint:{actions:['private-dislike-action']}});
  comment.unlike_command = new YTNodes.NavigationEndpoint({performCommentActionEndpoint:{actions:['private-unlike-action']}});
  comment.setActions(client.actions);
  client.getComments = async () => ({contents:[{comment}],has_continuation:false});
  const library = new Library(async () => client, [profile,{id:'two',name:'Peer',cookie:'peer-cookie'}], () => clock);
  let page = await library.execute({action:'comments',profile:'one',id:'00000000000'});
  const cursor = page.items[0].rating_cursor;
  assert.ok(cursor); assert.equal(page.items[0].rating_like, true); assert.equal(page.items[0].rating_remove, false);
  assert.equal(JSON.stringify(page).includes('private-'), false);
  await assert.rejects(library.execute({action:'comment_rate',profile:'guest',cursor,rating:'like'}), {code:'account_required'});
  await assert.rejects(library.execute({action:'comment_rate',profile:'two',cursor,rating:'like'}), {code:'cursor_expired'});
  await assert.rejects(library.execute({action:'comment_rate',profile:'one',cursor,rating:'none'}), {code:'invalid_rating'});
  const result = await library.execute({action:'comment_rate',profile:'one',cursor,rating:'like'});
  assert.deepEqual(result, {api_version:1,success:true,rating:'like'});
  assert.deepEqual(await library.execute({action:'comment_rate',profile:'one',cursor,rating:'like'}), result);
  await assert.rejects(library.execute({action:'comment_rate',profile:'one',cursor,rating:'dislike'}), {code:'comment_unconfirmed'});
  assert.deepEqual(requests, [['private-like-action']]);
  comment.is_liked = true;
  page = await library.execute({action:'comments',profile:'one',id:'00000000000'});
  assert.equal(page.items[0].rating_remove, true); assert.equal(page.items[0].rating_like, false);
  await library.execute({action:'comment_rate',profile:'one',cursor:page.items[0].rating_cursor,rating:'none'});
  assert.deepEqual(requests[1], ['private-unlike-action']);
  page = await library.execute({action:'comments',profile:'one',id:'00000000000'});
  clock = 120001;
  await assert.rejects(library.execute({action:'comment_rate',profile:'one',cursor:page.items[0].rating_cursor,rating:'dislike'}), {code:'cursor_expired'});
  assert.equal(requests.length, 2);
});

test('failed comment rating references cannot attempt another upstream mutation', async () => {
  let calls = 0;
  const comment = {comment_id:'failure',content:'Failure',like_command:{},dislike_command:{},
    like:async () => {calls++; throw new Error('private upstream error');}, dislike:async () => {calls++;}};
  const library = new Library(async () => ({session:{logged_in:true},getComments:async () => ({contents:[{comment}]})}),
    [{id:'one',name:'One',cookie:'cookie'}]);
  const page = await library.execute({action:'comments',profile:'one',id:'00000000000'});
  const request = {action:'comment_rate',profile:'one',cursor:page.items[0].rating_cursor,rating:'like'};
  await assert.rejects(library.execute(request));
  await assert.rejects(library.execute(request), {code:'comment_unconfirmed'});
  await assert.rejects(library.execute({...request,rating:'dislike'}), {code:'comment_unconfirmed'});
  assert.equal(calls, 1);
});
test('private playback preflight refreshes actual pinned TV tokens before expiry', async () => {
  const updates = [], requests = [];
  const nearExpiry = { ...oauthFixture, expiry_date: new Date(Date.now() + 30000).toISOString() };
  const library = new Library(profile => createProfileClient(profile, Innertube, async (input, init) => {
    const request = new Request(input, init); requests.push(request);
    assert.equal(new URL(request.url).pathname, '/o/oauth2/token');
    assert.equal(JSON.parse(await request.text()).grant_type, 'refresh_token');
    return new Response(JSON.stringify({access_token:'fresh-playback-access',expires_in:3600}),
      { headers: { 'Content-Type': 'application/json' } });
  }, (id, credentials) => updates.push({id,credentials})), [{id:'one',name:'TV',cookie:'',oauth:nearExpiry}]);
  const result = await library.playback({ action:'credentials',profile:'one' });
  assert.equal(result.access_token, 'fresh-playback-access');
  assert.deepEqual(Object.keys(result).sort(), ['access_token','expiry_date']);
  assert.equal(updates.length, 1); assert.equal(requests.length, 1);
  assert.equal(updates[0].credentials.refresh_token, oauthFixture.refresh_token);
  assert.equal((await library.playback({action:'credentials',profile:'one'})).access_token, result.access_token);
  assert.equal(requests.length, 1);
  await assert.rejects(library.playback({action:'credentials',profile:'guest'}), /OAuth account/);
  await assert.rejects(library.execute({action:'credentials',profile:'one'}), /Unsupported|supported|operation/i);
});
test('TV playback catalogs retain profile-scoped opaque continuations', async () => {
  const video = id => ({id,title:'TV video',author:{name:'Creator'},duration:{seconds:8}});
  let searches = 0, continuations = 0;
  const feed = {videos:[video('abcdefghijk')],has_continuation:true,
    async getContinuation() { continuations++; return {videos:[video('zyxwvutsrqp')],has_continuation:false}; }};
  const library = new Library(async () => ({async search(query) { searches++; assert.equal(query,'TV'); return feed; }}),
    [{id:'one',cookie:'',oauth:oauthFixture},{id:'two',cookie:'',oauth:oauthFixture}]);
  const first = await library.playback({action:'catalog',kind:'search',query:'TV',profile:'one'});
  assert.equal(first.items[0].id, 'abcdefghijk'); assert.equal(first.has_more, true);
  const second = await library.playback({action:'catalog',cursor:first.cursor,profile:'one'});
  assert.equal(second.items[0].id, 'zyxwvutsrqp'); assert.equal(second.has_more, false);
  await library.playback({action:'catalog',cursor:first.cursor,profile:'one'});
  assert.equal(searches, 1); assert.equal(continuations, 1);
  await assert.rejects(library.playback({action:'catalog',cursor:first.cursor,profile:'two'}), /Reload/);
});
test('OAuth profiles use actual pinned TV context and keep bearer auth off web clients', async () => {
  const requests = [];
  const client = await createProfileClient({ id: 'one', cookie: '', oauth: oauthFixture }, Innertube,
    async (input, init) => { const request = new Request(input, init); requests.push(request);
      return new Response('{}', { headers: { 'Content-Type': 'application/json' } }); });
  assert.equal(client.session.logged_in, true);
  await client.actions.execute('/browse', { browseId: 'FEwhat_to_watch' });
  assert.equal(requests.length, 1);
  assert.equal(requests[0].headers.get('Authorization'), 'Bearer fixture-access');
  const payload = await requests[0].json();
  assert.equal(payload.context.client.clientName, 'TVHTML5');
  assert.equal(oauthFixture.expiry_date, '2099-01-01T00:00:00.000Z');
  await assert.rejects(createProfileClient({ id: 'one', cookie: 'fixture-cookie', oauth: oauthFixture }, Innertube, null), { code: 'invalid_profiles' });
});
test('device authorization polls once, respects upstream states and bounds token response', async () => {
  let requests = 0, response = { error: 'authorization_pending' }, status = 400;
  const engine = new DeviceAuthorization(async () => ({ session: { oauth: {
    getClientID: async () => oauthFixture.client,
    getDeviceAndUserCode: async () => ({ device_code: 'fixture-device', user_code: 'ABCD-EFGH',
      verification_url: 'https://www.google.com/device', expires_in: 900, interval: 5 }) } } }),
    async (url, init) => { requests++; assert.equal(url, 'https://www.youtube.com/o/oauth2/token');
      const request = JSON.parse(init.body); assert.equal(request.code, 'fixture-device');
      return new Response(JSON.stringify(response), { status }); }, () => 1000);
  const started = await engine.execute({ action: 'start' });
  const poll = { action: 'poll', client: started.client, device_code: started.device_code };
  assert.equal(requests, 0);
  assert.deepEqual(await engine.execute(poll), { state: 'pending', slow_down: false });
  response = { error: 'slow_down' }; assert.deepEqual(await engine.execute(poll), { state: 'pending', slow_down: true });
  response = { error: 'access_denied' }; assert.deepEqual(await engine.execute(poll), { state: 'denied' });
  response = { error: 'expired_token' }; assert.deepEqual(await engine.execute(poll), { state: 'expired' });
  status = 200; response = { access_token: 'new-access', refresh_token: 'new-refresh', expires_in: 3600 };
  const done = await engine.execute(poll); assert(validCredentials(done.credentials));
  assert.equal(done.credentials.expiry_date, '1970-01-01T01:00:01.000Z');
  response = { ...response, expires_in: true };
  await assert.rejects(engine.execute(poll), { code: 'auth_response' });
  assert.equal(requests, 6);
});
test('OAuth refresh uses actual pinned token endpoint and retains the client privately', async () => {
  const updates = [], requests = [];
  const client = await createProfileClient({ id: 'one', cookie: '', oauth: { ...oauthFixture, expiry_date: '2000-01-01T00:00:00.000Z' } }, Innertube,
    async (input, init) => { const request = new Request(input, init); requests.push(request);
      return new Response(JSON.stringify({ access_token: 'refreshed-access', expires_in: 3600 }), { headers: { 'Content-Type': 'application/json' } }); },
    (profile, credentials) => updates.push({ profile, credentials }));
  assert.equal(requests.length, 1); assert.equal(new URL(requests[0].url).pathname, '/o/oauth2/token');
  assert.equal((await requests[0].json()).grant_type, 'refresh_token');
  assert.equal(updates.length, 1); assert.equal(updates[0].credentials.access_token, 'refreshed-access');
  assert.deepEqual(updates[0].credentials.client, oauthFixture.client);
  assert.equal(client.session.context.client.clientName, 'TVHTML5');
});

test('watch history uses actual pinned tracking methods and isolates playback sessions', async () => {
  const calls = [];
  const info = new YT.VideoInfo([{ data: { videoDetails: { videoId: 'abcdefghijk', title: 'Fixture' },
    playabilityStatus: { status: 'OK' }, playbackTracking: {
      videostatsPlaybackUrl: { baseUrl: 'https://s.youtube.com/api/stats/playback?fixture=1' },
      videostatsWatchtimeUrl: { baseUrl: 'https://s.youtube.com/api/stats/watchtime?fixture=1' }
    } } }], { stats: async (url, client, params) => { calls.push([url, params]); return new Response(null, { status: 204 }); } }, 'fixture-cpn');
  const library = new Library(async profile => ({ session: { logged_in: true }, getBasicInfo: async id => {
    calls.push([profile.id, id]); return info;
  } }), [{ id: 'one', cookie: 'fixture-cookie' }, { id: 'two', cookie: 'other-fixture' }]);
  const request = { action: 'watch_progress', profile: 'one', id: 'abcdefghijk', session: 'playback-one', position_sec: 1, state: 'playing' };
  const first = await library.execute(request);
  assert.equal(first.success, true); assert.equal(calls.length, 3);
  assert.deepEqual(calls[0], ['one', 'abcdefghijk']);
  assert.equal(calls[1][0], 'https://www.youtube.com/api/stats/playback?fixture=1');
  assert.equal(calls[1][1].cpn, 'fixture-cpn'); assert.equal(calls[2][1].cmt, '1.000');
  assert.deepEqual(await library.execute(request), first); assert.equal(calls.length, 3);
  await library.execute({ ...request, position_sec: 31 }); assert.equal(calls.length, 4);
  await library.execute({ ...request, position_sec: 8, state: 'stopped' }); assert.equal(calls.length, 5);
  await library.execute({ ...request, position_sec: 8, state: 'stopped' }); assert.equal(calls.length, 5);
  await assert.rejects(library.execute({ ...request, position_sec: 9 }), { code: 'session_closed' });
  await assert.rejects(library.execute({ ...request, id: '12345678901' }), { code: 'invalid_progress' });
  await library.execute({ ...request, profile: 'two' }); assert.equal(calls.length, 8);
  await assert.rejects(library.execute({ ...request, profile: 'guest' }), { code: 'account_required' });
});

test('unconfirmed history calls stop retries, validate reports and bound retained sessions', async () => {
  let time = 0, starts = 0, updates = 0, logged_in = true, status = 204;
  const library = new Library(async () => ({ session: { get logged_in() { return logged_in; } },
    getBasicInfo: async () => ({ basic_info: { id: 'abcdefghijk' }, addToWatchHistory: async () => { starts++; return new Response(null, { status }); },
      updateWatchTime: async () => { updates++; return new Response(null, { status: 503 }); } }) }),
  [{ id: 'one', cookie: 'fixture' }], () => time);
  const request = { action: 'watch_progress', profile: 'one', id: 'abcdefghijk', session: 'playback-one', position_sec: 1, state: 'playing' };
  logged_in = false;
  await assert.rejects(library.execute(request), { code: 'account_required' }); assert.equal(starts, 0);
  logged_in = true;
  for (const extra of [{ position_sec: NaN }, { position_sec: -1 }, { position_sec: Infinity }, { position_sec: 1000001 }, { state: 'invalid' }, { session: '' }, { session: 'bad/session' }])
    await assert.rejects(library.execute({ ...request, ...extra }), { code: 'invalid_progress' });
  await assert.rejects(library.execute(request), { code: 'history_unconfirmed' });
  await assert.rejects(library.execute(request), { code: 'history_unconfirmed' });
  assert.equal(starts, 1); assert.equal(updates, 1);
  status = 503;
  for (let i = 1; i < 64; i++) await assert.rejects(library.execute({ ...request, session: `playback-${i}` }), { code: 'history_unconfirmed' });
  assert.equal(library.watches.size, 64);
  await assert.rejects(library.execute({ ...request, session: 'overflow' }), { code: 'history_busy' });
  assert.equal(starts, 64); assert.equal(updates, 1);
  time = 1800001;
  await assert.rejects(library.execute({ ...request, session: 'fresh' }), { code: 'history_unconfirmed' });
  assert.equal(library.watches.size, 1);
  const wrong = new Library(async () => ({ session: { logged_in: true }, getBasicInfo: async () => ({ basic_info: { id: '12345678901' },
    addToWatchHistory: async () => { throw new Error('must not be called'); } }) }), [{ id: 'one', cookie: 'fixture' }]);
  await assert.rejects(wrong.execute(request), { code: 'history_unconfirmed' });
});

const video = index => ({ video_id: String(index).padStart(11, '0'), title: `Video ${index}`,
  author: { id: `UC${'a'.repeat(22)}`, name: 'Fixture channel' }, duration: { seconds: 8 },
  thumbnails: [{ url: 'https://i.ytimg.com/fixture.jpg' }] });
test('playlist creation explicitly requests private visibility and stable video identity', async () => {
  const calls = [];
  const client = { session: { logged_in: true }, actions: { execute: async (path, payload) => {
    calls.push([path, payload]); return { success: true, status_code: 200, data: { playlistId: 'PLabcdefghijk' } };
  } } };
  const library = new Library(async () => client, [{ id: 'one', cookie: 'fixture-cookie' }]);
  const result = await library.execute({ action: 'playlist_create', profile: 'one', title: ' New "playlist" ', id: 'abcdefghijk' });
  assert.equal(result.playlist_id, 'PLabcdefghijk');
  assert.deepEqual(calls[0], ['playlist/create', { title: 'New "playlist"', videoIds: ['abcdefghijk'], privacyStatus: 'PRIVATE' }]);
  const endpoint = new YTNodes.CreatePlaylistServiceEndpoint(calls[0][1]);
  assert.deepEqual(endpoint.buildRequest(), calls[0][1]);
});
test('playlist names preserve Unicode code points and reject invalid or oversized input', async () => {
  const calls = [];
  const library = new Library(async () => ({ session: { logged_in: true }, actions: { execute: async (_, payload) => {
    calls.push(payload); return { success: true, data: { playlistId: 'PLabcdefghijk' } };
  } }, playlist: { setName: async (id, title) => { calls.push([id, title]); return { success: true }; } } }),
  [{ id: 'one', cookie: 'fixture-cookie' }]);
  const title = '🌍'.repeat(150);
  await library.execute({ action: 'playlist_create', profile: 'one', title });
  assert.equal(calls[0].title, title); assert.deepEqual(calls[0].videoIds, []);
  await library.execute({ action: 'playlist_rename', profile: 'one', playlist: 'PLabcdefghijk', title });
  assert.deepEqual(calls[1], ['PLabcdefghijk', title]);
  for (const invalid of ['', ' ', 'x'.repeat(151), 'a\nb', 'a\0b'])
    await assert.rejects(library.execute({ action: 'playlist_create', profile: 'one', title: invalid }), { code: 'invalid_title' });
  assert.equal(calls.length, 2);
});
test('playlist creation requires a signed-in session and a confirmed new identity', async () => {
  let calls = 0, logged_in = false;
  const client = { session: { get logged_in() { return logged_in; } }, actions: { execute: async () => {
    calls++; return { success: true, data: {} };
  } } };
  const library = new Library(async () => client, [{ id: 'one', cookie: 'fixture-cookie' }]);
  await assert.rejects(library.execute({ action: 'playlist_create', profile: 'one', title: 'Name' }), { code: 'account_required' });
  assert.equal(calls, 0); logged_in = true;
  await assert.rejects(library.execute({ action: 'playlist_create', profile: 'one', title: 'Name' }), { code: 'operation_failed' });
  assert.equal(calls, 1);
});
test('pagination retains every item and scopes cursors to the profile', async () => {
  const second = { videos: [video(53)], has_continuation: false };
  const feed = { videos: Array.from({ length: 53 }, (_, i) => video(i)), has_continuation: true,
    getContinuation: async () => second };
  const library = new Library(async () => ({ getHomeFeed: async () => feed }), [{ id: 'other', name: 'Other', cookie: '' }]);
  const seen = [];
  let page = await library.execute({ action: 'home' });
  await assert.rejects(library.execute({ action: 'continue', profile: 'other', cursor: page.cursor }), { code: 'cursor_expired' });
  while (true) {
    seen.push(...page.items.map(item => item.id));
    if (!page.has_more) break;
    page = await library.execute({ action: 'continue', cursor: page.cursor });
  }
  assert.deepEqual(seen, Array.from({ length: 54 }, (_, i) => String(i).padStart(11, '0')));
});
test('expired pagination rejects without upstream calls', async () => {
  let time = 0, calls = 0;
  const library = new Library(async () => ({ getHomeFeed: async () => ({ videos: [video(1)], has_continuation: true,
    getContinuation: async () => { calls++; return {}; } }) }), [], () => time);
  const page = await library.execute({ action: 'home' }); time = 120001;
  await assert.rejects(library.execute({ action: 'continue', cursor: page.cursor }), { code: 'cursor_expired' });
  assert.equal(calls, 0);
});
test('a continuation retry returns the same page without an extra fetch', async () => {
  let calls = 0;
  const library = new Library(async () => ({ getHomeFeed: async () => ({ videos: [video(0)], has_continuation: true,
    getContinuation: async () => { calls++; return { videos: [video(1)], has_continuation: false }; } }) }));
  const first = await library.execute({ action: 'home' });
  const one = await library.execute({ action: 'continue', cursor: first.cursor });
  const two = await library.execute({ action: 'continue', cursor: first.cursor });
  assert.deepEqual(one, two); assert.equal(calls, 1);
});
test('account-only operations reject guests before opening a client', async () => {
  let calls = 0;
  const library = new Library(async () => { calls++; return {}; });
  await assert.rejects(library.execute({ action: 'subscriptions' }), { code: 'account_required' });
  await assert.rejects(library.execute({ action: 'like', id: '00000000000' }), { code: 'account_required' });
  await assert.rejects(library.execute({ action: 'invented' }), { code: 'unknown_operation' });
  assert.equal(calls, 0);
});
test('profiles never expose credentials and clients remain separate', async () => {
  const used = [];
  const library = new Library(async profile => { used.push(profile.id); return { getHomeFeed: async () => ({ videos: [] }) }; },
    [{ id: 'one', name: 'One', cookie: 'secret-one' }, { id: 'two', name: 'Two', cookie: 'secret-two' }]);
  const profiles = await library.execute({ action: 'profiles' });
  assert.equal(JSON.stringify(profiles).includes('secret'), false);
  await library.execute({ action: 'home', profile: 'one' });
  await library.execute({ action: 'home', profile: 'two' });
  await library.execute({ action: 'home', profile: 'one' });
  assert.deepEqual(used, ['one', 'two']);
});
test('mutations use the requested stable IDs and require a confirmed response', async () => {
  const calls = [];
  const client = { interact: { subscribe: async id => { calls.push(id); return { success: false }; } },
    playlist: { addVideos: async (id, videos) => { calls.push([id, videos]); return { action_result: [] }; } } };
  const library = new Library(async () => client, [{ id: 'one', name: 'One', cookie: 'cookie' }]);
  await assert.rejects(library.execute({ action: 'subscribe', profile: 'one', id: `UC${'a'.repeat(22)}` }), { code: 'operation_failed' });
  await assert.rejects(library.execute({ action: 'watch_later_add', profile: 'one', id: '00000000000' }), { code: 'operation_failed' });
  assert.deepEqual(calls, [`UC${'a'.repeat(22)}`, ['WL', ['00000000000']]]);
  await assert.rejects(library.execute({ action: 'like', profile: 'one', id: 'wrong' }), { code: 'invalid_identity' });
});
test('comment normalization returns display data and continuation', async () => {
  const library = new Library(async () => ({ getComments: async () => ({ contents: [{ comment: {
    comment_id: 'comment-1', content: { toString: () => 'Text café' }, author: { name: 'Fixture' },
    keys: { shared: 'private-implementation-token' }, is_pinned: true
  } }], has_continuation: false }) }));
  const page = await library.execute({ action: 'comments', id: '00000000000' });
  assert.equal(page.items[0].content, 'Text café'); assert.equal(page.items[0].pinned, true);
  assert.equal(JSON.stringify(page).includes('implementation-token'), false);
});
test('normalization rejects arbitrary origins and identities', () => {
  assert.equal(imageUrl('http://127.0.0.1/image'), '');
  assert.equal(imageUrl('https://i.ytimg.com.attacker.invalid/image'), '');
  assert.equal(imageUrl('https://user:password@i.ytimg.com/image'), '');
  assert.equal(normalizeVideo({ video_id: '/data/file' }), null);
  assert.equal(normalizeVideo(video(1)).duration_sec, 8);
});
test('account playlists retain a separate navigation namespace', async () => {
  const library = new Library(async () => ({ getPlaylists: async () => ({ playlists: [{ id: 'PLabcdefghijk', title: 'Library' }], has_continuation: false }) }),
    [{ id: 'one', name: 'One', cookie: 'cookie' }]);
  const page = await library.execute({ action: 'playlists', profile: 'one' });
  assert.equal(page.items[0].id, 'library:playlist:PLabcdefghijk');
  assert.equal(page.items[0].is_folder, true);
});

function lockup(kind = 'VIDEO', duration = '1:02:03') {
  const thumbnail = { thumbnailViewModel: { image: { sources: [{ url: 'https://i.ytimg.com/modern.jpg', width: 640, height: 360 }] },
    overlays: [{ thumbnailOverlayBadgeViewModel: { thumbnailBadges: [{ thumbnailBadgeViewModel: { text: duration, badgeStyle: 'THUMBNAIL_BADGE_STYLE_DEFAULT' } }] } }] } };
  return new YTNodes.LockupView({ contentId: kind === 'VIDEO' ? '00000000000' : 'PLabcdefghijk',
    contentType: `LOCKUP_CONTENT_TYPE_${kind}`,
    contentImage: kind === 'VIDEO' ? thumbnail : { collectionThumbnailViewModel: { primaryThumbnail: thumbnail } },
    metadata: { lockupMetadataViewModel: { title: { content: 'Modern café card' }, metadata: { contentMetadataViewModel: {
      metadataRows: [{ metadataParts: [{ text: { content: 'Fixture channel', commandRuns: [{ startIndex: 0, length: 15,
        onTap: { browseEndpoint: { browseId: `UC${'a'.repeat(22)}` } } }] } }] }], delimiter: ' · '
    } } } }
  });
}

test('actual pinned modern video parser retains title, channel, duration and artwork', () => {
  const item = normalizeVideo(lockup());
  assert.equal(item.title, 'Modern café card');
  assert.equal(item.subtitle, 'Fixture channel');
  assert.equal(item.channel_id, `UC${'a'.repeat(22)}`);
  assert.equal(item.duration_sec, 3723);
  assert.equal(item.art_url, 'https://i.ytimg.com/modern.jpg');
  assert.equal(normalizeVideo(lockup('PLAYLIST')), null);
  assert.equal(normalizeVideo(lockup('VIDEO', '1:99')).duration_sec, 0);
});

test('actual pinned collection parser retains account playlist title and artwork', async () => {
  const library = new Library(async () => ({ getPlaylists: async () => ({ playlists: [lockup('PLAYLIST')], has_continuation: false }) }),
    [{ id: 'one', name: 'One', cookie: 'cookie' }]);
  const page = await library.execute({ action: 'playlists', profile: 'one' });
  assert.equal(page.items[0].title, 'Modern café card');
  assert.equal(page.items[0].art_url, 'https://i.ytimg.com/modern.jpg');
  assert.equal(page.items[0].id, 'library:playlist:PLabcdefghijk');
});

test('explicit promoted records are excluded without matching localized title text', () => {
  assert.equal(normalizeVideo({ ...video(1), is_ad: true }), null);
  assert.equal(normalizeVideo({ ...video(1), is_promoted: true }), null);
  assert.equal(normalizeVideo({ ...video(1), title: 'Advertising explained' }).title, 'Advertising explained');
});

test('reply browsing retains all pages, scopes references and caches retries', async () => {
  let calls = 0;
  const reply = index => ({ comment: { comment_id: `reply-${index}`, content: `Reply ${index}` } });
  const replyFeed = { replies: Array.from({ length: 30 }, (_, i) => reply(i)), has_continuation: true,
    getContinuation: async () => ({ replies: [reply(30)], has_continuation: false }) };
  const thread = { comment: { comment_id: 'parent', content: 'Parent', keys: { shared: 'never-export' } },
    has_replies: true, getReplies: async () => { calls++; return replyFeed; } };
  const library = new Library(async () => ({ getComments: async () => ({ contents: [thread], has_continuation: false }) }),
    [{ id: 'other', name: 'Other', cookie: '' }]);
  const first = await library.execute({ action: 'comments', id: '00000000000' });
  const cursor = first.items[0].reply_cursor;
  assert.ok(cursor); assert.equal(JSON.stringify(first).includes('never-export'), false);
  await assert.rejects(library.execute({ action: 'replies', profile: 'other', cursor }), { code: 'cursor_expired' });
  const start = await library.execute({ action: 'replies', cursor });
  assert.deepEqual(await library.execute({ action: 'replies', cursor }), start);
  const seen = [...start.items.map(item => item.id)];
  let page = start;
  while (page.has_more) {
    page = await library.execute({ action: 'continue', cursor: page.cursor });
    seen.push(...page.items.map(item => item.id));
  }
  assert.equal(calls, 1); assert.deepEqual(seen, Array.from({ length: 31 }, (_, i) => `reply-${i}`));
});

test('expired or evicted reply references never call the upstream thread', async () => {
  let time = 0, calls = 0;
  const make = index => ({ comment: { comment_id: `parent-${index}`, content: 'Parent' }, has_replies: true,
    getReplies: async () => { calls++; return { replies: [], has_continuation: false }; } });
  const library = new Library(async () => ({ getComments: async () => ({ contents: Array.from({ length: 24 }, (_, i) => make(i)), has_continuation: false }) }), [], () => time);
  const first = await library.execute({ action: 'comments', id: '00000000000' });
  for (let i = 0; i < 11; i++) await library.execute({ action: 'comments', id: '00000000000' });
  assert.equal(library.threads.size, 256);
  await assert.rejects(library.execute({ action: 'replies', cursor: first.items[0].reply_cursor }), { code: 'cursor_expired' });
  const newest = await library.execute({ action: 'comments', id: '00000000000' }); time = 120001;
  await assert.rejects(library.execute({ action: 'replies', cursor: newest.items[0].reply_cursor }), { code: 'cursor_expired' });
  assert.equal(calls, 0);
});
