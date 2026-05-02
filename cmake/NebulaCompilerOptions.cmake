function(nebula_target_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /permissive- /EHsc)
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
            -Woverloaded-virtual)
    endif()
endfunction()

function(nebula_target_optimizations target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/GL>
            $<$<CONFIG:Release>:/fp:precise>)
        target_link_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:/LTCG>)
    else()
        target_compile_options(${target_name} PRIVATE
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Release>:-fno-math-errno>
            $<$<CONFIG:Release>:-ffunction-sections>
            $<$<CONFIG:Release>:-fdata-sections>)
        target_link_options(${target_name} PRIVATE
            $<$<AND:$<CONFIG:Release>,$<NOT:$<PLATFORM_ID:Darwin>>>:-Wl,--gc-sections>
            $<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Darwin>>:-Wl,-dead_strip>)
    endif()
endfunction()
