// Original bounded WatchNext queue adapter. SPDX-License-Identifier: GPL-3.0-or-later
const videoId = value => typeof value === 'string' && /^[A-Za-z0-9_-]{11}$/.test(value);
const playlistId = value => typeof value === 'string' && /^(WL|LL|[A-Za-z0-9_-]{10,100})$/.test(value);
const index = value => Number.isInteger(value) && value >= 0 && value <= 100000;

export async function remotePlaylist(client, input, ErrorType) {
  const lookup = input.action === 'playback_at';
  const fail = () => { throw new ErrorType('remote_playlist_unavailable', 'YouTube did not return a usable playback queue.', 502); };
  if ((lookup ? input.id !== undefined && input.id !== '' : !videoId(input.id)) || !playlistId(input.playlist) || !index(input.index))
    throw new ErrorType('invalid_request', 'The playback queue request is invalid.');
  const response = await client.actions.execute('/next', { ...(lookup ? {} : {videoId: input.id}),
    playlistId: input.playlist, playlistIndex: input.index, racyCheckOk: true, contentCheckOk: true });
  if (!response?.success || !response.data || typeof response.data !== 'object') fail();
  // WEB embeds playlist.playlist; TV and continuation responses use a panel.
  // Inspect raw nodes to retain endpoint indices and duplicate occurrences.
  const pending = [{ node: response.data, depth: 0 }], panels = [];
  let visited = 0;
  while (pending.length) {
    const { node, depth } = pending.pop();
    if (++visited > 100000) fail();
    if (depth > 40) continue;
    if (!node || typeof node !== 'object') continue;
    if (!Array.isArray(node) && node.playlistId === input.playlist && Array.isArray(node.contents)) {
      if (panels.length >= 16) fail();
      panels.push(node);
      continue; // Deep video menus are irrelevant to queue discovery.
    }
    for (const value of Object.values(node)) if (value && typeof value === 'object') pending.push({ node: value, depth: depth + 1 });
  }
  const matches = [];
  for (const panel of panels) {
    if (!panel.contents.length || panel.contents.length > 4096) continue;
    const rows = [];
    let invalid = false;
    for (const item of panel.contents) {
      if (item.continuationItemRenderer) continue;
      const row = item.playlistPanelVideoRenderer ?? item.playlistPanelVideoWrapperRenderer?.primaryRenderer?.playlistPanelVideoRenderer;
      const endpoint = row?.navigationEndpoint?.watchEndpoint;
      const unavailable = row?.isPlayable === false;
      if (!row || (!unavailable && (!videoId(row.videoId) || endpoint?.videoId !== row.videoId)) ||
          (endpoint?.playlistId !== undefined && endpoint.playlistId !== input.playlist)) { invalid = true; break; }
      const global = endpoint?.index ?? endpoint?.playlistIndex;
      if ((global !== undefined && !index(global)) ||
          (endpoint?.index !== undefined && endpoint?.playlistIndex !== undefined &&
           endpoint.index !== endpoint.playlistIndex)) { invalid = true; break; }
      rows.push({ id: unavailable ? null : row.videoId, index: global, selected: row.selected === true });
    }
    if (invalid || !rows.length) continue;
    const selected = rows.findIndex(row => row.selected);
    const selectedRows = rows.filter(row => row.selected);
    const current = panel.currentIndex ?? input.index;
    // Index-only navigation needs upstream proof of the selected occurrence.
    // Never infer its position merely from the requested index or a radio rebase.
    if (lookup && (panel.isInfinite === true || selectedRows.length !== 1 ||
        (rows[selected].index !== input.index && panel.currentIndex !== input.index))) continue;
    const selectedId = lookup ? rows[selected].id : input.id;
    if (!videoId(selectedId)) continue;
    const radio = panel.isInfinite === true && input.playlist.startsWith('RD');
    if (selectedRows.length > 1 || !index(current) || (current !== input.index && (!radio || current > input.index))) continue;
    // Radio/Mix can restart its service-relative indices at the selected video.
    const shift = input.index - current;
    const anchor = rows.findIndex(row => row.index !== undefined && row.index + shift === input.index && row.id === selectedId);
    const at = anchor >= 0 ? anchor : selected >= 0 && rows[selected].id === selectedId ? selected : -1;
    if (at < 0) continue;
    const base = input.index - at;
    if (!index(base)) continue;
    const explicit = rows.every(row => row.index !== undefined);
    const positioned = rows.map((row, offset) => ({...row, index: explicit ? row.index + shift : base + offset}));
    if (positioned.some((row, offset) => !index(row.index) ||
        (offset > 0 && row.index <= positioned[offset - 1].index)) ||
        (!explicit && rows.some((row, offset) => row.index !== undefined && row.index + shift !== base + offset))) continue;
    if (selected >= 0 && (positioned[selected].id !== selectedId || positioned[selected].index !== input.index)) continue;
    const playable = positioned.filter(row => row.id !== null);
    const selectedPlayable = playable.findIndex(row => row.index === input.index && row.id === selectedId);
    if (selectedPlayable < 0) continue;
    const start = Math.max(0, Math.min(selectedPlayable - 32, playable.length - 128));
    const window = playable.slice(start, start + 128);
    const sparse = window.some((row, offset) => row.index !== window[0].index + offset);
    matches.push({ api_version: 1, playlist: input.playlist, index: input.index,
      video_id: selectedId, window_start: window[0].index, video_ids: window.map(row => row.id),
      ...(sparse ? {video_indices: window.map(row => row.index)} : {}), radio_rebased: shift !== 0 });
  }
  if (matches.length !== 1) fail();
  return matches[0];
}
