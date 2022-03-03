#ifndef GIT_H
#define GIT_H

/**
 * Get the Git repository link.
 *
 * @return A string.
 */
const char* git_url(void);

/**
 * Get a message describing how users may request support, including a link to
 * the bugtracker.
 *
 * @return A string.
 */
const char* git_support_message(void);

/**
 * Generate a download URL for a file of a given release version. Make sure to
 * free the memory after usage to prevent memory leaks.
 *
 * @param[in] version Git tag of the version or "latest".
 * @param[in] file Name of the file to be downloaded.
 *
 * @return A heap-allocated string.
 */
const char* git_release_download_url(const* version, const* file);

#endif
