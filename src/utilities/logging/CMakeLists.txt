add_library(logging logging_utils.cpp)
add_library(NGen::logging ALIAS logging)
target_include_directories(logging PUBLIC ${PROJECT_SOURCE_DIR}/include/utilities)

target_include_directories(logging
    PUBLIC
        ${NGEN_INC_DIR}
)

if(USE_EWTS)
    target_link_libraries(logging PUBLIC NGen::ewts)
endif()

target_link_libraries(logging PUBLIC
Boost::boost
)
