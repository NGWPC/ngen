cmake_minimum_required(VERSION 3.19)

# If we are configured standalone in this directory, define a minimal project so CMake is happy.
if(NOT DEFINED PROJECT_NAME)
  project(ngen_coastal LANGUAGES C CXX)
endif()

# Try to infer the repo root when configured from src/realizations/coastal
# ngen root is 3 levels up from here: ngen/src/realizations/coastal
set(NGEN_TOP "${CMAKE_CURRENT_LIST_DIR}/../../.." CACHE PATH "Path to ngen repository root")

# ---------------------------------------------------------------------------
# Bring in the helper that defines dynamic_sourced_cxx_library, if available
# When configuring from the repo root, PROJECT_SOURCE_DIR will already be the top.
# Here we use NGEN_TOP so this file also works standalone.
# ---------------------------------------------------------------------------
set(_DYNAMIC_LIB_HELPER "${NGEN_TOP}/cmake/dynamic_sourced_library.cmake")
if(EXISTS "${_DYNAMIC_LIB_HELPER}")
  include("${_DYNAMIC_LIB_HELPER}")
else()
  message(STATUS "dynamic_sourced_library.cmake not found at: ${_DYNAMIC_LIB_HELPER}")
  message(STATUS "This coastal CMakeLists is meant to be driven from the repo root.")
  message(STATUS "Continuing, but you must provide equivalent targets manually or configure from ${NGEN_TOP}")
endif()

# ---------------------------------------------------------------------------
# Library: realizations_coastal
# ---------------------------------------------------------------------------

# Only create the library via the helper if it exists
if(COMMAND dynamic_sourced_cxx_library)
  include("${NGEN_TOP}/cmake/dynamic_sourced_library.cmake")  # ensure scope
  dynamic_sourced_cxx_library(realizations_coastal "${CMAKE_CURRENT_SOURCE_DIR}")
else()
  # Fallback: collect all .cpp files in this directory tree and build a static lib
  file(GLOB_RECURSE REALIZATIONS_COASTAL_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  )
  add_library(realizations_coastal STATIC ${REALIZATIONS_COASTAL_SRC})
endif()

add_library(NGen::realizations_coastal ALIAS realizations_coastal)

# ---------------------------------------------------------------
# Toggle SCHISM coastal integration (DEFAULT: ON)
#   Disable with: -DNGEN_ENABLE_SCHISM=OFF
# ---------------------------------------------------------------
option(NGEN_ENABLE_SCHISM "Build SCHISM coastal integration" ON)

if(NOT NGEN_ENABLE_SCHISM)
  # Mark SCHISM sources as header-only so they are not compiled/linked
  set(_schism_srcs
    ${CMAKE_CURRENT_LIST_DIR}/SchismCreator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/SchismFormulation.cpp
  )
  foreach(_s IN LISTS _schism_srcs)
    if(EXISTS "${_s}")
      set_source_files_properties(${_s} PROPERTIES HEADER_FILE_ONLY TRUE)
      message(STATUS "Coastal: excluding SCHISM source from build: ${_s}")
    endif()
  endforeach()

  # Also exclude the SCHISM coastal test if present in the repo
  set(_schism_test "${NGEN_TOP}/test/coastal/SchismFormulation_Test.cpp")
  if(EXISTS "${_schism_test}")
    set_source_files_properties("${_schism_test}" PROPERTIES HEADER_FILE_ONLY TRUE)
    message(STATUS "Tests: excluding SCHISM coastal test: ${_schism_test}")
  endif()
endif()

# Public include paths
target_include_directories(realizations_coastal PUBLIC
  ${NGEN_TOP}/include/core
  ${NGEN_TOP}/include/realizations/coastal
  ${NGEN_TOP}/include/forcing
  ${NGEN_TOP}/include/simulation_time
  ${NGEN_TOP}/include/utilities
  ${NGEN_TOP}/include/coastal
  ${NGEN_TOP}/include/bmi
)

# Link to imported targets if they exist; otherwise, warn clearly when standalone
set(_missing_imports "")
foreach(_tgt NGen::config_header NGen::geojson NGen::logging NGen::ngen_bmi)
  if(NOT TARGET ${_tgt})
    list(APPEND _missing_imports ${_tgt})
  endif()
endforeach()

if(_missing_imports)
  message(STATUS "The following imported targets are missing: ${_missing_imports}")
  message(STATUS "You are likely configuring this subfolder standalone.")
  message(STATUS "For a full build, configure from the repo root: ${NGEN_TOP}")
endif()

# Always link libdl if available; conditionally link NGen targets if present
target_link_libraries(realizations_coastal PUBLIC ${CMAKE_DL_LIBS})
if(TARGET NGen::config_header)
  target_link_libraries(realizations_coastal PUBLIC NGen::config_header)
endif()
if(TARGET NGen::geojson)
  target_link_libraries(realizations_coastal PUBLIC NGen::geojson)
endif()
if(TARGET NGen::logging)
  target_link_libraries(realizations_coastal PUBLIC NGen::logging)
endif()
if(TARGET NGen::ngen_bmi)
  target_link_libraries(realizations_coastal PUBLIC NGen::ngen_bmi)
endif()

# ---------------------------------------------------------------------------
# Optional SFINCS tests (no MPI)
# Build only if we have Fortran BMI enabled at configure time and NGen targets exist
# ---------------------------------------------------------------------------
option(NGEN_BUILD_COASTAL_TESTS "Build coastal realization tests" ON)
option(NGEN_WITH_BMI_FORTRAN "Enable BMI Fortran adapter in NGen" OFF)  # default OFF if standalone

if(NGEN_BUILD_COASTAL_TESTS AND NGEN_WITH_BMI_FORTRAN)
  include(CTest)

  # Provide convenient cache vars for the SFINCS BMI lib and init file
  set(SFINCS_BMI_LIBRARY "${NGEN_TOP}/extern/SFINCS/source/src/build/libsfincs_bmi.so"
      CACHE FILEPATH "Path to libsfincs_bmi shared library")
  set(SFINCS_INIT_CONFIG "${NGEN_TOP}/extern/SFINCS/source/src/build/sfincs_config.txt"
      CACHE FILEPATH "Path to SFINCS BMI initialize() config file")

  # -------- test_sfincs_formulation_smoke (only if the test source exists) --------
  set(_sfincs_smoke_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sfincs_formulation_smoke.cpp")
  if(EXISTS "${_sfincs_smoke_src}")
    add_executable(test_sfincs_formulation_smoke "${_sfincs_smoke_src}")
    target_include_directories(test_sfincs_formulation_smoke PRIVATE
      ${NGEN_TOP}/include
    )
    target_link_libraries(test_sfincs_formulation_smoke PRIVATE
      realizations_coastal
    )
    if(TARGET NGen::ngen_bmi)
      target_link_libraries(test_sfincs_formulation_smoke PRIVATE NGen::ngen_bmi)
    endif()
    if(TARGET NGen::logging)
      target_link_libraries(test_sfincs_formulation_smoke PRIVATE NGen::logging)
    endif()

    add_test(NAME test_sfincs_formulation_smoke
      COMMAND test_sfincs_formulation_smoke
              ${SFINCS_BMI_LIBRARY}
              ${SFINCS_INIT_CONFIG})
  else()
    message(STATUS "Skipping test_sfincs_formulation_smoke: file not found at ${_sfincs_smoke_src}")
  endif()

  # -------- test_sfincs_creator_smoke (optional helper) --------
  set(_sfincs_creator_src "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sfincs_creator_smoke.cpp")
  if(EXISTS "${_sfincs_creator_src}")
    add_executable(test_sfincs_creator_smoke "${_sfincs_creator_src}")
    target_include_directories(test_sfincs_creator_smoke PRIVATE
      ${NGEN_TOP}/include
    )
    target_link_libraries(test_sfincs_creator_smoke PRIVATE
      realizations_coastal
    )
    if(TARGET NGen::ngen_bmi)
      target_link_libraries(test_sfincs_creator_smoke PRIVATE NGen::ngen_bmi)
    endif()
    if(TARGET NGen::logging)
      target_link_libraries(test_sfincs_creator_smoke PRIVATE NGen::logging)
    endif()

    add_test(NAME test_sfincs_creator_smoke
      COMMAND test_sfincs_creator_smoke
              ${SFINCS_BMI_LIBRARY}
              ${SFINCS_INIT_CONFIG})
  else()
    message(STATUS "Skipping test_sfincs_creator_smoke: file not found at ${_sfincs_creator_src}")
  endif()

endif()

