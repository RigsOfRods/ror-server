FILE(GLOB_RECURSE angelscript_src  source/*.cpp)
FILE(GLOB_RECURSE angelscript_include  include/*.h)

FILE(GLOB_RECURSE angelscript_add_on_dict    add_on/scriptdictionary/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_math    add_on/scriptmath/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_math3d  add_on/scriptmath3d/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_array  add_on/scriptarray/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_stdstring  add_on/scriptstdstring/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_builder  add_on/scriptbuilder/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_any  add_on/scriptany/*.cpp)

include_directories (include)
add_definitions("-DANGELSCRIPT_EXPORT -D_LIB")

add_library(angelscript STATIC ${angelscript_src} ${angelscript_include} ${angelscript_add_on_dict} ${angelscript_add_on_math} ${angelscript_add_on_math3d} ${angelscript_add_on_array} ${angelscript_add_on_stdstring} ${angelscript_add_on_builder} ${angelscript_add_on_any})
