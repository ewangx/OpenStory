# GMS v95 PDB Analysis

This document records independently derived metadata and usage boundaries for
the GMS v95 debug database. The executable, PDB, and patch archive must remain
outside this repository.

## Provenance

- Source: Nexon's `00094to00095.patch` archive.
- The archive checksum and the checksums of both replacement entries validate.
- `MapleStory.exe`: 5,788,184 bytes, SHA-256
  `b14fbc5145d1463e515d6433716a323f1328829e10f04a20098f7b405f18d6eb`.
- `MapleStory.pdb`: 34,761,728 bytes, SHA-256
  `d6d7148561881ce3c218622c0fa0a3022b52b1017925acab99f412d3ea4803db`.

The PDB is an MSF 7 database with GUID
`ed1cf4f0-de3b-436e-8817-a2423aba8823`, age 1, 675 streams, 670 modules, and
661 private module streams. The packed executable has no visible CodeView
`RSDS` record, so its GUID and age cannot be compared directly. Its PE
timestamp is one second after the PDB signature timestamp, which is strong
provenance evidence but not a cryptographic match.

## Useful Coverage

Private module and symbol coverage exists for these client subsystems:

- Socket lifecycle and buffering: `ClientSocket`.
- Movement serialization: `MovePath`.
- Physics and foothold control: `VecCtrl` and its user, mob, NPC, pet, and
  summoned variants.
- Local player behavior: `UserLocal` and `UserLocal_Skill`.
- Rendering and animation: `Avatar` and `ActionMan`.
- Input and application lifecycle: `InputSystem`, `WndMan`, `Stage`, and
  `WvsApp`.

The PDB provides names, types, module boundaries, and line tables. It does not
provide the original source bodies.

## Evidence Rules

- Cosmic remains authoritative for opcodes, packet fields, authentication,
  migration, and all other server-owned v83 behavior.
- Official v83 behavior remains authoritative for client physics, animation,
  input, rendering, and UI behavior.
- v95 symbols may establish architecture, ownership, ordering hypotheses, and
  version-independent invariants.
- Version-sensitive findings require confirmation against Cosmic, v83 packet
  fixtures, or observed v83 client behavior before implementation.
- Only independently written findings, tests, and implementations belong in
  this repository. Do not commit extracted symbols, binaries, disassembly, or
  proprietary debug data.
