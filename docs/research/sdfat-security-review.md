# SdFat - Adafruit Fork security review

Reviewed: 2026-08-30

## Conclusion

Do **not** upgrade solely in response to an unspecified "SdFat CWE" finding.
The checked official vulnerability sources contain no published CVE or GitHub
Security Advisory for `adafruit/SdFat`, so there is no verified CWE, affected
version range, or fixed version to apply.  The project is already resolving a
newer package than the declared minimum.

If a scanner reported a finding, retain its rule ID, report URL, affected file
and line, and dependency identity before changing the dependency.  This review
does not establish that malformed SD cards are safe; it only establishes that
no published SdFat CVE/GHSA was found in the checked first-party databases.

## Installed dependency and build path

| Item | Evidence | Result |
| --- | --- | --- |
| Declared PlatformIO constraint | `platformio.ini` | `adafruit/SdFat - Adafruit Fork@^2.2.54` |
| Resolved PlatformIO package | `.pio/libdeps/project_18_memory/SdFat - Adafruit Fork/.piopm` | `2.3.103` |
| Embedded upstream SdFat version | `src/SdFat.h` in that resolved package | `SD_FAT_VERSION_STR "2.3.1"` |
| Filesystem configuration | resolved package `src/SdFatConfig.h` | `SDFAT_FILE_TYPE 1` (FAT) |

The `2.3.103` PlatformIO package number must not be compared directly with the
embedded upstream `2.3.1` source version.  The project initializes the card as
an SPI `SdFat` instance in `src/platform/main.cpp`; the SD card and its
filesystem content are therefore physically local, attacker-controlled input
when an untrusted card can be inserted.

## Security-source result

- The [Adafruit fork's GitHub Security Advisories endpoint](https://api.github.com/repos/adafruit/SdFat/security-advisories) returned `[]` when checked.
- The [upstream SdFat releases](https://github.com/greiman/SdFat/releases) show
  `2.3.1` as the current release; its release notes describe exFAT length-field
  behaviour, not a security advisory or CWE fix.
- The [upstream version definition](https://github.com/greiman/SdFat/blob/master/src/SdFat.h)
  identifies `2.3.1`, matching the resolved package source.
- The [official PlatformIO Registry entry](https://registry.platformio.org/libraries/adafruit/SdFat%20-%20Adafruit%20Fork)
  is the authoritative package identity for the `adafruit/...` dependency.
- No matching entry was found in the [GitHub Advisory Database](https://github.com/advisories)
  or [NVD](https://nvd.nist.gov/) during this review.  These are negative search
  results, not evidence that the code is defect-free.

## STM32 relevance and next action

No *known published* SdFat vulnerability currently justifies a dependency
upgrade for this STM32F103 FAT/SPI path.  An upgrade may still be appropriate
for maintenance or compatibility, but it needs a separate build and device
regression check because SdFat is at the filesystem boundary.

If the alleged CWE is instead a static-analysis warning in a project call site
or in vendored SdFat source, investigate that exact finding.  It may require
input-length/format validation in this firmware; upgrading a library version
without a corresponding upstream fix would not resolve it.
