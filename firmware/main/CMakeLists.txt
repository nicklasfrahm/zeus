# Embed the server root certificate into the final binary
idf_build_get_property(project_dir PROJECT_DIR)
idf_component_register(
  SRCS "git.c"
       "http.c"
       "net.c"
       "semver.c"
       "update.c"
       "zeus.c"
  INCLUDE_DIRS "."
  EMBED_TXTFILES ${project_dir}/certs/github_com_ca_cert.pem
)
