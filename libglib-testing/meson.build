libglib_testing_api_version = '0'
libglib_testing_api_name = 'glib-testing-' + libglib_testing_api_version
libglib_testing_sources = [
  'dbus-queue.c',
  'signal-logger.c',
]
libglib_testing_headers = [
  'dbus-queue.h',
  'signal-logger.h',
]

libglib_testing_public_deps = [
  dependency('gio-2.0', version: '>= 2.44'),
  dependency('glib-2.0', version: '>= 2.44'),
  dependency('gobject-2.0', version: '>= 2.44'),
]

libglib_testing_include_subdir = join_paths(libglib_testing_api_name, 'libglib-testing')

# FIXME: https://github.com/mesonbuild/meson/issues/2992
if meson.is_subproject()
  libglib_testing = static_library(libglib_testing_api_name,
    libglib_testing_sources + libglib_testing_headers,
    dependencies: libglib_testing_public_deps,
    include_directories: root_inc,
    install: not meson.is_subproject(),
    version: meson.project_version(),
    soversion: libglib_testing_api_version,
  )
else
  libglib_testing = library(libglib_testing_api_name,
    libglib_testing_sources + libglib_testing_headers,
    dependencies: libglib_testing_public_deps,
    include_directories: root_inc,
    install: not meson.is_subproject(),
    version: meson.project_version(),
    soversion: libglib_testing_api_version,
  )
endif

libglib_testing_dep = declare_dependency(
  link_with: libglib_testing,
  include_directories: root_inc,
)

# Public library bits.
if not meson.is_subproject()
  install_headers(libglib_testing_headers,
    subdir: libglib_testing_include_subdir,
  )

  pkgconfig.generate(
    libraries: [ libglib_testing ],
    subdirs: libglib_testing_api_name,
    version: meson.project_version(),
    name: 'libglib-testing',
    filebase: libglib_testing_api_name,
    description: 'libglib-testing provides test harnesses and mock classes.',
    requires: libglib_testing_public_deps,
  )
endif

subdir('docs')
subdir('tests')
