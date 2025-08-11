#
# Add files for fault-line
#
FILE(GLOB_RECURSE SOURCE_FILES "lib/*.c")
FILE(GLOB_RECURSE HEADER_FILES "include/*.h")

set(SOURCES ${SOURCE_FILES} ${HEADER_FILES})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")

#
# Compile options
#
add_compile_options(-g)
add_compile_options(-Wall)
add_compile_options(-Werror)
add_compile_options(-std=c17)
add_compile_options(-D__USE_ISOC11)
add_compile_options(-D_GNU_SOURCE)
add_compile_options(-Wno-deprecated)
add_compile_options(-Wno-deprecated-declarations)
add_compile_options(-Wunused)
add_compile_options(-Wunused-result)

#
# Build fault-line shared library
#
add_library(fl_shared SHARED ${SOURCES})
set_target_properties(fl_shared PROPERTIES OUTPUT_NAME fl LINKER_LANGUAGE C VERSION ${VERSION_STRING}
                               SOVERSION ${VERSION_MAJOR})
target_link_libraries(fl_shared PUBLIC)
install(TARGETS fl_shared DESTINATION ${CMAKE_INSTALL_LIBDIR}/)

#
# Build fault-line
#
add_library(fl_static STATIC ${SOURCES})
set_target_properties(fl_static PROPERTIES OUTPUT_NAME fl LINKER_LANGUAGE C VERSION ${VERSION_STRING}
                               SOVERSION ${VERSION_MAJOR})
target_link_libraries(fl_static PUBLIC)
install(TARGETS fl_static DESTINATION ${CMAKE_INSTALL_LIBDIR}/)