project('stb', 'c')

libstb = library('stb', 'src/stb_image.c',
    include_directories : [ 'include' ]
)

stb = declare_dependency(
    include_directories : [ 'include' ],
    link_with : [ libstb ]
)
