import pytest
from pydantic import ValidationError
from prosperotube.app import LibraryRequest


def test_remote_fallback_origin_is_explicit_and_only_valid_for_next_metadata():
    assert LibraryRequest(action="playback_next", id="abcdefghijk", remote_origin=True).remote_origin
    assert not LibraryRequest(action="playback_next", id="abcdefghijk").remote_origin
    for patch in ({"remote_origin": "true"}, {"remote_origin": 1}, {"action": "home", "remote_origin": True}):
        with pytest.raises(ValidationError):
            LibraryRequest(**dict({"action": "playback_next", "id": "abcdefghijk"}, **patch))
