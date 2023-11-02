project('stb', 'c')

src = [ 'src/stb_image.c', 'src/stb_truetype.c' ]

libstb = library('stb', src,
    include_directories : [ 'include' ],
    c_args : [ '/wd4244' ] # disable warning C4244: 'initializing': conversion from 'int' to 'unsigned char', possible loss of data
)

stb = declare_dependency(
    include_directories : [ 'include' ],
    link_with : [ libstb ]
)
