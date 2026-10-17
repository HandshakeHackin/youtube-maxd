#!/usr/bin/env bash
# The two FFmpeg 7.1.1 trees the release checks compile against, built into an
# isolated directory (never the PS5 SDK): a pristine copy whose dashdec.c the
# DASH patch regression reads, and a host candidate with the HLS DVR patch and
# static archives for scripts/test_live_dvr.sh. Prints the environment the
# checks read (PROSPEROTUBE_DASH_SOURCE, PROSPEROTUBE_DVR_FFMPEG_SOURCE,
# PROSPEROTUBE_DVR_CANDIDATE). Reuses a directory built from the same recipe.
# usage: build_check_ffmpeg.sh [DIRECTORY]   (default ~/.cache/prosperotube/check-ffmpeg)
# Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
directory="${1:-${HOME}/.cache/prosperotube/check-ffmpeg}"
version=7.1.1
# The checksum EVO's scripts/build-ffmpeg.sh pins for the same release.
sha256=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
stamp=$(cat "${BASH_SOURCE[0]}" "${root}/scripts/patch_ffmpeg_hls_dvr.py" "${root}/scripts/ffmpeg_hls_dvr.inc" | sha256sum | cut -d ' ' -f 1)
if [[ "$(cat "${directory}/.recipe" 2>/dev/null)" != "${stamp}" ]]; then
    rm -rf "${directory}"
    mkdir -p "${directory}"
    tarball="${directory}/ffmpeg-${version}.tar.xz"
    curl -fsSL --retry 3 -o "${tarball}" "https://ffmpeg.org/releases/ffmpeg-${version}.tar.xz"
    printf '%s  %s\n' "${sha256}" "${tarball}" | sha256sum -c - >&2
    tar -xf "${tarball}" -C "${directory}"
    rm "${tarball}"
    cp -a "${directory}/ffmpeg-${version}" "${directory}/pristine"
    mv "${directory}/ffmpeg-${version}" "${directory}/dvr"
    python3 "${root}/scripts/patch_ffmpeg_hls_dvr.py" "${directory}/dvr/libavformat/hls.c" >&2
    # HEVC is enabled only because 7.1.1's H.264 decoder needs its film-grain code to link.
    (cd "${directory}/dvr" && ./configure --disable-everything --disable-autodetect --disable-doc \
        --disable-programs --disable-x86asm --enable-static --disable-shared --enable-avcodec \
        --enable-avformat --enable-swresample --enable-network --enable-protocol='file,http,tcp' \
        --enable-demuxer='hls,mpegts,aac,mov' --enable-decoder='h264,hevc,aac' --enable-parser='h264,aac' \
        --enable-bsf='h264_mp4toannexb,aac_adtstoasc' --enable-libxml2 --enable-openssl --enable-zlib \
        --enable-bzlib --enable-lzma) > "${directory}/configure.log" 2>&1 ||
        { tail -20 "${directory}/configure.log" >&2; exit 1; }
    make -C "${directory}/dvr" -j"$(nproc)" libavformat/libavformat.a libavcodec/libavcodec.a \
        libswresample/libswresample.a libavutil/libavutil.a > "${directory}/make.log" 2>&1 ||
        { tail -20 "${directory}/make.log" >&2; exit 1; }
    printf '%s\n' "${stamp}" > "${directory}/.recipe"
fi
printf '%s\n' "PROSPEROTUBE_DASH_SOURCE=${directory}/pristine/libavformat/dashdec.c" \
    "PROSPEROTUBE_DVR_FFMPEG_SOURCE=${directory}/dvr" "PROSPEROTUBE_DVR_CANDIDATE=1"
