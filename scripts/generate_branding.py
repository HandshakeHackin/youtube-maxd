"""Create original code-drawn launcher artwork; no upstream logo is reused."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / "native/branding"
out.mkdir(exist_ok=True)
font = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
for name, width, height in (("icon0.png", 512, 512), ("pic0.png", 1920, 1080), ("pic1.png", 1920, 1080)):
    size = 120 if width == 512 else 96
    title = "PT" if width == 512 else "ProsperoTube"
    filters = f"drawbox=x=0:y=0:w=iw:h=18:color=0xef304c:t=fill,drawtext=fontfile={font}:text='{title}':fontsize={size}:fontcolor=white:x=(w-tw)/2:y=(h-th)/2"
    if width != 512:
        filters += f",drawtext=fontfile={font}:text='Your videos. Your screen.':fontsize=32:fontcolor=0x9aabc0:x=(w-tw)/2:y=h/2+110"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", f"color=c=0x101620:s={width}x{height}",
                    "-vf", filters, "-frames:v", "1", "-threads", "1", str(out / name)], check=True)
