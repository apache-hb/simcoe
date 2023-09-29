project('simcoe', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'default_library=static'
    ]
)

###
### deps
###

directx = subproject('directx').get_variable('directx')


###
### engine
###

src = [
    'engine/src/engine.cpp',

    # os
    'engine/src/os/system.cpp',

    # assets
    'engine/src/assets/assets.cpp',

    # render
    'engine/src/render/render.cpp'
]

libengine = library('engine', src,
    include_directories : [ 'engine/include' ],
    dependencies : [ directx ]
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : [ 'engine/include' ],
    dependencies : [ directx ]
)

###
### editor
###

python3 = import('python').find_installation()

here = meson.current_source_dir()
script = here/'data/scripts/compile-hlsl.py'

hlsl = generator(python3,
    output : [ '@BASENAME@.ps.cso', '@BASENAME@.vs.cso' ],
    arguments : [ script, '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'vs,ps', '6_0' ]
)

shaders = [ hlsl.process('data/shaders/triangle.hlsl') ]

src = [ 'editor/src/main.cpp' ]

executable('editor', src + shaders,
    dependencies : engine,
    include_directories : [ 'editor/include' ]
)

###
### client
###

executable('client', 'client/src/main.cpp',
    dependencies : engine,
    include_directories : [ 'client/include' ]
)
