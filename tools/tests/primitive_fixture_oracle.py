#!/usr/bin/env python3
"""Independent big-integer/date oracle for the frozen primitive-only script.

Prints canonical JSON to stdout. Changing the golden is an explicit review step;
neither CTest nor CI regenerates or silently accepts it.
"""
import datetime
import json
import sys

MASK = (1 << 64) - 1
GAMMA = 0x9E3779B97F4A7C15
SEED = 123456789


def mix(value):
    value = ((value ^ (value >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    value = ((value ^ (value >> 27)) * 0x94D049BB133111EB) & MASK
    return value ^ (value >> 31)


def bounded(states, index, bound):
    threshold = (1 << 64) % bound
    while True:
        states[index] = (states[index] + GAMMA) & MASK
        draw = mix(states[index])
        if draw >= threshold:
            return draw % bound


def rounded(numerator, denominator):
    quotient, remainder = divmod(abs(numerator), denominator)
    magnitude = quotient + (2 * remainder >= denominator)
    return -magnitude if numerator < 0 else magnitude


def canonical(state):
    return json.dumps(state, separators=(",", ":"), ensure_ascii=True)


def checksum(state):
    result = 14695981039346656037
    for byte in canonical(state).encode("ascii"):
        result = ((result ^ byte) * 1099511628211) & MASK
    return f"{result:016x}"


def fixture():
    streams = [mix((SEED + GAMMA * (index + 1)) & MASK) for index in range(7)]
    cash = 120000000
    work = 0
    hashes = []
    for day in range(1, 1001):
        draws = [bounded(streams, index, 10001) for index in range(7)]
        cash += rounded(draws[0] * draws[3] * draws[5], 100000000)
        cash += rounded(-(1000000 + draws[2]) * (100 + draws[4]), 10000)
        capacity = 100000 + draws[1]
        work += capacity
        entities = [{"id": identity, "score": (draws[6] + identity) % 3} for identity in (11, 3, 7)]
        entities.sort(key=lambda entity: (-entity["score"], entity["id"]))
        state = {
            "ordinal_day": day,
            "date": (datetime.date(2014, 1, 1) + datetime.timedelta(days=day)).isoformat(),
            "cash_cents": cash,
            "work_milli_CU": work,
            "capacity_milli_CU_per_day": capacity,
            "rate_bp": draws[4],
            "probability_ppm": draws[5] * 100,
            "rng_states": list(streams),
            "entities": entities,
        }
        hashes.append(checksum(state))
    return {"fixture": "primitive-1000-days-v1", "hash": "fnv1a64-canonical-json-v1",
            "seed": SEED, "daily_hashes": hashes, "final": state}


if __name__ == "__main__":
    sys.stdout.write(canonical(fixture()) + "\n")
