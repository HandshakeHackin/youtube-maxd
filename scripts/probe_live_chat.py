"""Bounded anonymous live-chat worker probe; never posts or contacts the console."""
import argparse
import asyncio
import json
from pathlib import Path

from prosperotube.library import LibraryService
from prosperotube.models import ServiceError, video_id


async def probe(args):
    library = LibraryService(args.node)
    result = {'id': args.video, 'scope': 'public YouTube -> companion library worker',
              'console_traffic': False, 'account_profile_selected': False, 'result': 'failed'}
    cursor = ''
    stage = 'open'
    try:
        opened = await library.execute({'action': 'live_chat', 'id': args.video, 'profile': 'guest', 'chat_mode': args.mode})
        cursor = opened['cursor']
        if opened.get('kind') != 'live_chat' or opened.get('is_replay') is not False:
            raise ValueError('invalid chat')
        delay = opened.get('poll_after_ms', 0)
        if type(delay) is not int or not 0 <= delay <= 30000:
            raise ValueError('invalid delay')
        await asyncio.sleep(delay / 1000)
        stage = 'poll'
        update = await library.execute({'action': 'chat_poll', 'cursor': cursor, 'profile': 'guest'})
        cursor = update['cursor']
        if update.get('kind') != 'live_chat' or update.get('is_replay') is not False or not isinstance(update.get('items'), list):
            raise ValueError('invalid chat')
        stage = 'close'
        closed = await library.execute({'action': 'chat_close', 'cursor': cursor, 'profile': 'guest'})
        if closed.get('success') is not True:
            raise ValueError('invalid close')
        cursor = ''
        result.update(result='pass', checks=['open', 'poll', 'close'],
                      initial_messages=len(opened['items']), polled_messages=len(update['items']),
                      chat_mode=args.mode, ended=update.get('ended') is True)
    except ServiceError as error:
        result.update(stage=stage, code=error.code, status=error.status)
    except (KeyError, ValueError, OSError):
        result.update(stage=stage, code='probe_failed')
    finally:
        if cursor:
            try:
                await library.execute({'action': 'chat_close', 'cursor': cursor, 'profile': 'guest'})
            except ServiceError:
                pass
        await library.close()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--video', required=True)
    parser.add_argument('--node', required=True)
    parser.add_argument('--mode', choices=('top', 'live'), default='top')
    parser.add_argument('--output', type=Path, default=Path('output/validation/live-chat.json'))
    args = parser.parse_args()
    try:
        video_id(args.video)
    except ServiceError as error:
        parser.error(error.message)
    result = asyncio.run(probe(args))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['result'] == 'pass' else 1


if __name__ == '__main__':
    raise SystemExit(main())
