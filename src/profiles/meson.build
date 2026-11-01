libprofiles_library = static_library(
  'profiles',
  ['application_profile.cpp'],
  dependencies : [spdlog],
  include_directories : vkShade_include_path
)

libprofiles = declare_dependency(
  link_with : libprofiles_library,
)
