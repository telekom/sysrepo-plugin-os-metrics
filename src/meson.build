cxx = meson.get_compiler('cpp')

libyang = dependency('libyang', version: '>=2.0')
libyang_cpp = dependency('libyang-cpp', version: '>=alpha')
libsysrepo = dependency('sysrepo', version: '>=2.0')
libsysrepo_cpp = dependency('sysrepo-cpp', version: '>=alpha')
libprocps = dependency('libprocps')
if not libprocps.found()
    libprocps = cxx.find_library('libprocps', required : true)
endif

thread_dep = dependency('threads')

find_program('df', required : true)

inc = include_directories('utils')
shared_library('dt-metrics-plugin', 'dt_metrics_plugin.cc',
                include_directories : inc,
                dependencies : [libyang, libyang_cpp, libsysrepo, libsysrepo_cpp, libprocps, thread_dep],
                install : true,
                install_dir : get_option('prefix'))