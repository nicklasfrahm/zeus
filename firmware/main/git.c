#include "git.h"

#include <stdio.h>

/////////////
// git_url //
/////////////

static const char url[] = "https://github.com/nicklasfrahm/zeus";

const char* git_url(void) { return url; }

/////////////////////////
// git_support_message //
/////////////////////////

static char support_message_template[] =
    "For support, please create an issue at: "
    "%s/issues";

static char* support_message = NULL;

const char* git_support_message(void) {
  if (support_message == NULL) {
    asprintf(support_message, support_message_template, url);
  }
  return support_message;
}

//////////////////////////////
// git_release_download_url //
//////////////////////////////

const char* git_release_download_url(const* version, const* file) {
  char* url;
  if (strcmp(version, "latest") == 0) {
    asprintf(url, "%s/releases/latest/download/%s", git_url(), file);
  } else {
    asprintf(url, "%s/releases/download/%s/%s", git_url(), version, file);
  }
  return url;
}
