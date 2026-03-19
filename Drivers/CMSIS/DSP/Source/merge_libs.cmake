
set(MRI "CREATE ${COMBINED}\n")
foreach(lib ${LIBS})
    string(APPEND MRI "ADDLIB ${lib}\n")
endforeach()
string(APPEND MRI "SAVE\nEND\n")

get_filename_component(_mri_dir "${COMBINED}" DIRECTORY)
set(_mri_file "${_mri_dir}/merge_dsp.mri")
file(WRITE "${_mri_file}" "${MRI}")

execute_process(
    COMMAND ${AR} -M
    INPUT_FILE "${_mri_file}"
    RESULT_VARIABLE _result
)

if(_result)
    message(FATAL_ERROR "Failed to merge libraries: ${LIBS}")
else()
    message(STATUS "Merged libraries into ${COMBINED}")
endif()