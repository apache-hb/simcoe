project('gameinput', 'cpp')

here = meson.current_source_dir()

gdk = declare_dependency(
    include_directories : 'include',
    link_args : [ here/'lib'/'GameInput.lib', here/'lib'/'xgameruntime.lib' ]
)
