project('pokeheartgold-pc', ['c', 'cpp', 'nasm'],
  version: '0.1.0',
  meson_version: '>=1.3.0',
  default_options: ['c_std=gnu11', 'cpp_std=c++20', 'buildtype=debugoptimized', 'warning_level=2', 'default_library=static'])

build_target = get_option('build_target')
target_system = build_target == 'win64' ? 'windows' : 'linux'
if host_machine.system() != target_system or host_machine.cpu_family() != 'x86_64'
  error('Use an x86-64 ' + target_system + ' compiler for -Dbuild_target=' + build_target + '.')
endif

game_version = get_option('game_version')
game_code = game_version == 'heartgold' ? 'IPKE' : 'IPGE'
game_define = game_version == 'heartgold' ? '-DHEARTGOLD' : '-DSOULSILVER'
sdk_project = subproject('libntr', default_options: [
  'build_target=' + build_target, 'card_game_code=' + game_code, 'card_maker_code=01'])
system_project = subproject('libntrsystem', default_options: ['build_target=' + build_target])
wifi_project = subproject('libntrwifi', default_options: ['build_target=' + build_target])
dwc_project = subproject('libntrdwc', default_options: ['build_target=' + build_target])
vct_project = subproject('libvct', default_options: ['build_target=' + build_target])

# Upstream SDKs suppress their own warnings with -w. Keep application diagnostics.
sdk_args = []
foreach arg : sdk_project.get_variable('nitrosdk_args')
  if arg not in ['-w', '-DSDK_CW_FORCE_EXPORT_SUPPORT']
    sdk_args += arg
  endif
endforeach
sdk = declare_dependency(
  dependencies: dependency('NitroSDK').partial_dependency(
    compile_args: false, includes: true, links: true, link_args: true, sources: true).as_system('system'),
  compile_args: sdk_args)
sdk_system = dependency('NitroSystem').partial_dependency(
  compile_args: false, includes: true, links: true, link_args: true, sources: true).as_system('system')
game_overlay_link_args = ['-Wl,--wrap=FS_LoadOverlay', '-Wl,--wrap=FS_UnloadOverlay']

configuration = configuration_data()
configuration.set_quoted('HG_PC_GAME_NAME', game_version)
configuration.set_quoted('HG_PC_GAME_CODE', game_code)
configure_file(output: 'hg_pc_config.h', configuration: configuration)

python = import('python').find_installation('python3')
subdir('generated')
# The SDK's PC libraries are build-machine targets, including on Windows.
runtime_dependencies = [dependency('threads', native: true)]
if build_target == 'win64'
  runtime_dependencies += meson.get_compiler('c', native: true).find_library('winmm')
endif
# RPATH also applies to indirect dependencies of the libraries in lib/.
# RUNPATH alone would only cover each executable's direct dependencies.
runtime_link_args = build_target == 'linux' ? ['-Wl,--disable-new-dtags'] : []
runtime_rpath = build_target == 'linux' ? '$ORIGIN/lib' : ''
extractor = executable('hg-extract', 'asset_extract.cpp', 'runtime_directory.cpp',
  link_language: 'cpp', native: true,
  link_args: runtime_link_args, build_rpath: runtime_rpath,
  name_suffix: build_target == 'win64' ? 'exe' : [],
  dependencies: [sdk] + runtime_dependencies)

if build_target == 'linux'
  config_text_test = executable('hg-config-text-test', 'tests/config_text.c',
    dependencies: sdk, native: true, link_language: 'cpp',
    build_by_default: false)
  test('config UTF-16 text', config_text_test)
endif

# Native game sources and reviewed assembly constants.
game_sources = run_command(python, files('tools/game_sources.py'), check: true).stdout().strip().split('\n')
native_data_sources = run_command(python, files('tools/fixed_data.py'), '--list-sources', check: true).stdout().strip().split('\n')
native_data = []
foreach source : native_data_sources
  name = source.split('/')[-1].replace('.s', '')
  native_data += custom_target('native-data-' + name,
    input: source, output: name + '.S',
    depends: message_headers,
    depend_files: files('fixed_data_subsets.json', 'fixed_data_typed_tails.json', 'fixed_data_readonly.json', 'fixed_data_exports.json', 'fixed_data_typed_tables.json'),
    command: [python, files('tools/fixed_data.py'), '--platform', build_target, '@INPUT@', '@OUTPUT@'])
endforeach
# Native overlays are static code. Separate functions/data so disabled service
# entry points do not retain the DS-only implementation beside them in a TU.
native_game_section_args = ['-ffunction-sections', '-fdata-sections']
game_objects = static_library('hg-game-objects', game_sources, message_headers, message_index, resource_indices, native_data, overlay_ids,
  include_directories: include_directories('include', 'generated', '../src', '../include', '../include/library', '../files'),
  dependencies: [sdk, sdk_system,
    dependency('NitroWiFi').partial_dependency(compile_args: false, includes: true),
    dependency('NitroDWC').partial_dependency(compile_args: false, includes: true),
    dependency('libvct').partial_dependency(compile_args: false, includes: true)],
  c_args: [game_define, '-DENGLISH', '-DPM_KEEP_ASSERTS',
    '-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types', '-Werror=pointer-to-int-cast', '-Werror=int-to-pointer-cast'] + native_game_section_args,
  cpp_args: [game_define, '-DENGLISH', '-DPM_KEEP_ASSERTS'] + native_game_section_args,
  override_options: ['cpp_std=c++14'],
  native: true,
  build_by_default: false)

# Explicit game target used by Makefile.pc.
native_game_dep = declare_dependency(link_with: game_objects,
  dependencies: [sdk, sdk_system, dependency('NitroWiFi'), dependency('NitroDWC'), dependency('libvct')] + runtime_dependencies,
  link_args: game_overlay_link_args + ['-Wl,--gc-sections'])
executable('hg-game', 'game_anchor.c', 'game_startup.cpp', 'runtime_directory.cpp', 'game_diagnostics.c', 'audio_diagnostics.c', 'gui_config.cpp', 'gui_cheats.cpp', dependencies: native_game_dep,
  link_args: runtime_link_args + ['-Wl,--wrap=main', '-Wl,--wrap=SDL_GL_SwapWindow', '-Wl,--wrap=SDL_OpenAudio', '-Wl,--wrap=SIM_Config_LoadDefaults', '-Wl,-Map=hg-game.map'],
  build_rpath: runtime_rpath,
  link_language: 'cpp', native: true, name_suffix: build_target == 'win64' ? 'exe' : [], build_by_default: false)
