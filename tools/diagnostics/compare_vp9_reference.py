#!/usr/bin/env python3
"""Compare sampled luma identity; packet-byte equivalence must be established separately."""
import argparse
from bisect import bisect_left
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path


def load(path):
    with Path(path).open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    result = []
    for row in rows:
        result.append({key: int(row[key]) for key in
            ("pts_us", "luma_hash", "hash_valid", "width", "height", "ten_bit")})
    return result


def compare(reference, native):
    by_pts, by_hash = defaultdict(list), defaultdict(list)
    for index, row in enumerate(reference):
        by_pts[row["pts_us"]].append(index)
        if row["hash_valid"]:
            by_hash[row["luma_hash"]].append(index)
    occurrence = Counter()
    pairs = Counter()
    records = []
    exact = invalid = missing_pts = geometry_mismatch = ambiguous = 0
    for index, row in enumerate(native):
        pts = row["pts_us"]
        which = occurrence[pts]
        occurrence[pts] += 1
        indexes = by_pts[pts]
        expected = indexes[which] if which < len(indexes) else None
        record = {"native_index": index, "pts_us": pts, "pts_occurrence": which,
                  "reference_index": expected, "nearest_hash_index": None, "offset": None, "hash_candidates": 0}
        if expected is None:
            missing_pts += 1
        elif not row["hash_valid"] or not reference[expected]["hash_valid"]:
            invalid += 1
        else:
            target = reference[expected]
            if any(row[key] != target[key] for key in ("width", "height", "ten_bit")):
                geometry_mismatch += 1
            same = row["luma_hash"] == target["luma_hash"]
            exact += same
            candidates = by_hash[row["luma_hash"]]
            record["hash_candidates"] = len(candidates)
            ambiguous += len(candidates) > 1
            if candidates:
                at = bisect_left(candidates, expected)
                near = candidates[max(0, at-1):at+1]
                match = min(near, key=lambda item: (abs(item-expected), item))
                record["nearest_hash_index"] = match
                record["offset"] = match - expected
        records.append(record)
    offsets = Counter(str(row["offset"]) for row in records if row["reference_index"] is not None)
    transitions = []
    previous = object()
    for row in records:
        if row["offset"] != previous:
            transitions.append(row)
            previous = row["offset"]
    jumps = []
    for left, right in zip(records, records[1:]):
        a, b = left["nearest_hash_index"], right["nearest_hash_index"]
        if a is not None and b is not None:
            pairs[str(b-a)] += 1
            if b-a != 1:
                jumps.append({"native_index": right["native_index"], "pts_us": right["pts_us"],
                              "mapped_delta": b-a, "from_reference": a, "to_reference": b})
    backward = [row for row in jumps if row["mapped_delta"] < 0]
    back_intervals = Counter(str(b["native_index"]-a["native_index"]) for a,b in zip(backward,backward[1:]))
    unique_backward = [row for row in records if row["offset"] is not None and
                       row["offset"] < 0 and row["hash_candidates"] == 1]
    native_hashes = {row["luma_hash"] for row in native if row["hash_valid"]}
    expected_indexes = {row["reference_index"] for row in records if row["reference_index"] is not None}
    omitted = sorted(index for index in expected_indexes if reference[index]["hash_valid"] and
                     reference[index]["luma_hash"] not in native_hashes)
    omitted_intervals = Counter(str(b-a) for a,b in zip(omitted,omitted[1:]))
    return {
        "reference_frames": len(reference), "native_frames": len(native), "same_pts_occurrence_hash_matches": exact,
        "same_pts_occurrence_hash_mismatches": len(native)-exact-invalid-missing_pts,
        "missing_pts_occurrences": missing_pts, "invalid_hash_comparisons": invalid,
        "geometry_mismatches": geometry_mismatch, "ambiguous_hash_occurrences": ambiguous,
        "nearest_hash_offset_counts": dict(offsets), "mapped_adjacent_index_delta_counts": dict(pairs),
        "backward_jump_count": len(backward), "backward_jump_native_index_interval_counts": dict(back_intervals),
        "unique_backward_hash_matches": len(unique_backward),
        "expected_reference_hashes_absent_from_native": len(omitted),
        "first_absent_reference_indices": omitted[:20],
        "absent_reference_index_interval_counts": dict(omitted_intervals),
        "offset_transition_count": len(transitions), "first_offset_transitions": transitions[:20],
        "first_nonsequential_mappings": jumps[:20],
        "limits": ["Sparse fingerprints do not establish every pixel or compressed input identity.",
                   "Repeated hashes can represent static/repeated content; nearest matches are candidates, not unique proof.",
                   "Indices and PTS occurrence numbers are zero-based; frame order is never sorted or deduplicated."]
    }


def self_test():
    def row(pts, hash):
        return dict(pts_us=pts, luma_hash=hash, hash_valid=1, width=128, height=72, ten_bit=0)
    reference = [row(0,1),row(0,2),row(1,3),row(2,3)]
    native = [row(0,2),row(0,1),row(1,3),row(2,9)]
    result = compare(reference,native)
    assert result["same_pts_occurrence_hash_matches"] == 1
    assert result["same_pts_occurrence_hash_mismatches"] == 3
    assert result["nearest_hash_offset_counts"] == {"1":1,"-1":1,"0":1,"None":1}
    assert result["ambiguous_hash_occurrences"] == 1
    assert result["unique_backward_hash_matches"] == 1
    assert result["expected_reference_hashes_absent_from_native"] == 0
    assert result["mapped_adjacent_index_delta_counts"] == {"-1":1,"2":1}
    assert result["first_offset_transitions"][1]["pts_occurrence"] == 1
    native.append(row(99,5)); native.append(dict(row(2,3), hash_valid=0))
    result = compare(reference,native)
    assert result["missing_pts_occurrences"] == 2
    native[0]["hash_valid"] = 0
    assert compare(reference,native)["invalid_hash_comparisons"] == 1
    print("VP9 comparison occurrence preservation, repeated-hash ambiguity, missing/invalid rows: PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", nargs="?")
    parser.add_argument("native", nargs="?")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.reference or not args.native:
        parser.error("reference and native CSV paths are required")
    result = json.dumps(compare(load(args.reference), load(args.native)), indent=2) + "\n"
    if args.output:
        with args.output.open("x") as stream:
            stream.write(result)
    else:
        print(result, end="")


if __name__ == "__main__":
    main()
