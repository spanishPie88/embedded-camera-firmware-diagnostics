# UVC/V4L2 Diagnostic Report

## Executive summary

- **Device / platform:** [[REDACTED OR PUBLIC NAME]]
- **Host OS / kernel:** [[VERSION]]
- **Target mode:** [[FORMAT, RESOLUTION, FPS]]
- **Observed failure:** [[ONE-SENTENCE DESCRIPTION]]
- **Current root-cause confidence:** [[LOW / MEDIUM / HIGH]]

## Reproduction

1. [[STEP]]
2. [[STEP]]
3. [[STEP]]

Expected: [[EXPECTED RESULT]]

Actual: [[ACTUAL RESULT]]

## Evidence

### USB enumeration and descriptors

[[KEY FINDING AND FILE REFERENCE]]

### UVC probe/commit and streaming mode

[[KEY FINDING]]

### V4L2 capabilities and format enumeration

[[KEY FINDING]]

### Kernel and driver behavior

[[KEY FINDING]]

## Root cause

[[EXPLAIN THE EARLIEST PROVEN FAILURE, NOT ONLY THE FINAL SYMPTOM]]

## Recommended fix

1. [[FIX]]
2. [[FIX]]

## Verification plan

- [ ] Device enumerates without relevant kernel errors
- [ ] Required format/resolution/FPS is advertised
- [ ] Streaming starts and stops repeatedly
- [ ] No unexpected frame loss during [[DURATION]]
- [ ] Controls behave consistently on [[TARGET HOSTS]]

## Open questions / risks

- [[QUESTION OR RISK]]

