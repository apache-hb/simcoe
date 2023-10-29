project('argo', 'cpp')

src = [
    'argo/json.cpp',
    'argo/utf8.cpp',
    'argo/lexer.cpp',
    'argo/reader.cpp',
    'argo/stream_reader.cpp',
    'argo/fd_reader.cpp',
    'argo/file_reader.cpp',
    'argo/writer.cpp',
    'argo/stream_writer.cpp',
    'argo/fd_writer.cpp',
    'argo/file_writer.cpp',
    'argo/token.cpp',
    'argo/json_exception.cpp',
    'argo/parser.cpp',
    'argo/unparser.cpp',
    'argo/json_io_exception.cpp',
    'argo/json_parser_exception.cpp',
    'argo/json_utf8_exception.cpp',
    'argo/json_array_index_range_exception.cpp',
    'argo/json_pointer_exception.cpp',
    'argo/json_invalid_key_exception.cpp',
    'argo/pointer.cpp'
]

libargo = library('argo', src,
    include_directories : 'argo'
)

argo = declare_dependency(
    link_with : libargo,
    include_directories : '.'
)