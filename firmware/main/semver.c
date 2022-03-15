#include "semver.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * Check if the character is a digit.
 *
 * @param charcode An integer.
 *
 * @return true if the character is a digit, otherwise false.
 */
static bool semver_is_digit(char charcode) {
  return charcode > 47 && charcode < 58;
}

/**
 * Extract an integer from the semantic version, by skipping the prefix.
 *
 * @param[out] ver A pointer to either the major, minor, patch or prerelease.
 * @param[in] str A semantic version string.
 *
 * @return index of the next non-digit character.
 */
static int32_t semver_parse_int(uint32_t* ver, const char* str) {
  // Skip prefix.
  int32_t cursor = 0;
  while (!semver_is_digit(str[cursor])) {
    cursor += 1;
  };

  // Convert string to integer.
  *ver = (uint32_t)atoi(&str[cursor]);

  return cursor + 1;
}

void semver_parse(semver_t* ver, const char* str) {
  int32_t cursor = 0;

  // I could have used `memcpy` and an extra loop, but I feel that that would
  // have been overkill for a simple parser like this.

  // Parse major version.
  cursor += semver_parse_int(&ver->major, &str[cursor]);

  // Parse minor version, if present.
  if (str[cursor] == 0) {
    return;
  }
  cursor += semver_parse_int(&ver->minor, &str[cursor]);

  // Parse patch version, if present.
  if (str[cursor] == 0) {
    return;
  }
  cursor += semver_parse_int(&ver->patch, &str[cursor]);

  // The optional pre-release identifier must be seperated with a hyphen.
  if (str[cursor] != '-') {
    return;
  }
  cursor += 1;

  int32_t i = 0;
  while (str[cursor] != '.' && str[cursor] != 0) {
    ver->prerelease_id[i] = str[cursor];
    cursor += 1;
    i += 1;
  }

  // Parse the prerelease version, if present.
  if (str[cursor] == 0) {
    return;
  }
  semver_parse_int(&ver->prerelease, &str[cursor]);
}

int8_t semver_compare(const char* compare, const char* reference) {
  semver_t semver_compare = SEMVER_INITIALIZER;
  semver_t semver_reference = SEMVER_INITIALIZER;

  semver_parse(&semver_compare, compare);
  semver_parse(&semver_reference, reference);

  int32_t diff = semver_compare.major - semver_reference.major;
  if (diff) {
    return diff > 0 ? 1 : -1;
  }

  diff = semver_compare.minor - semver_reference.minor;
  if (diff) {
    return diff > 0 ? 1 : -1;
  }

  diff = semver_compare.patch - semver_reference.patch;
  if (diff) {
    return diff > 0 ? 1 : -1;
  }

  // Don't upgrade if the running firmware is "dirty" or and thus modified.
  bool modified = strstr(semver_reference.prerelease_id, "dirty") != NULL;
  if (modified) {
    // Returning -1 indicates that this is always considered a downgrade.
    return -1;
  }

  // This is a naive implementation which only handles "alpha" vs. "beta".
  diff = strcmp(semver_compare.prerelease_id, semver_reference.prerelease_id);
  if (diff) {
    return diff > 0 ? 1 : -1;
  }

  diff = semver_compare.prerelease - semver_reference.prerelease;
  if (diff) {
    return diff > 0 ? 1 : -1;
  }

  return 0;
}
