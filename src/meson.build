cxx = meson.get_compiler('cpp')

libyang = dependency('libyang')
libyang_cpp = dependency('libyang-cpp')
libsysrepo = dependency('sysrepo')
libsysrepo_cpp = dependency('sysrepo-cpp')
libprocps = dependency('libprocps')
if not libprocps.found()
    libprocps = cxx.find_library('libprocps', required : true)
endif

thread_dep = dependency('threads')

inc = include_directories('utils')
shared_library('dt-metrics-plugin', 'dt_metrics_plugin.cc',
                include_directories : inc,
                dependencies : [libyang, libyang_cpp, libsysrepo, libsysrepo_cpp, libprocps, thread_dep],
                install : true,
                install_dir : get_option('prefix'),
                override_options : ['cpp_std=c++17'])