"""Real WSL/Linux workers and evaluator descendants must retire together."""
import asyncio
import json
import os
from pathlib import Path
import signal
import sys

import pytest

from prosperotube.extractor import Extractor
from prosperotube.models import ServiceError


@pytest.mark.skipif(os.name != "posix" or not Path("/proc").is_dir(), reason="WSL/Linux process groups")
@pytest.mark.parametrize("mode", ["cancel", "timeout", "success"])
def test_real_worker_reaps_live_evaluator_descendant(tmp_path, monkeypatch, mode):
    marker = tmp_path / "child.json"
    worker = tmp_path / "worker.py"
    worker.write_text(
        "import json,os,subprocess,sys,time\n"
        "request=json.load(sys.stdin)\n"
        "child=subprocess.Popen([sys.executable,'-c','import time; time.sleep(120)'],"
        "stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)\n"
        f"open({str(marker)!r},'w').write(json.dumps({{'parent':os.getpid(),'child':child.pid,"
        "'parent_group':os.getpgrp(),'child_group':os.getpgid(child.pid)}))\n"
        + ("print(json.dumps({'id':'abcdefghijk','title':'Done'}),flush=True)\n" if mode == "success"
           else "time.sleep(120)\n"), encoding="utf-8")
    compile(worker.read_text(), str(worker), "exec")
    original = asyncio.create_subprocess_exec
    processes = []
    async def spawn(*args, **kwargs):
        assert kwargs["start_new_session"] is True
        process = await original(sys.executable, str(worker), **kwargs)
        processes.append(process)
        return process
    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)

    async def run():
        resolver = Extractor(timeout=.7 if mode == "timeout" else 10)
        task = asyncio.create_task(resolver.extract("https://www.youtube.com/watch?v=abcdefghijk"))
        try:
            async with asyncio.timeout(5):
                while not marker.is_file():
                    await asyncio.sleep(.01)
            identities = json.loads(marker.read_text())
            assert identities["parent"] == processes[0].pid
            assert identities["child_group"] == identities["parent_group"] == identities["parent"]
            assert os.getpgrp() != identities["parent"]
            if mode == "cancel":
                task.cancel()
                with pytest.raises(asyncio.CancelledError):
                    await task
            elif mode == "timeout":
                with pytest.raises(ServiceError) as error:
                    await task
                assert error.value.code == "timeout"
            else:
                assert (await task)["title"] == "Done"
            assert processes[0].returncode is not None
            # Init may still be reaping an orphan zombie; a zombie has already
            # stopped and cannot evaluate or retain pipes/semaphore capacity.
            async with asyncio.timeout(3):
                while True:
                    proc = Path(f"/proc/{identities['child']}/stat")
                    try:
                        state = proc.read_text().rsplit(")", 1)[1].split()[0]
                    except (FileNotFoundError, ProcessLookupError):
                        break
                    if state == "Z":
                        break
                    await asyncio.sleep(.01)
            assert not resolver.semaphore.locked()
            assert bool(resolver.cache) == (mode == "success")
        finally:
            if not task.done():
                task.cancel()
            await asyncio.gather(task, return_exceptions=True)
            # Test failure cleanup remains scoped to the freshly created group.
            for process in processes:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                await process.wait()
    asyncio.run(run())
