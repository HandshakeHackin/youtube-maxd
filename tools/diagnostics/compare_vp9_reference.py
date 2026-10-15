#!/usr/bin/env python3
"""Compare sampled VP9 luma and optionally verify recorded compressed-packet prefixes."""
import argparse
from bisect import bisect_left
from collections import Counter, defaultdict
import csv
import json
from pathlib import Path


def load_csv(path):
    with Path(path).open(newline="") as stream:
        return list(csv.DictReader(stream))


def load(path):
    result = []
    for row in load_csv(path):
        result.append({key: int(row[key]) for key in
            ("pts_us", "luma_hash", "hash_valid", "width", "height", "ten_bit")})
    return result


def identity(row):
    return tuple(row[key] for key in ("width", "height", "ten_bit", "luma_hash"))


def compare(reference, native):
    by_pts, by_hash = defaultdict(list), defaultdict(list)
    for index, row in enumerate(reference):
        by_pts[row["pts_us"]].append(index)
        if row["hash_valid"]:
            by_hash[identity(row)].append(index)
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
                  "reference_index": expected, "nearest_hash_index": None, "offset": None, "hash_candidates": 0, "comparable": False}
        if expected is None:
            missing_pts += 1
        elif not row["hash_valid"] or not reference[expected]["hash_valid"]:
            invalid += 1
        else:
            target = reference[expected]
            if any(row[key] != target[key] for key in ("width", "height", "ten_bit")):
                geometry_mismatch += 1
                records.append(record)
                continue
            record["comparable"] = True
            same = row["luma_hash"] == target["luma_hash"]
            exact += same
            candidates = by_hash[identity(row)]
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
    native_hashes = {identity(row) for row in native if row["hash_valid"]}
    expected_indexes = {row["reference_index"] for row in records if row["comparable"]}
    omitted = sorted(index for index in expected_indexes if reference[index]["hash_valid"] and
                     identity(reference[index]) not in native_hashes)
    omitted_intervals = Counter(str(b-a) for a,b in zip(omitted,omitted[1:]))
    return {
        "reference_frames": len(reference), "native_frames": len(native), "same_pts_occurrence_hash_matches": exact,
        "same_pts_occurrence_hash_mismatches": len(native)-exact-invalid-missing_pts-geometry_mismatch,
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



def compare_packets(reference, native):
    """Strict recorded prefix identity, including packet occurrence order within epochs."""
    def key(row):
        digest = row["sha256"].lower()
        if int(row["sha256_valid"]) != 1 or int(row["size"]) <= 0 or len(digest) != 64 or any(
                char not in "0123456789abcdef" for char in digest):
            return None
        return int(row["pts_us"]), int(row["size"]), digest

    reasons = []
    if not reference or not native:
        reasons.append("empty packet trace")
    # The native recorder stops at 2048 rows; a full CSV cannot exclude an omitted tail.
    if len(native) >= 2048:
        reasons.append("native trace reached capacity; omitted tail cannot be excluded")
    expected = [key(row) for row in reference]
    if any(value is None for value in expected):
        reasons.append("invalid reference packet hash or size")
    if any(int(row["packet"]) != index+1 for index,row in enumerate(reference)):
        reasons.append("reference packet sequence is not contiguous from one")
    epochs = []
    previous_epoch = None
    for index,row in enumerate(native):
        epoch = int(row["epoch"])
        if int(row["packet"]) != index+1:
            reasons.append("native packet sequence has an omission or reorder")
        if epoch != previous_epoch:
            if previous_epoch is not None and epoch <= previous_epoch:
                reasons.append("native epoch order regresses or repeats")
            epochs.append(dict(epoch=epoch, recorded_packets=0, invalid_rows=0, mismatching_positions=0))
            previous_epoch = epoch
        current = epochs[-1]
        position = current["recorded_packets"]
        if int(row["epoch_packet"]) != position+1:
            reasons.append("native epoch packet sequence has an omission or reorder")
        current["recorded_packets"] += 1
        value = key(row)
        if value is None:
            current["invalid_rows"] += 1
        if value is None or position >= len(expected) or value != expected[position]:
            current["mismatching_positions"] += 1
    for epoch in epochs:
        epoch["recorded_prefix_identical"] = not reasons and not epoch["mismatching_positions"]
    return dict(recorded_packet_prefix_identical=bool(epochs) and all(
        epoch["recorded_prefix_identical"] for epoch in epochs),
        reference_packets=len(reference), native_packets=len(native), epochs=epochs,
        validation_errors=sorted(set(reasons)),
        completeness="Only recorded prefixes are verified; CSV alone cannot establish an unreported tail or whole-stream completeness.")


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
    wrong_geometry = compare([row(0,1)], [dict(row(0,1), width=256)])
    assert wrong_geometry["geometry_mismatches"] == 1
    assert wrong_geometry["same_pts_occurrence_hash_matches"] == 0
    assert wrong_geometry["same_pts_occurrence_hash_mismatches"] == 0
    assert wrong_geometry["expected_reference_hashes_absent_from_native"] == 0
    assert wrong_geometry["first_offset_transitions"][0]["hash_candidates"] == 0
    def packet(index, pts, digest, epoch=0, local=None):
        return dict(packet=index, epoch=epoch, epoch_packet=local or index,
                    pts_us=pts, size=12, sha256_valid=1, sha256=digest*64)
    packets = [packet(1,0,"a"), packet(2,0,"b"), packet(3,42000,"c")]
    assert compare_packets(packets,packets[:2])["recorded_packet_prefix_identical"]
    epochs = packets[:2] + [packet(3,0,"a",1,1)]
    assert compare_packets(packets,epochs)["recorded_packet_prefix_identical"]
    for bad in ([packets[1]], [packets[0],packets[2]],
                [dict(packets[0],sha256_valid=0)], [dict(packets[0],sha256="invalid")],
                [dict(packets[0],size=13)], [dict(packets[0],sha256="b"*64)],
                [dict(packets[0],pts_us=1)], [],
                [packets[0],dict(packets[1],sha256="a"*64)]):
        assert not compare_packets(packets,bad)["recorded_packet_prefix_identical"]
    full = [packet(i+1,i,"a") for i in range(2048)]
    assert not compare_packets(full,full)["recorded_packet_prefix_identical"]
    assert not compare_packets([dict(packets[0],sha256_valid=0)],packets[:1])["recorded_packet_prefix_identical"]
    print("Packet identity: ordered prefixes, equal PTS, epochs, omissions, invalid hashes and capacity: PASS")
    print("VP9 comparison occurrence preservation, repeated-hash ambiguity, missing/invalid rows: PASS")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", nargs="?")
    parser.add_argument("native", nargs="?")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--reference-packets", type=Path)
    parser.add_argument("--native-packets", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not args.reference or not args.native:
        parser.error("reference and native CSV paths are required")
    if bool(args.reference_packets) != bool(args.native_packets):
        parser.error("--reference-packets and --native-packets must be supplied together")
    result = compare(load(args.reference), load(args.native))
    if args.reference_packets:
        result["packet_identity"] = compare_packets(load_csv(args.reference_packets), load_csv(args.native_packets))
    result = json.dumps(result, indent=2) + "\n"
    if args.output:
        with args.output.open("x") as stream:
            stream.write(result)
    else:
        print(result, end="")


if __name__ == "__main__":
    main()
