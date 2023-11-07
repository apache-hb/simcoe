project('ryzenmonitor', 'cpp')

if not (host_machine.system() == 'windows' and host_machine.cpu_family() == 'x86_64')
    error('amds ryzenmonitor sdk only supports windows x86_64 machines')
endif

cc = meson.get_compiler('cpp')
libdir = meson.current_source_dir()/'lib'

platform = cc.find_library('platform', dirs : libdir)
device = cc.find_library('device', dirs : libdir)

ryzenmonitor_dep = declare_dependency(
    include_directories : include_directories('include'),
    dependencies : [ platform, device ],
    compile_args : [ '/wd4201' ] # nonstandard extension used: nameless struct/union
)

meson.override_dependency('ryzenmonitor', ryzenmonitor_dep)
