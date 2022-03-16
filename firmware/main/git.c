#include "git.h"

#include <stdio.h>
#include <string.h>

static const char url[] = "https://github.com/nicklasfrahm/zeus";

const char* git_url(void) { return url; }

static char support_message_template[] =
    "For support, please create an issue at: "
    "%s/issues";

static char* support_message = NULL;

char* git_support_message(void) {
  if (support_message == NULL) {
    asprintf(&support_message, support_message_template, url);
  }
  return support_message;
}

char* git_release_download_url(const char* version, const char* file) {
  char* url;
  if (strcmp(version, "latest") == 0) {
    asprintf(&url, "%s/releases/latest/download/%s", git_url(), file);
  } else {
    asprintf(&url, "%s/releases/download/%s/%s", git_url(), version, file);
  }
  return url;
}
