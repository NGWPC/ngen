include(${PROJECT_SOURCE_DIR}/cmake/dynamic_sourced_library.cmake)
dynamic_sourced_cxx_library(core "${CMAKE_CURRENT_SOURCE_DIR}")

# parallel_utils.cpp is built by src/utilities as NGen::parallel.
# Do not also compile it into core, or the final ngen link gets duplicate symbols.
get_target_property(_core_sources core SOURCES)
if(_core_sources)
    list(FILTER _core_sources EXCLUDE REGEX ".*/parallel_utils\\.cpp$")
    list(FILTER _core_sources EXCLUDE REGEX ".*parallel_utils\\.cpp$")
    set_target_properties(core PROPERTIES SOURCES "${_core_sources}")
endif()

add_library(NGen::core ALIAS core)
target_link_libraries(core PUBLIC
                           NGen::config_header
                           Boost::boost                # Headers-only Boost
                           )

target_link_libraries(core PRIVATE
                           NGen::parallel
                           )

target_link_libraries(core PUBLIC
                           ewts::ewts_ngen_bridge
                           NGen::core_mediator
                           )

target_include_directories(core PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/simulation_time
        ${PROJECT_SOURCE_DIR}/include/core
        ${PROJECT_SOURCE_DIR}/include/core/catchment
        ${PROJECT_SOURCE_DIR}/include/core/nexus
        ${PROJECT_SOURCE_DIR}/include/core/hydrolocation
        ${PROJECT_SOURCE_DIR}/include/core/mediator
        ${PROJECT_SOURCE_DIR}/include/realizations/catchment
        ${PROJECT_SOURCE_DIR}/include/geojson
        ${PROJECT_SOURCE_DIR}/include/forcing
        ${PROJECT_SOURCE_DIR}/include/utilities
        ${PROJECT_SOURCE_DIR}/include/realizations/catchment
        ${PROJECT_SOURCE_DIR}/include/bmi
        )

if (NGEN_WITH_PYTHON)
    target_link_libraries(core PUBLIC NGen::python)
endif ()

if(NGEN_WITH_MPI)
    target_link_libraries(core PUBLIC MPI::MPI_C MPI::MPI_CXX)
endif()

add_subdirectory("catchment")
add_subdirectory("nexus")
add_subdirectory("hydrolocation")
add_subdirectory("mediator")
