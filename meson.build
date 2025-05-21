project(
  'ccq', 'c',
  version : '0.2.0',
  default_options : [
    'warning_level=3',
    'werror=true',
    'optimization=2',
    'c_std=c11',
    'prefix=/usr/local'
  ]
)

sources = files(
  'main.c'
)

conf_data = configuration_data()

ccq_datadir = join_paths(get_option('prefix'), get_option('datadir'), 'ccq')
conf_data.set_quoted('CCQ_DATA_DIR', ccq_datadir) 

generated_config_h = configure_file(
  input : 'config.h.in',    
  output : 'config.h',
  configuration : conf_data
)

data_to_install = files(
  'db',     
  'sample' 
)

c_compiler = meson.get_compiler('c')
math_dep = c_compiler.find_library('m', required : true) 

ccq_exe = executable(
  'ccq',
  sources, 
  dependencies : [math_dep],
  install : true
)

install_data(
  data_to_install,
  install_dir : ccq_datadir
)

install_man(
  'ccq.1',
  'ccq.5',  
  'ccq.7'
)
