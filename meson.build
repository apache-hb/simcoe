project('simcoe', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'default_library=static',
        'cpp_eh=default',
        'cpp_rtti=false',
        'b_vscrt=static_from_buildtype',

        'warning_level=3',

        'tomlplusplus:build_lib=true',
        'tomlplusplus:default_library=static',

        'libpng:tests=disabled',
        'zlib:tests=disabled',

        'harfbuzz:cpp_std=c++20',
        'harfbuzz:warning_level=1',
        'harfbuzz:tests=disabled',
        'harfbuzz:freetype=disabled',

        'freetype2:harfbuzz=enabled',
        'freetype2:warning_level=0'
    ]
)

###
### config
###

# fuck you
add_global_arguments('-DNOMINMAX', language : 'cpp')

buildtype = get_option('buildtype')
is_release = buildtype.contains('release')
is_debug = buildtype.contains('debug')

# if we're building in release mode, we dont want render debugging
debug_render = get_option('debug-render').disable_auto_if(is_release)

# if we're building in release mode, we want a gui app
gui_app = get_option('gui-app').enable_auto_if(is_release)

# if we're building for release, we want a windows store bundle
package_app = get_option('windows-store').enable_auto_if(is_release)

message('buildtype:', buildtype)
message('render debug layers:', debug_render.allowed())
message('building as a windows gui app:', gui_app.enabled())
message('packaging for windows store deploy:', package_app.enabled())

data_dir = meson.current_source_dir()/'data'
script_dir = data_dir/'scripts'

###
### deps
###

# argo = subproject('argo').get_variable('argo') TODO: config

# windows deps
win32 = subproject('win32')

dbghelp = win32.get_variable('dbghelp')
netapi = win32.get_variable('netapi')

directx = subproject('directx').get_variable('directx')
xinput = subproject('xinput').get_variable('xinput')

# config
tomlpp = dependency('tomlplusplus')

# asset loading
stb = subproject('stb').get_variable('stb')
zlib = dependency('zlib')
libpng = dependency('libpng')

# font loading
harfbuzz = dependency('harfbuzz')
freetype2 = dependency('freetype2')

# threading
threads = dependency('threads')
moodycamel = subproject('atomic-queue').get_variable('queue')

# vendor
gdk = subproject('gdk').get_variable('gdk')
ryzen = subproject('ryzen').get_variable('ryzen')

###
### args
###

config = configuration_data()
config.set10('DEBUG_ENGINE', is_debug)
config.set10('DEBUG_RENDER', debug_render.allowed())

services = get_option('services')

config.set10('SERVICE_DEBUG', is_debug)
config.set10('SERVICE_LOGGING', true)
config.set10('SERVICE_RYZEN', services.contains('ryzenmonitor'))
config.set10('SERVICE_GDK', services.contains('gdk'))

###
### engine
###

args = [
    '/wd4324' # disable warning C4324: structure was padded due to alignment specifier
              # we want this because of UNIFORM_BUFFER
]

src = [
    # config
    configure_file(output : 'simcoe-config.h', configuration : config),

    # core
    'engine/src/core/panic.cpp',
    'engine/src/core/utf8.cpp',
    'engine/src/core/bitmap.cpp',
    'engine/src/core/strings.cpp',
    'engine/src/core/units.cpp',

    # config
    'engine/src/config/config.cpp',
    'engine/src/config/schema.cpp',
    'engine/src/config/source.cpp',
    'engine/src/config/toml.cpp',

    # service
    'engine/src/service/service.cpp',

    # os
    'engine/src/service/platform.cpp',
    'engine/src/service/debug.cpp',

    # logging
    'engine/src/log/service.cpp',
    'engine/src/log/message.cpp',
    'engine/src/log/sinks.cpp',

    # util
    'engine/src/util/retry.cpp',
    'engine/src/util/time.cpp',

    # input
    'engine/src/input/input.cpp',
    'engine/src/input/win32-device.cpp',
    'engine/src/input/xinput-device.cpp',

    # threads
    'engine/src/threads/service.cpp',
    'engine/src/threads/queue.cpp',
    'engine/src/threads/exclude.cpp',
    'engine/src/threads/thread.cpp',
    'engine/src/threads/scheduler.cpp',

    # assets
    'engine/src/service/freetype.cpp',
    'engine/src/assets/assets.cpp',
    'engine/src/assets/font.cpp',

    # rhi
    'engine/src/rhi/render.cpp',
    'engine/src/rhi/vendor.cpp',

    # render graph
    'engine/src/render/render.cpp',
    'engine/src/render/graph.cpp',
    'engine/src/render/assets.cpp',

    # vendor
    'vendor/src/microsoft/gdk.cpp',
    'vendor/src/amd/ryzen.cpp',
    'vendor/src/amd/devices.cpp'
]

inc = [
    'engine/include',
    'vendor/include'
]

deps = [
    # debug
    dbghelp,

    # input
    xinput,

    # threads
    moodycamel, threads,

    # assets
    stb, harfbuzz, freetype2, tomlpp,

    # render
    directx,

    # vendor
    gdk, ryzen, netapi
]

libengine = library('engine', src,
    include_directories : inc,
    dependencies : deps,
    cpp_args : args
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : inc,
    dependencies : deps,
    compile_args : args
)

###
### editor
###

tinyobj = subproject('tinyobj').get_variable('tinyobj')
# fastgltf = subproject('fastgltf').get_variable('fastgltf')

fs = import('fs')
python3 = find_program('python3')

hlsl_vs = generator(python3,
    output : '@BASENAME@.vs.cso',
    arguments : [ script_dir/'compile-hlsl.py', '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'vs', '6_0' ]
)

hlsl_ps = generator(python3,
    output : '@BASENAME@.ps.cso',
    arguments : [ script_dir/'compile-hlsl.py', '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'ps', '6_0' ]
)

# hlsl_cs = generator(python3,
#     output : '@BASENAME@.cs.cso',
#     arguments : [ script_dir/'compile-hlsl.py', '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'cs', '6_0' ]
# )

copy = generator(python3,
    output : '@PLAINNAME@',
    arguments : [ script_dir/'copy-asset.py', '@INPUT@', '@OUTPUT@' ]
)

# TODO: really need a depot config soon
fs.copyfile('data/config/MicrosoftGame.Config')

src = [
    'editor/src/main.cpp',

    # dear imgui
    'editor/src/imgui/imgui.cpp',
    'editor/src/imgui/imgui_draw.cpp',
    'editor/src/imgui/imgui_widgets.cpp',
    'editor/src/imgui/imgui_demo.cpp',
    'editor/src/imgui/imgui_tables.cpp',

    # dear imgui freetype
    'editor/src/imgui/misc/imgui_freetype.cpp',

    # dear imgui backends
    'editor/src/imgui/backends/imgui_impl_win32.cpp',
    'editor/src/imgui/backends/imgui_impl_dx12.cpp',

    # dear imgui extensions
    'editor/src/implot/implot.cpp',
    'editor/src/implot/implot_items.cpp',
    'editor/src/implot/implot_demo.cpp',

    # render graph objects
    'editor/src/editor/graph/assets.cpp',
    'editor/src/editor/graph/mesh/obj.cpp',

    # render graph passes
    'editor/src/editor/graph/post.cpp',
    'editor/src/editor/graph/scene.cpp',
    'editor/src/editor/graph/gui.cpp',
    # 'editor/src/editor/graph/game.cpp',
    # 'editor/src/editor/graph/mipmap.cpp',

    # game debug
    'editor/src/editor/debug/debug.cpp',
    'editor/src/editor/debug/ryzen.cpp',
    'editor/src/editor/debug/gdk.cpp',
    'editor/src/editor/debug/engine.cpp',
    'editor/src/editor/debug/threads.cpp',
    'editor/src/editor/debug/logging.cpp',

    # game framework
    'editor/src/game/info.cpp',
    'editor/src/game/entity.cpp',
    'editor/src/game/level.cpp',
    'editor/src/game/world.cpp',

    # game rendering
    'editor/src/game/render/hud.cpp',
    'editor/src/game/render/scene.cpp',

    # shaders
    hlsl_vs.process('data/shaders/blit.hlsl'),
    hlsl_ps.process('data/shaders/blit.hlsl'),

    hlsl_vs.process('data/shaders/object.hlsl'),
    hlsl_ps.process('data/shaders/object.hlsl'),

    #hlsl_cs.process('data/shaders/mipmap.hlsl'),

    # fonts
    copy.process('data/fonts/SwarmFace-Regular.ttf'),

    # textures
    copy.process('data/images/assets/default.png'),
    copy.process('data/images/assets/uv-coords.png'),
    copy.process('data/images/assets/player.png'),
    copy.process('data/images/assets/cross.png'),
    copy.process('data/images/assets/alien.png'),
    copy.process('data/images/assets/death.png'),
    copy.process('data/images/assets/meme.jpg'),

    # models
    copy.process('data/models/default.model'),
    copy.process('data/models/cross.model'),
    copy.process('data/models/grid.model'),
    copy.process('data/models/ship.model'),
    copy.process('data/models/alien.model'),
    copy.process('data/models/plane.model'),

    copy.process('data/models/bullet.model'),
    copy.process('data/models/egg-small.model'),
    copy.process('data/models/egg-medium.model'),
    copy.process('data/models/egg-large.model'),

    # libraries
    copy.process('data/libs/warp/d3d10warp.dll'),
    copy.process('data/libs/warp/d3d10warp.pdb'),

    copy.process('data/libs/agility/D3D12Core.dll'),
    copy.process('data/libs/agility/D3D12Core.pdb'),
    copy.process('data/libs/agility/d3d12SDKLayers.dll'),
    copy.process('data/libs/agility/d3d12SDKLayers.pdb'),

    # config
    copy.process('data/config/MicrosoftGame.Config'),

    # gdk assets
    copy.process('data/images/store/logo44x44.png'),
    copy.process('data/images/store/logo100x100.png'),
    copy.process('data/images/store/logo150x150.png'),
    copy.process('data/images/store/logo480x480.png'),
    copy.process('data/images/store/splash.png'),

    # imgui config
    copy.process('data/config/imgui.ini')
]

deps = [
    # general
    engine,

    # render
    tinyobj
]

editor = executable('editor', src,
    dependencies : deps,
    include_directories : [ 'editor/include' ],
    cpp_args : [ '-DEDITOR=1', '/wd4127' ], # disable warning C4127: conditional expression is constant
    gui_app : gui_app.enabled()
)

###
### client
###

src = [ 'client/src/main.cpp' ]

client = executable('client', src,
    dependencies : engine,
    include_directories : [ 'client/include' ],
    cpp_args : [ '-DCLIENT=1' ],
    gui_app : gui_app.enabled()
)

if package_app.enabled()
    # mkdir bundle
    # makepkg pack /f data/config/layout.xml /lt /d build /nogameos /pc /pd bundle
    # wdapp install bundle\32190Simcoe.Swarm1983_1.0.0.0_x64__7vy8vpnrb1vvr.msixvc /bootstrapper
endif
