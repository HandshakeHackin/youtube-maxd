// Original request-driven live-chat reader. SPDX-License-Identifier: GPL-3.0-or-later
import { randomUUID } from 'node:crypto';

export class ChatReader {
  constructor(ErrorType, text, imageUrl, clock) {
    this.ErrorType = ErrorType; this.text = text; this.imageUrl = imageUrl; this.clock = clock;
    this.contexts = new Set(); this.cursors = new Map();
  }
  fail(code, message, status = 409) { throw new this.ErrorType(code, message, status); }
  prune() {
    for (const context of this.contexts) if (context.deadline <= this.clock()) this.drop(context);
    for (const [key, entry] of this.cursors) if (entry.deadline <= this.clock()) this.cursors.delete(key);
  }
  drop(context) {
    this.contexts.delete(context);
    for (const [key, entry] of this.cursors) if (entry.context === context) this.cursors.delete(key);
  }
  normalize(node) {
    if (!node || !['LiveChatTextMessage', 'LiveChatPaidMessage', 'LiveChatPaidSticker',
      'LiveChatMembershipItem', 'LiveChatSponsorshipsGiftPurchaseAnnouncement',
      'LiveChatSponsorshipsGiftRedemptionAnnouncement'].includes(node.type) || !node.id) return null;
    return { id: this.text(node.id, 160), author: this.text(node.author?.name ?? node.header?.author_name),
      content: this.text(node.message ?? node.header_subtext ?? node.header_primary_text ?? node.header?.primary_text ?? node.sticker_accessibility_label, 4096),
      published: this.text(node.timestamp_text, 64), amount: this.text(node.purchase_amount, 64),
      type: node.type, art_url: this.imageUrl(node.author?.thumbnails?.[0]?.url ?? node.header?.author_photo?.[0]?.url),
      moderator: !!node.author?.is_moderator, verified: !!node.author?.is_verified,
      author_id: this.text(node.author?.id ?? node.author_external_channel_id, 100) };
  }
  update(messages, actions) {
    if (!Array.isArray(actions) || actions.length > 4096)
      this.fail('chat_response', 'The chat response exceeds its action limit.', 502);
    const output = new Map(messages);
    for (const action of actions) {
      if (action.type === 'RemoveChatItemAction' || action.type === 'MarkChatItemAsDeletedAction')
        output.delete(this.text(action.target_item_id, 160));
      else if (action.type === 'RemoveChatItemByAuthorAction' || action.type === 'MarkChatItemsByAuthorAsDeletedAction') {
        const author = this.text(action.external_channel_id, 100);
        if (author) for (const [id, item] of output) if (item.author_id === author) output.delete(id);
      } else if (['AddChatItemAction', 'ReplaceChatItemAction'].includes(action.type)) {
        const item = this.normalize(action.item ?? action.replacement_item);
        if (action.type === 'ReplaceChatItemAction') output.delete(this.text(action.target_item_id, 160));
        if (item) output.set(item.id, item);
      }
    }
    while (output.size > 256) output.delete(output.keys().next().value);
    return output;
  }
  async fetch(context, token) {
    const response = await context.actions.execute('live_chat/get_live_chat', { continuation: token, parse: true });
    const contents = response?.continuation_contents;
    if (contents?.type !== 'liveChatContinuation') this.fail('chat_response', 'YouTube did not return a chat update.', 502);
    return contents;
  }
  snapshot(context, cursor, delay) {
    return { api_version: 1, kind: 'live_chat', id: context.id, chat_mode: context.mode,
      items: Array.from(context.messages.values()).slice(-24).map(({ author_id, ...item }) => item),
      cursor, has_more: false, ended: !context.token, is_replay: false, poll_after_ms: delay };
  }
  commit(context, contents) {
    const messages = this.update(context.messages, contents.actions ?? []);
    const token = contents.continuation?.token;
    if (token !== undefined && (typeof token !== 'string' || token.length > 65536))
      this.fail('chat_response', 'YouTube returned an invalid chat continuation.', 502);
    context.messages = messages; context.token = token || '';
    const delay = context.token ? Math.max(1000, Math.min(30000, Number(contents.continuation?.timeout_ms) || 2000)) : 0;
    context.next_at = this.clock() + delay; context.deadline = this.clock() + 1800000;
    if (this.cursors.size >= 256) this.cursors.delete(this.cursors.keys().next().value);
    const cursor = randomUUID();
    this.cursors.set(cursor, { context, deadline: context.deadline });
    return this.snapshot(context, cursor, delay);
  }
  async execute(input, owner, client) {
    this.prune();
    if (input.action === 'live_chat') {
      if (this.contexts.size >= 16) this.fail('chat_busy', 'Close another chat before opening this one.', 429);
      const info = await client.getInfo(input.id);
      if (info?.basic_info?.id !== input.id || !info.livechat?.continuation || !info.actions)
        this.fail('chat_unavailable', 'This video has no available live chat.', 404);
      if (info.livechat.is_replay) this.fail('chat_replay_pending', 'Chat replay is not connected to playback yet.', 422);
      const context = { owner, id: input.id, mode: input.chat_mode ?? 'top', actions: info.actions,
        messages: new Map(), token: info.livechat.continuation,
        sender: typeof info.getLiveChat === 'function' ? info.getLiveChat() : null, sends: new Map() };
      let contents = await this.fetch(context, context.token);
      const views = contents.header?.view_selector?.sub_menu_items;
      const selected = views?.[context.mode === 'top' ? 0 : 1];
      if (context.mode === 'live' && !selected)
        this.fail('chat_mode_unavailable', 'YouTube did not offer the live-chat view.', 422);
      if (selected && !selected.selected && !selected.continuation)
        this.fail('chat_mode_unavailable', 'YouTube did not offer this chat continuation.', 422);
      if (selected?.continuation && !selected.selected) contents = await this.fetch(context, selected.continuation);
      const result = this.commit(context, contents);
      this.contexts.add(context);
      return result;
    }
    const entry = this.cursors.get(input.cursor);
    if (!entry || entry.context.owner !== owner) this.fail('cursor_expired', 'Reopen this chat to continue.');
    const context = entry.context;
    if (input.action === 'chat_close') { this.drop(context); return { api_version: 1, success: true }; }
    if (input.action === 'chat_send') {
      if (input.id !== context.id || !context.token || typeof context.sender?.sendMessage !== 'function')
        this.fail('chat_unavailable', 'Reopen an active chat before sending a message.');
      if (typeof input.text !== 'string' || !input.text.trim() || Array.from(input.text).length > 200 || /[\0-\x1f\x7f]/.test(input.text) ||
          !/^[A-Za-z0-9_-]{1,64}$/.test(input.session ?? ''))
        this.fail('invalid_message', 'Enter a message of 1 to 200 characters.', 400);
      const attempted = context.sends.get(input.session);
      if (attempted) {
        if (attempted.text === input.text && attempted.result) return attempted.result;
        this.fail('chat_unconfirmed', 'Check the chat; this message intent cannot be repeated.');
      }
      if (context.sends.size >= 256) this.fail('chat_busy', 'Reopen the chat before sending more messages.', 429);
      const send = { text: input.text, result: null };
      context.sends.set(input.session, send);
      const actions = await context.sender.sendMessage(input.text);
      const items = Array.from(actions ?? []).filter(action => action.type === 'AddChatItemAction')
        .map(action => this.normalize(action.item)).filter(item => item?.type === 'LiveChatTextMessage' && item.content === input.text);
      if (items.length !== 1) this.fail('chat_unconfirmed', 'YouTube did not confirm this message.', 502);
      context.messages.set(items[0].id, items[0]);
      while (context.messages.size > 256) context.messages.delete(context.messages.keys().next().value);
      send.result = { api_version: 1, success: true, id: context.id, session: input.session, message_id: items[0].id };
      return send.result;
    }
    if (entry.result) return entry.result;
    if (!context.token || this.clock() < context.next_at)
      return this.snapshot(context, input.cursor, Math.max(0, context.next_at - this.clock()));
    entry.result = this.commit(context, await this.fetch(context, context.token));
    return entry.result;
  }
}
