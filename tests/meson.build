test_cc = meson.get_compiler('c')
test_m_dep = test_cc.find_library('m', required: false)
test_cairo_dep = dependency('cairo', version: '>= 1.16')

test_config = executable('test-config',
  'test-config.c',
  '../src/config.c',
  '../src/vendor/tomlc99/toml.c',
  include_directories: include_directories('../src'),
  dependencies: [glib_dep, test_m_dep],
  c_args: ['-DGETTEXT_PACKAGE="cathode"'],
)
test('config', test_config)

test_shader_active = executable('test-shader-active',
  'test-shader-active.c',
  '../src/shader.c',
  '../src/config.c',
  '../src/vendor/tomlc99/toml.c',
  include_directories: include_directories('../src'),
  dependencies: [
    gtk4_dep,
    vte_dep,
    epoxy_dep,
    test_cairo_dep,
    test_m_dep,
    glib_dep,
  ],
  c_args: ['-DGETTEXT_PACKAGE="cathode"'],
)
test('shader-active', test_shader_active)
