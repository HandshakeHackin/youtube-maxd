// Playback-clock driven replay; no background timers. SPDX-License-Identifier: GPL-3.0-or-later
import { Parser } from 'youtubei.js';

export class ReplayTimeline {
  constructor(reader) { this.reader = reader; }
  position(input) {
    const value = input.chat_position_sec;
    if (typeof value !== 'number' || !Number.isFinite(value) || value < 0 || value > 1000000)
      this.reader.fail('chat_playback_required', 'Open the video to synchronize chat replay with playback.', 422);
    return Math.floor(value * 1000 + 0.000001);
  }
  token(value) {
    if (value === undefined) return '';
    if (typeof value !== 'string' || value.length > 65536)
      this.reader.fail('chat_response', 'YouTube returned an invalid replay continuation.', 502);
    return value;
  }
  async fetch(context, token, position) {
    const response = await context.actions.execute('live_chat/get_live_chat_replay', {
      continuation: token, currentPlayerState: { playerOffsetMs: String(position) }, parse: false,
    });
    const data = response?.data;
    const raw = data?.continuationContents?.liveChatContinuation;
    if (!raw || !Array.isArray(raw.actions ?? []) || (raw.actions?.length ?? 0) > 4096 ||
        Buffer.byteLength(JSON.stringify(raw)) > 1024 * 1024)
      this.reader.fail('chat_response', 'YouTube did not return a bounded chat replay update.', 502);
    const continuations = raw.continuations ?? [];
    if (!Array.isArray(continuations) || continuations.length > 16)
      this.reader.fail('chat_response', 'YouTube returned invalid replay continuations.', 502);
    let next = '', seek = '';
    for (const row of continuations) {
      if (row.playerSeekContinuationData) seek = this.token(row.playerSeekContinuationData.continuation);
      if (row.liveChatReplayContinuationData) next = this.token(row.liveChatReplayContinuationData.continuation);
    }
    const parsed = Parser.parseResponse(data).continuation_contents;
    if (parsed?.type !== 'liveChatContinuation')
      this.reader.fail('chat_response', 'YouTube did not return a chat replay update.', 502);
    const events = [];
    for (const group of parsed.actions ?? []) {
      if (group.type !== 'ReplayChatItemAction') continue;
      if (!/^-?[0-9]+$/.test(String(group.video_offset_time_msec ?? '')))
        this.reader.fail('chat_response', 'YouTube returned an invalid replay timestamp.', 502);
      const offset = Number(group.video_offset_time_msec);
      if (!Number.isSafeInteger(offset) || Math.abs(offset) > 1000000000 || !Array.isArray(group.actions))
        this.reader.fail('chat_response', 'YouTube returned an invalid replay timestamp.', 502);
      for (const action of group.actions) {
        events.push({ offset, action });
        if (events.length > 4096) this.reader.fail('chat_response', 'The replay update exceeds its action limit.', 502);
      }
    }
    return { events, next, seek, header: parsed.header };
  }
  apply(context, position) {
    const due = [], future = [];
    for (const event of context.pending) (event.offset <= position ? due : future).push(event);
    due.sort((a, b) => a.offset - b.offset);
    let consumed = 0;
    for (const { offset, action } of due) {
      if (context.paginated) {
        const item = this.reader.normalize(action.item ?? action.replacement_item);
        if (item && !context.messages.has(item.id) && context.messages.size >= 256) break;
      }
      const updated = this.reader.update(context.messages, [action]);
      if (context.paginated && Buffer.byteLength(JSON.stringify([...updated.values()])) > 700 * 1024) break;
      context.messages = updated;
      const node = action.item ?? action.replacement_item;
      if (node?.id && context.messages.has(node.id)) context.messages.get(node.id).offset_ms = offset;
      ++consumed;
    }
    context.pending = [...due.slice(consumed), ...future];
  }
  async advance(context, position, initial = false, page = 0) {
    // Stage every change: malformed/failed responses must not erase the last good view.
    const staged = { ...context, messages: new Map(context.messages), pending: [...context.pending] };
    const seek = !initial && (context.paginated ? position !== context.position || (page === 0 && context.page > 0) :
      position < context.position || position - context.position > 10000);
    const nextPage = context.paginated && !seek && page > context.page;
    if (nextPage) staged.messages = new Map();
    staged.page = page;
    if (seek) {
      staged.messages = new Map(); staged.pending = [];
      staged.token = context.seek_token || context.seed;
    }
    this.apply(staged, position);
    let selected = !initial;
    const mayFetch = initial || seek || (context.paginated ? nextPage && !staged.pending.length : (!staged.pending.length &&
      (position !== context.position || !context.messages.size || context.catching_up) && this.reader.clock() >= context.next_at));
    if (mayFetch) {
      staged.catching_up = false;
      for (let request = 0; staged.token && request < 4; ++request) {
        const fetched = await this.fetch(staged, staged.token, position);
        if (!selected) {
          selected = true;
          const views = fetched.header?.view_selector?.sub_menu_items;
          const view = views?.[staged.mode === 'top' ? 0 : 1];
          if (staged.mode === 'live' && !view)
            this.reader.fail('chat_mode_unavailable', 'YouTube did not offer this replay view.', 422);
          if (view && !view.selected) {
            staged.token = this.token(view.continuation);
            if (!staged.token) this.reader.fail('chat_mode_unavailable', 'YouTube did not offer this replay continuation.', 422);
            staged.seed = staged.token;
            if (request === 3) staged.catching_up = true;
            continue;
          }
        }
        if (fetched.seek) staged.seek_token = fetched.seek;
        const previous = staged.token;
        staged.token = fetched.next;
        staged.pending.push(...fetched.events);
        if (staged.pending.length > 4096 || Buffer.byteLength(JSON.stringify(staged.pending)) > 1024 * 1024)
          this.reader.fail('chat_response', 'The replay buffer exceeds its action limit.', 502);
        this.apply(staged, position);
        // Future actions are held until video reaches them. Empty header pages may advance once.
        if (staged.pending.length || staged.token === previous || !staged.token) break;
        if (request === 3) staged.catching_up = true;
      }
    }
    staged.page_more = !!staged.paginated && (staged.pending.some(event => event.offset <= position) ||
      (!!staged.token && !staged.pending.length && !!staged.catching_up));
    staged.position = position; staged.next_at = this.reader.clock() + 1000;
    staged.deadline = this.reader.clock() + 1800000;
    Object.assign(context, staged);
    return this.reader.commitReplayCursor(context);
  }
}
