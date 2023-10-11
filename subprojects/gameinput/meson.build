project('gameinput', 'cpp')

here = meson.current_source_dir()

gameinput = declare_dependency(
    include_directories : 'include',
    link_args : [ here/'lib'/'GameInput.lib', here/'lib'/'xgameruntime.lib' ]
)
