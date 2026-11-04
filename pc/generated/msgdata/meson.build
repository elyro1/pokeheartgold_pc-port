message_index = custom_target('message-archive-index',
  input: message_inputs,
  output: 'msg.naix',
  command: [python, header_generator, '--index', '@OUTPUT@'])
subdir('msg')
