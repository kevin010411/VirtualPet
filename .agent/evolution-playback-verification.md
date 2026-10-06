# Evolution playback counts verification

Scope: Web editor and preview settings, runtime export and independent inspection,
firmware appearance decoding and Evolution playback. Existing unrelated firmware
worktree changes were preserved. No commit, deployment, or device upload performed.

Cross-layer contract: `E:/C++/virtualPet/web/.scratch/evolution-playback-counts/spec.md`.

Passed:

- Web preflight and backend architecture contracts.
- Focused canonical verification: 23 backend tests, 1 frontend file / 15 tests,
  and production build.

```text
Architecture: backend contracts passed
Backend: 23 tests passed
Frontend: 1 files / 15 tests passed
Build: production bundle passed
```
- Game host tests: default, FirstStart enabled, and appearance selection enabled.
  Actual frame draws match separate configured counts; species entry and save
  occur once after the source segment.
- Runtime Table host tests: actual SD record decoding returns configured source
  and target counts; zero, six, and missing-capability records are rejected.
- PlatformIO `project_29`: RAM 6,860 / 20,480 bytes; Flash 56,744 / 65,536 bytes.
  These are current dirty-worktree build totals, not change-attributable savings.

Final full Web verification: backend architecture contracts and all 294 backend
tests passed. Frontend reported 58 passing files / 487 passing tests and six
5,000 ms timeouts in three unchanged test files: EvolutionGraphPanel (2),
AdminPage (3), and AnimationTimelinePanel (1). Full verification did not pass
and stopped before its production build. The focused build listed above passed.
Canonical focused retry of EvolutionGraphPanel passed all 30 tests and rebuilt
the latest production bundle successfully, without changing that component,
its tests, or timeout settings.
AdminPage focused retry also passed all 13 tests and the latest production
build. AnimationTimelinePanel focused retry passed four tests but its unchanged
unsaved-workspace scenario still exceeded 5,000 ms. That timeout remains
unresolved; no timeout settings or unrelated tests were changed.
SD/TFT/button behavior on hardware remains unverified. Re-export the full SD
bundle and pair it with updated firmware for device acceptance.
