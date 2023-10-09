project('stb', 'c')

src = [ 'src/stb_image.c', 'src/stb_truetype.c' ]

libstb = library('stb', src,
    include_directories : [ 'include' ]
)

stb = declare_dependency(
    include_directories : [ 'include' ],
    link_with : [ libstb ]
)
