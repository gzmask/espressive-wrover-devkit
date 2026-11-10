include_guard(GLOBAL)
set(LISPBM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.local/lispbm" CACHE PATH "Unmodified pinned LispBM checkout")
set(WROVER_LISPBM_COMMIT "cbb30151bcb167c1035c16907b45ba1cb8a40e46")
function(wrover_lispbm_sources output)
    find_package(Git REQUIRED)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${LISPBM_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE result)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${LISPBM_SOURCE_DIR}" status --porcelain --untracked-files=no
        OUTPUT_VARIABLE dirty OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE status_result)
    if(NOT result EQUAL 0 OR NOT status_result EQUAL 0 OR
       NOT revision STREQUAL WROVER_LISPBM_COMMIT OR NOT dirty STREQUAL "")
        message(FATAL_ERROR "Use unmodified LispBM ${WROVER_LISPBM_COMMIT}; see docs/LISP_REPL.md")
    endif()
    set(sources)
    foreach(name env eval_cps extensions fundamental heap lbm_memory print stack symrepr tokpar
                 lispbm lbm_c_interop lbm_custom_type lbm_channel lbm_flat_value lbm_defrag_mem lbm_image)
        list(APPEND sources "${LISPBM_SOURCE_DIR}/src/${name}.c")
    endforeach()
    set(${output} "${sources}" PARENT_SCOPE)
endfunction()
