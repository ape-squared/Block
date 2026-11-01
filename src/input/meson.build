libinput_source = [
  'input_manager.cpp',
]

libinput_deps = [
  glm,
  magic_enum,
  spdlog,
]

if host_machine.system() == 'windows'
  libinput_source += ['input_backend_win32.cpp']
else
  libinput_source += [
    'input_backend_wayland.cpp',
    'input_backend_xcb.cpp',
    'input_backend_xlib.cpp',
    'wayland_callbacks.cpp',
  ]
  libinput_deps += [
    wayland_client,
    x11,
    xcb,
    xi,
    xkbcommon,
  ]
endif

libinput_library = static_library(
  'input',
  libinput_source,
  dependencies : libinput_deps,
  include_directories : vkShade_include_path
)

libinput = declare_dependency(
  link_with : libinput_library,
)
