add_library(mdframe handler_csv.cpp handler_netcdf.cpp)
add_library(NGen::mdframe ALIAS mdframe)
target_include_directories(mdframe PUBLIC ${NGEN_INC_DIR})
target_include_directories(mdframe PUBLIC ${PROJECT_SOURCE_DIR}/include/utilities)
target_link_libraries(mdframe PUBLIC
    Boost::boost
    NGen::config_header
    NGen::mdarray
    NGen::logging
)

option(USE_EWTS "Build with EWTS logging support" ON)
if(USE_EWTS)
    target_link_libraries(mdframe PUBLIC NGen::ewts)
endif()

if(NGEN_WITH_NETCDF)
    target_link_libraries(mdframe PUBLIC NetCDF)
endif()
