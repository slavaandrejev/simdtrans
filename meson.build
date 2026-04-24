project(
    'simdtrans'
  , ['c', 'cpp']
  , version : '1.0'
  , meson_version : '>= 1.2'
  , default_options: {
        'cpp_std': 'gnu++23'
      , 'c_std':   'gnu18'
      }
  )

compiler = meson.get_compiler('c')

if get_option('buildtype') == ('debug')
    add_project_arguments('-DDEBUG', language: ['c', 'cpp'])
endif
if get_option('buildtype') == 'release'
    add_project_arguments('-DNDEBUG', language: ['c', 'cpp'])
endif

fmt_dep = dependency('fmt')
pfc_dep = compiler.find_library('pfc')

executable(
    'mtxtrans'
  , 'mtxtrans.cpp'
  , 'simdtrans.s'
  , dependencies : [
        fmt_dep
      , pfc_dep
      ]
  )
