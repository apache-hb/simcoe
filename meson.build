project('simcoe', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'default_library=static',
        'b_vscrt=static_from_buildtype'
    ]
)

###
### config
###

is_release = get_option('buildtype').startswith('release')

###
### deps
###

directx = subproject('directx').get_variable('directx')
xinput = subproject('xinput').get_variable('xinput')
gdk = subproject('gdk').get_variable('gdk')
stb = subproject('stb').get_variable('stb')
moodycamel = subproject('atomic-queue').get_variable('queue')
threads = dependency('threads')

###
### engine
###

src = [
    'engine/src/engine.cpp',

    # os
    'engine/src/os/system.cpp',
    'engine/src/os/timer.cpp',

    # input
    'engine/src/input/input.cpp',
    'engine/src/input/win32-device.cpp',
    'engine/src/input/xinput-device.cpp',

    # memory
    'engine/src/memory/bitmap.cpp',

    # tasks
    'engine/src/tasks/task.cpp',

    # assets
    'engine/src/assets/assets.cpp',

    # rhi
    'engine/src/rhi/render.cpp',
    'engine/src/rhi/vendor.cpp',

    # render graph
    'engine/src/render/render.cpp',
    'engine/src/render/graph.cpp',
    'engine/src/render/assets.cpp',

    # vendor
    'vendor/src/microsoft/gdk.cpp'
]

inc = [
    'engine/include',
    'vendor/include'
]

deps = [
    # input
    xinput, gdk,

    # tasks
    moodycamel, threads,

    # assets
    stb,

    # render
    directx
]

libengine = library('engine', src,
    include_directories : inc,
    dependencies : deps
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : inc,
    dependencies : deps
)

###
### editor
###

tinyobj = subproject('tinyobj').get_variable('tinyobj')
fastgltf = subproject('fastgltf').get_variable('fastgltf')

python3 = find_program('python3')

data_dir = meson.current_source_dir()/'data'
script_dir = data_dir/'scripts'

hlsl = generator(python3,
    output : [ '@BASENAME@.ps.cso', '@BASENAME@.vs.cso' ],
    arguments : [ script_dir/'compile-hlsl.py', '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'vs,ps', '6_0' ]
)

copy = generator(python3,
    output : '@PLAINNAME@',
    arguments : [ script_dir/'copy-asset.py', '@INPUT@', '@OUTPUT@' ]
)

custom_target('config',
    output : 'MicrosoftGame.Config',
    input : 'data/config/MicrosoftGame.Config',
    command : [ python3, script_dir/'copy-asset.py', '@INPUT@', '@OUTPUT@' ],
    build_by_default : true
)

src = [
    'editor/src/main.cpp',

    # dear imgui
    'editor/src/imgui/imgui.cpp',
    'editor/src/imgui/imgui_draw.cpp',
    'editor/src/imgui/imgui_widgets.cpp',
    'editor/src/imgui/imgui_demo.cpp',
    'editor/src/imgui/imgui_tables.cpp',

    # dear imgui backends
    'editor/src/imgui/backends/imgui_impl_win32.cpp',
    'editor/src/imgui/backends/imgui_impl_dx12.cpp',

    # render graph objects
    'editor/src/editor/graph/assets.cpp',
    'editor/src/editor/graph/post.cpp',
    'editor/src/editor/graph/scene.cpp',
    'editor/src/editor/graph/gui.cpp',
    'editor/src/editor/graph/game.cpp',

    # game objects
    'editor/src/editor/game/obj.cpp',
    'editor/src/editor/game/gltf.cpp',

    # game debug
    'editor/src/editor/debug/gui.cpp',

    # shaders
    hlsl.process('data/shaders/blit.hlsl'),
    hlsl.process('data/shaders/object.hlsl'),

    # textures
    copy.process('data/images/assets/uv-coords.png'),
    copy.process('data/images/assets/player.png'),
    copy.process('data/images/assets/cross.png'),
    copy.process('data/images/assets/alien.png'),

    # models
    copy.process('data/models/cross.model'),
    copy.process('data/models/grid.model'),
    copy.process('data/models/ship.model'),
    copy.process('data/models/alien.model'),

    copy.process('data/models/bullet.model'),
    copy.process('data/models/egg-small.model'),
    copy.process('data/models/egg-medium.model'),
    copy.process('data/models/egg-large.model'),

    # libraries
    copy.process('data/libs/warp/d3d10warp.dll'),
    copy.process('data/libs/warp/d3d10warp.pdb'),

    # config
    copy.process('data/config/MicrosoftGame.Config'),

    # gdk assets
    copy.process('data/images/store/logo44x44.png'),
    copy.process('data/images/store/logo100x100.png'),
    copy.process('data/images/store/logo150x150.png'),
    copy.process('data/images/store/logo480x480.png'),
    copy.process('data/images/store/splash.png'),
]

deps = [
    # general
    engine,

    # render
    tinyobj, fastgltf,
]

executable('editor', src,
    dependencies : deps,
    include_directories : [ 'editor/include' ],
    gui_app : is_release
)

###
### client
###

src = [
    'client/src/main.cpp'
]

executable('client', src,
    dependencies : engine,
    include_directories : [ 'client/include' ]
)

if is_release
    layout = data_dir/'config/layout.xml'
    # mkdir bundle
    # makepkg pack /f data/config/layout.xml /lt /d build /nogameos /pc /pd bundle
    # wdapp install bundle\32190Simcoe.Swarm1983_1.0.0.0_x64__7vy8vpnrb1vvr.msixvc /bootstrapper
endif
