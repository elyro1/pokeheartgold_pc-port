message_headers = custom_target('message-headers',
  input: message_inputs,
  output: message_names,
  command: [python, header_generator, '--headers', meson.current_build_dir()])
