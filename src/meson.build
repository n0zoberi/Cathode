cathode_sources = [
  'main.c',
  'app.c',
  'terminal.c',
]

cathode_deps = [
  gtk4_dep,
  adwaita_dep,
  vte_dep,
  epoxy_dep,
  glib_dep,
]

gnome = import('gnome')

cathode_resources = gnome.compile_resources(
  'cathode-resources',
  'cathode.gresource.xml',
  c_name: 'cathode',
)

executable('cathode',
  cathode_sources + cathode_resources,
  dependencies: cathode_deps,
  install: true,
  install_dir: get_option('bindir'),
)
