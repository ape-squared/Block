hooks_source = [
  'hooks_device.cpp',
  'hooks_entrypoints.cpp',
  'hooks_instance.cpp',
  'hooks_surface.cpp',
  'hooks_swapchain.cpp',
]

hooks_deps = [
  libconfig,
  libgui,
  libinput,
  libruntime,
  libprofiles,
  libvk,
  glm,
  imgui,
  magic_enum,
  spdlog,
  vma,
  vulkan,
]

if is_linux
  hooks_deps += [wayland_client, x11, xcb, xkbcommon]
endif

# Keep recognizable export names from vkShade; shared library product name is Block → block.dll/.so
block_layer = shared_library(
  'block',
  hooks_source,
  include_directories : vkShade_include_path,
  dependencies : hooks_deps,
  gnu_symbol_visibility: 'hidden',
  name_prefix: '',
  install : true,
  install_dir : libdir
)
