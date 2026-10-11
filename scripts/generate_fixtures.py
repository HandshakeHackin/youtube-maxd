"""Generate redistributable AVC/AAC, VP9/Opus, AV1/AAC, HEVC/AAC, HLS and caption fixtures."""
from pathlib import Path
import subprocess
import sys


def generate(out):
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    def ff(*args): subprocess.run(["ffmpeg", "-v", "error", "-y", *args], check=True)
    ff("-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30", "-t", "8", "-an", "-c:v", "libx264",
       "-g", "30", "-movflags", "+faststart", str(out / "video.mp4"))
    ff("-f", "lavfi", "-i", "sine=frequency=880:sample_rate=48000", "-t", "8", "-vn", "-c:a", "aac",
       "-movflags", "+faststart", str(out / "audio.m4a"))
    ff("-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30", "-t", "8", "-an", "-c:v", "libvpx-vp9",
       "-deadline", "realtime", "-cpu-used", "8", "-g", "30", str(out / "video.webm"))
    ff("-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30", "-t", "8", "-an", "-c:v", "libaom-av1",
       "-cpu-used", "8", "-row-mt", "1", "-threads", "2", "-crf", "40", "-g", "30",
       "-movflags", "+faststart", str(out / "video-av1.mp4"))
    ff("-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30", "-t", "8", "-an", "-c:v", "libx265",
       "-preset", "ultrafast", "-x265-params", "pools=1:frame-threads=1:keyint=30:log-level=error",
       "-tag:v", "hvc1", "-movflags", "+faststart", str(out / "video-hevc.mp4"))
    ff("-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000", "-t", "8", "-vn", "-c:a", "libopus", str(out / "audio.webm"))
    ff("-i", str(out / "video.mp4"), "-i", str(out / "audio.m4a"), "-c", "copy", "-movflags", "+faststart", str(out / "muxed.mp4"))
    ff("-i", str(out / "muxed.mp4"), "-c", "copy", "-hls_time", "2", "-hls_list_size", "0",
       "-hls_flags", "independent_segments", str(out / "fixture.m3u8"))
    ff("-f", "lavfi", "-i", "testsrc2=size=640x360", "-frames:v", "1", str(out / "thumb.png"))
    (out / "captions.vtt").write_text("WEBVTT\n\n00:00.000 --> 00:02.000\nProsperoTube fixture: picture and tone\n\n00:04.000 --> 00:06.000\nCaptions after seeking — café\n", encoding="utf-8")
    print(out)


if __name__ == "__main__":
    generate(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "fixtures/generated")
