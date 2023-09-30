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
stb = subproject('stb').get_variable('stb')

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

deps = [
    # assets
    stb,

    # render
    directx
]

libengine = library('engine', src,
    include_directories : [ 'engine/include' ],
    dependencies : deps
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : [ 'engine/include' ],
    dependencies : deps
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

src = [
    'editor/src/main.cpp',

    # shaders
    hlsl.process('data/shaders/quad.hlsl'),
    hlsl.process('data/shaders/blit.hlsl')
]

executable('editor', src,
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
