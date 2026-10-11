"""The only code of the console session that reaches the console.

Every operation is one method of Console and is recorded (ops.jsonl) before it
is carried out. Installs, verified single-file uploads, the title listing and
the error records go through ps5-homebrew-dev-protocol's ps5-console.py; launch
and close through its title-aware send-controller.sh. What that tool does not
offer (reading a file from an offset, a listing, removing a file, a port
check, the kernel log stream) is done here with the standard library.

Nothing in this file decides anything: the session does, and a dry run
replaces this class with the simulator.
"""
from ftplib import FTP, all_errors, error_perm
import io
import json
import os
import socket
import subprocess
import time


class ConsoleDown(Exception):
    """The console did not answer. The session ends: nothing more is sent."""


class OpLog:
    def __init__(self, path, echo):
        self.path, self.echo, self.count, self.plan = path, echo, 0, []

    def record(self, kind, detail, write):
        self.count += 1
        entry = {"n": self.count, "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                 "kind": kind, "detail": detail, "writes": write}
        if self.path:
            with open(self.path, "a") as handle:
                handle.write(json.dumps(entry) + "\n")
        line = f"{kind} {detail}".strip()
        if self.plan and self.plan[-1][0] == line:
            self.plan[-1][1] += 1
        else:
            self.plan.append([line, 1])
            self.echo(f"    console op {self.count}: {line}")


class Console:
    def __init__(self, host, protocol, evidence, echo=print, ftp_port=2121, klog_port=3232, elf_port=9021):
        self.host, self.protocol, self.evidence = host, str(protocol), str(evidence)
        self.ftp_port, self.klog_port, self.elf_port = ftp_port, klog_port, elf_port
        self.ops = OpLog(os.path.join(self.evidence, "ops.jsonl"), echo)
        self._ftp = None
        self._klog = None
        self._env = dict(os.environ, PS5_HOST=host, PS5_FTP_PORT=str(ftp_port))

    # --- protocol tool ---------------------------------------------------------
    def _tool(self, arguments, timeout):
        command = ["python3", os.path.join(self.protocol, "scripts/ps5-console.py"), *arguments]
        try:
            done = subprocess.run(command, env=self._env, capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            raise ConsoleDown(f"ps5-console.py {arguments[0]} did not finish in {timeout} s") from None
        output = (done.stdout + done.stderr).strip()
        if done.returncode == 4:
            raise ConsoleDown(output or "console not answering")
        return done.returncode, output

    def titles(self):
        """Every running title (a sandbox under /mnt/sandbox). Read-only."""
        self.ops.record("ftp-list", "/mnt/sandbox (running titles, ps5-console.py titles)", False)
        code, output = self._tool(["titles"], 60)
        if code != 0 or not output.startswith("running:"):
            raise ConsoleDown(f"running titles unknown: {output}")
        names = output[len("running:"):].split()
        return [] if names == ["none"] else names

    def where(self, title):
        self.ops.record("ftp-read", f"/user/app/{title}/mount.lnk (ps5-console.py where)", False)
        return self._tool(["where", title], 60)[1]

    def errors(self):
        self.ops.record("ftp-list", "/system_data/priv/error/history (ps5-console.py errors)", False)
        return self._tool(["errors", "1"], 60)[1]

    def install(self, title, folder):
        self.ops.record("ftp-install", f"{folder} -> /data/homebrew/{title} (ps5-console.py install: "
                        "temp name, readback, rename; signed files by stored-SELF or size)", True)
        return self._tool(["install", title, str(folder)], 1800)

    def settle(self, title, folder, seconds):
        self.ops.record("ftp-settle", f"{seconds} s watch, then compare /data/homebrew/{title} again "
                        "(ps5-console.py settle)", False)
        return self._tool(["settle", title, str(folder), str(seconds)], seconds + 900)

    def put(self, remote, data, note=None):
        """One small file: temporary name, readback, rename (ps5-console.py put)."""
        self.ops.record("ftp-put", f"{remote} ({note or str(len(data)) + ' bytes'}; temp name, readback, rename)", True)
        local = os.path.join(self.evidence, ".put.tmp")
        with open(local, "wb") as handle:
            handle.write(data)
        try:
            return self._tool(["put", local, remote], 120)
        finally:
            os.unlink(local)

    # --- direct FTP ------------------------------------------------------------
    def _connect(self):
        if self._ftp is None:
            ftp = FTP()
            try:
                ftp.connect(self.host, self.ftp_port, timeout=15)
                ftp.login(os.environ.get("PS5_FTP_USER", "anonymous"),
                          os.environ.get("PS5_FTP_PASSWORD", "anonymous"))
                ftp.voidcmd("TYPE I")
            except (OSError, *all_errors) as error:
                raise ConsoleDown(f"FTP: {error}") from None
            self._ftp = ftp
        return self._ftp

    def _drop(self):
        if self._ftp is not None:
            try:
                self._ftp.close()
            except all_errors:
                pass
            self._ftp = None

    def _ftp_call(self, action):
        """One reconnect for a session the server dropped; a second failure ends it."""
        for attempt in (1, 2):
            try:
                return action(self._connect())
            except error_perm:
                raise
            except (OSError, EOFError, *all_errors) as error:
                self._drop()
                if attempt == 2:
                    raise ConsoleDown(f"FTP: {error}") from None

    def read(self, remote, offset=0):
        """The file's bytes from offset, or None when it does not exist."""
        self.ops.record("ftp-read", remote, False)

        def action(ftp, rest=offset or None):
            sink = io.BytesIO()
            ftp.retrbinary(f"RETR {remote}", sink.write, blocksize=262144, rest=rest)
            return sink.getvalue()
        try:
            return self._ftp_call(action)
        except error_perm:
            if not offset:
                return None
        try:  # a server without REST: the whole file, cut here
            return self._ftp_call(lambda ftp: action(ftp, None))[offset:]
        except error_perm:
            return None

    def size(self, remote):
        self.ops.record("ftp-size", remote, False)

        def action(ftp):
            size = ftp.size(remote)
            return size if size is not None and 0 <= size < 1 << 62 else None
        try:
            return self._ftp_call(action)
        except error_perm:
            return None

    def listdir(self, remote):
        """{name: {"type", "size"}} or None when the folder cannot be listed."""
        self.ops.record("ftp-list", remote, False)

        def action(ftp):
            ftp.cwd(remote)  # refused for a folder that does not exist
            try:
                return {name: {"type": facts.get("type"), "size": int(facts.get("size") or 0)}
                        for name, facts in ftp.mlsd() if name not in (".", "..")}
            finally:
                ftp.cwd("/")
        try:
            return self._ftp_call(action)
        except error_perm:
            return None

    def delete(self, remote):
        self.ops.record("ftp-delete", remote, True)
        try:
            self._ftp_call(lambda ftp: ftp.sendcmd(f"DELE {remote}"))
            return True
        except error_perm:
            return False

    # --- other services ----------------------------------------------------------
    def port_open(self, port):
        self.ops.record("tcp-connect", f"port {port} (connect and close, nothing sent)", False)
        try:
            socket.create_connection((self.host, port), timeout=5).close()
            return True
        except OSError:
            return False

    def klog_start(self, path):
        self.ops.record("klog", f"record port {self.klog_port} -> {os.path.basename(path)}", False)
        handle = open(path, "wb")
        self._klog = (subprocess.Popen(["nc", self.host, str(self.klog_port)], stdout=handle,
                                       stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL), handle)

    def klog_stop(self):
        if self._klog:
            process, handle = self._klog
            self._klog = None
            process.terminate()  # our own recorder, by its handle
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
            handle.close()

    def controller(self, mode, title):
        """The title-aware launch or close payload, sent once. Returns its exit status:
        0 only says it was delivered, the effect is read from the console afterwards."""
        self.ops.record("payload", f"{mode} {title} -> port {self.elf_port} (send-controller.sh)", True)
        try:
            done = subprocess.run(["bash", os.path.join(self.protocol, "scripts/send-controller.sh"),
                                   mode, title, self.host, str(self.elf_port)],
                                  capture_output=True, text=True, timeout=90)
        except subprocess.TimeoutExpired:
            return 124, "send-controller.sh did not finish in 90 s"
        return done.returncode, (done.stdout + done.stderr).strip()

    def finish(self):
        self.klog_stop()
        self._drop()
