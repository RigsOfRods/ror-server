find_program(PAGECOMPILER_EXE cpspc HINTS ${PAGECOMPILER_DIR} REQUIRED)



macro(PageCompiler file outfile)
    get_filename_component(out ${outfile} DIRECTORY)

if (WIN32)
    set(PC_COMMAND ${PAGECOMPILER_EXE} /output-dir ${out} ${file})
else ()
    set(PC_COMMAND ${PAGECOMPILER_EXE} --output-dir ${out} ${file})
endif ()

    add_custom_command(
            OUTPUT ${outfile}
            COMMENT "Compiling ${file}"
            DEPENDS ${file}
            COMMAND ${PC_COMMAND}
    )
endmacro()