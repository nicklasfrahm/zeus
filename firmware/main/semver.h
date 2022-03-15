#ifndef SEMVER_H
#define SEMVER_H

#include <stdint.h>
#include <stdlib.h>

// Initializes a semver to its default values.
#define SEMVER_INITIALIZER \
  { .major = 0, .minor = 0, .patch = 0, .prerelease_id = {0}, .prerelease = 0 }

/**
 * Describes a semantic version. More information can be found at
 * https://semver.org.
 *
 * @param major An int indicating breaking changes.
 * @param minor An int indicating feature releases.
 * @param patch An int indicating bugfix releases.
 * @param prerelease_id A string indicating the prerelease identifier, such as
 * "alpha" or "beta".
 * @param prerelease An int indicating the prerelease version.
 */
typedef struct semver {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  char prerelease_id[33];
  uint32_t prerelease;
} semver_t;

/**
 * Parse a semantic version string. Although not a valid semantic version,
 * the string may also contain a prefix, similar to "v1.0", commonly used for
 * Git tags. Please note that this does NOT currently support BUILD identifiers.
 *
 * @param[out] ver A pointer to an INITIALIZED semver struct.
 * @param[in] str A string containing the semver.
 */
void semver_parse(semver_t* ver, const char* str);

/**
 * Compare two semantic version strings. Although not a valid semantic
 * version, the string may also contain a prefix, similar to "v1.0",
 * commonly used for Git tags. Please note that this does NOT currently support
 * BUILD identifiers.
 *
 * @param[in] compare Semantic version string.
 * @param[in] reference Semantic version string.
 *
 * @return 1 if the compare version is above the reference version, 0 if the
 * versions match and -1 otherwise.
 */
int8_t semver_compare(const char* compare, const char* reference);

#endif
