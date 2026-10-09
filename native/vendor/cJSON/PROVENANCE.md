# cJSON

Upstream: https://github.com/DaveGamble/cJSON

Version: v1.7.19, commit `c859b25da02955fef659d658b8f324b5cde87be3`.

Files: unmodified `cJSON.c`, `cJSON.h`, and `LICENSE` from that revision.
License: MIT; copyright and license text are preserved in these files.

ProsperoTube replaces EVO's minimal JSON implementation in the generated build.
The minimal implementation cannot serialize arrays, nested objects, booleans, or
escaped strings safely, all of which are needed for persistent local state and
quoted/Unicode search requests. The official implementation preserves the public
cJSON interface while providing its complete bounded parser and serializer.
