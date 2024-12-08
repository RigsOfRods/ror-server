FILE(GLOB_RECURSE angelscript_add_on_any scriptany/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_array scriptarray/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_builder scriptbuilder/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_dict scriptdictionary/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_math scriptmath/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_math3d scriptmath3d/*.cpp)
FILE(GLOB_RECURSE angelscript_add_on_stdstring scriptstdstring/*.cpp)

add_library(angelscript_addons STATIC ${angelscript_add_on_dict} ${angelscript_add_on_math} ${angelscript_add_on_math3d} ${angelscript_add_on_array} ${angelscript_add_on_stdstring} ${angelscript_add_on_builder} ${angelscript_add_on_any})

target_link_libraries(angelscript_addons PRIVATE Angelscript::angelscript)

# ignore warnings
set_target_properties(angelscript_addons PROPERTIES COMPILE_FLAGS "-w")