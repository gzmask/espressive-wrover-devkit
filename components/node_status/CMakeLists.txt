idf_component_register(SRCS "node_status.c" INCLUDE_DIRS "include" REQUIRES json)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
