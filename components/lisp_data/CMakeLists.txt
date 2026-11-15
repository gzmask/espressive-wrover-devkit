idf_component_register(SRCS "lisp_json.c" INCLUDE_DIRS "include")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -fstack-usage -Wframe-larger-than=768)
