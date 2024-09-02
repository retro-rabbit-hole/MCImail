project('server', ['c', 'cpp'],
  default_options : [
    'c_std=c23',
    'cpp_std=c++23',
    'warning_level=2'
    ]
  )

incdir = include_directories('include')

mep2_pdu_lib_sources = [
	'src/mep2_pdu_parser.cpp',
	'src/mep2_pdu.cpp',
	'src/address.cpp',
	'src/date.cpp',
	'src/string_utils.cpp']

mep2_pdu_lib = static_library('mep2_pdu',
	mep2_pdu_lib_sources,
	include_directories : incdir)
	
executable('serverv2', 'src/serverv2.c', include_directories : incdir)

gtest_proj = subproject('gtest')
gtest_dep = gtest_proj.get_variable('gtest_dep')
gtest_main_dep = gtest_proj.get_variable('gtest_main_dep')

tests = executable('testprog', 'test/test.cpp', 
	include_directories: incdir,
	dependencies: [ gtest_dep, gtest_main_dep],
	link_with: mep2_pdu_lib,
	)
	
test('gtest test', tests)

cc = meson.get_compiler('cpp')
if cc.get_id() == 'clang'
  asan_dep = cc.find_library('asan', required : true)

  fuzz_common_config = {
    'include_directories': incdir,
    'dependencies': [asan_dep],
    'cpp_args': ['-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined,leak,fuzzer',
                 '-fprofile-instr-generate', '-fcoverage-mapping', '-DFUZZING_BUILD'],
    'link_args': ['-fsanitize=address,undefined,leak,fuzzer',
                  '-fprofile-instr-generate', '-fcoverage-mapping'],
  }

  fuzz_targets = [
    ['fuzz_parser', 'test/fuzz_parser.cpp'],
    ['fuzz_stringdecode', 'test/fuzz_stringdecode.cpp'],
	['fuzz_date', 'test/fuzz_date.cpp'],
  ]

  foreach target : fuzz_targets
    executable(target[0], 
               [target[1], mep2_pdu_lib_sources],
               kwargs: fuzz_common_config)
  endforeach
endif