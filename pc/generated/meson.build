header_generator = files('../tools/message_headers.py')
message_inputs = run_command(python, header_generator, '--list-inputs', check: true).stdout().strip().split('\n')
message_names = run_command(python, header_generator, '--list-names', check: true).stdout().strip().split('\n')
message_inputs = files(message_inputs)
subdir('msgdata')

overlay_ids = custom_target('overlay-ids',
  input: '../../main.lsf', output: 'hg_overlay_ids.h',
  command: [python, files('../tools/overlay_ids.py'), '@INPUT@', '@OUTPUT@'])

# The named .naix includes are generated beside this declared umbrella output.
# Always run the inexpensive metadata pass so deleted side outputs are restored;
# unchanged content retains timestamps and does not rebuild game objects.
resource_indices = custom_target('resource-indices',
  output: 'resource_indices.h',
  command: [python, files('../tools/resource_indices.py'), '--game', game_version,
    '--output', meson.current_build_dir()],
  build_always_stale: true)
