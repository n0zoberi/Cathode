project('cathode', 'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>= 1.0.0',
  default_options: [
    'c_std=c11',
    'warning_level=3',
    'optimization=2',
  ],
)

gtk4_dep       = dependency('gtk4',       version: '>= 4.12')
adwaita_dep    = dependency('libadwaita-1', version: '>= 1.4')
vte_dep        = dependency('vte-2.91-gtk4', version: '>= 0.74')
epoxy_dep      = dependency('epoxy',      version: '>= 1.5')
glib_dep       = dependency('glib-2.0',   version: '>= 2.76')

subdir('src')
