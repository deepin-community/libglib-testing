# FIXME: Would be good to eliminate version.xml generation if possible. See:
# https://github.com/mesonbuild/meson/issues/3581
version_conf = configuration_data()
version_conf.set('LIBGLIB_TESTING_VERSION', meson.project_version())

configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: version_conf,
)

gnome.gtkdoc('libglib-testing',
  mode: 'none',
  main_xml: 'docs.xml',
  src_dir: [
    include_directories('..'),
  ],
  dependencies: libglib_testing_dep,
  scan_args: [
    '--ignore-decorators=G_GNUC_WARN_UNUSED_RESULT',
    '--ignore-headers=' + ' '.join(['tests']),
  ],
  install: not meson.is_subproject(),
)