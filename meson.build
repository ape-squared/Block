# Windows/MSVC: cpp_std=c++latest (enables std::expected / std::format).
# Linux/GCC: configure with -Dcpp_std=c++23 (see Makefile).
project('Block', ['c', 'cpp'], version: files('VERSION'),
  default_options: [
    'c_std=c11',
    'cpp_std=c++latest',
    'b_ndebug=if-release',
    'force_fallback_for=inih',
  ])

cpp = meson.get_compiler('cpp')
host_system = host_machine.system()
is_windows = host_system == 'windows'
is_linux = host_system == 'linux'

vkShade_include_path = include_directories(['./src'])

datadir = join_paths(get_option('datadir'), 'Block')
libdir = get_option('libdir')

add_project_arguments('-DVK_NO_PROTOTYPES', language: 'cpp')
add_project_arguments('-DIMGUI_IMPL_VULKAN_NO_PROTOTYPES', language: 'cpp')
add_project_arguments('-DVKSHADE_PRODUCT_NAME="Block"', language: 'cpp')

if is_windows
  add_project_arguments('-DVK_USE_PLATFORM_WIN32_KHR', language: 'cpp')
  add_project_arguments('-DNOMINMAX', language: 'cpp')
  add_project_arguments('-DWIN32_LEAN_AND_MEAN', language: 'cpp')
endif

if is_linux
  add_project_arguments('-DVK_USE_PLATFORM_WAYLAND_KHR', language: 'cpp')
  add_project_arguments('-DVK_USE_PLATFORM_XCB_KHR', language: 'cpp')
  add_project_arguments('-DVK_USE_PLATFORM_XLIB_KHR', language: 'cpp')
endif

conf_paths = configuration_data()
conf_paths.set_quoted('SYSCONFDIR', join_paths(get_option('prefix'), get_option('sysconfdir')))
conf_paths.set_quoted('DATADIR', join_paths(get_option('prefix'), get_option('datadir')))

subdir('config')
subdir('src')

if get_option('tests')
  subdir('tests')
endif
