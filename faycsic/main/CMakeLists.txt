message(STATUS "Compiling SRCS: ${SRCS}")
idf_component_register(
SRCS 
    "hash.c" 
    "main.c"
INCLUDE_DIRS "."
)