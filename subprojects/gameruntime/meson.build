project('gameruntime', 'cpp')

if not (host_machine.system() == 'windows' and host_machine.cpu_family() == 'x86_64')
    error('microsoft gameruntime only supports windows x86_64 machines')
endif

cc = meson.get_compiler('cpp')
libdir = meson.current_source_dir()/'lib'

gameinput = cc.find_library('GameInput', dirs : libdir)
xgameruntime = cc.find_library('xgameruntime', dirs : libdir)

gameruntime_dep = declare_dependency(
    include_directories : include_directories('include'),
    dependencies : [ gameinput, xgameruntime ]
)

meson.override_dependency('gameruntime', gameruntime_dep)
