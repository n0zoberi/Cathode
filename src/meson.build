cathode_sources = [
  'main.c',
  'app.c',
  'terminal.c',
  'config.c',
  'shader.c',
  'vendor/tomlc99/toml.c',
]

cathode_include = include_directories('.')

cairo_dep = dependency('cairo', version: '>= 1.16')

cathode_deps = [
  gtk4_dep,
  adwaita_dep,
  vte_dep,
  epoxy_dep,
  glib_dep,
  cairo_dep,
]

gnome = import('gnome')

cathode_resources = gnome.compile_resources(
  'cathode-resources',
  'cathode.gresource.xml',
  c_name: 'cathode',
)

executable('cathode',
  cathode_sources,
  cathode_resources,
  include_directories: cathode_include,
  dependencies: cathode_deps,
  c_args: ['-Wno-pedantic'],
  install: true,
  install_dir: get_option('bindir'),
)
