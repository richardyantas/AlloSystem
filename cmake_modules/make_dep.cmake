
macro(make_dep DEP_NAME)
  set(${DEP_NAME}_RELATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../${DEP_NAME})
  string(TOUPPER "${DEP_NAME}" DEP_NAME_UPPER)
  if(EXISTS "${${DEP_NAME}_RELATIVE_DIR}" AND IS_DIRECTORY "${${DEP_NAME}_RELATIVE_DIR}")
    if (EXISTS "${${DEP_NAME}_RELATIVE_DIR}/CMakeLists.txt")
      message(STATUS "Allosystem: ${DEP_NAME} sources with CMakeLists found. Building ${DEP_NAME}.")
	  set(BUILDING_${DEP_NAME_UPPER} 1)
      add_subdirectory(${${DEP_NAME}_RELATIVE_DIR} ${${DEP_NAME}_RELATIVE_DIR}/AlloSystem-build)
	  set(${DEP_NAME_UPPER}_INCLUDE_DIR ${${DEP_NAME}_RELATIVE_DIR})
	  get_target_property(${DEP_NAME_UPPER}_LIBRARY ${DEP_NAME} LOCATION)
      message(STATUS "${DEP_NAME} library: ${${DEP_NAME}_LIBRARY}")
    else()
      message(STATUS "Allosystem: Trying make for ${DEP_NAME}.")
      execute_process(COMMAND make WORKING_DIRECTORY "${${DEP_NAME}_RELATIVE_DIR}"
        RESULT_VARIABLE make_result)
      if (NOT make_result EQUAL 0)
        message("Allosystem: ${DEP_NAME} make failed. Can't build ${DEP_NAME}.")
      else()
        find_package(${DEP_NAME})
      endif(NOT make_result EQUAL 0)
    endif()
  else()
    message("Allosystem: ${DEP_NAME} sources NOT found. Can't build ${DEP_NAME}.")
  endif()
endmacro()