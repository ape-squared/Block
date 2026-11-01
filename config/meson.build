data_dir = get_option('datadir')

is_32bit = cpp.sizeof('void*') == 4
arch_suffix = is_32bit ? '_32' : '_64'

if host_machine.system() == 'windows'
  vulkan_layer_dir = join_paths(get_option('datadir'), 'vulkan', 'implicit_layer.d')
  # Relative library_path so VK_LAYER_PATH can point at the build/install dir.
  lib_dir_for_json = ''
  lib_suffix = '.dll'
else
  vulkan_layer_dir = join_paths(get_option('datadir'), 'vulkan', 'implicit_layer.d')
  lib_dir_for_json = join_paths(get_option('prefix'), libdir) + '/'
  lib_suffix = '.so'
endif

configure_file(
    input : 'Block.json.in',
    output : is_32bit ? 'Block_i386.json' : 'Block_x86_64.json',
    configuration : {
      'ld_lib_dir_vkshade' : lib_dir_for_json,
      'arch_suffix' : arch_suffix,
      'lib_suffix' : lib_suffix,
    },
    install : true,
    install_dir : vulkan_layer_dir,
)

install_data('Block.ini', install_dir: get_option('datadir') / 'Block')
install_data('ReShade.ini', install_dir: get_option('datadir') / 'Block')
# Keep upstream filename available for users migrating from vkShade.
install_data('vkShade.ini', install_dir: get_option('datadir') / 'Block')
