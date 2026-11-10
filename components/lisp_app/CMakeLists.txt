idf_component_register(SRCS "lisp_app.c" INCLUDE_DIRS "include" REQUIRES lcd_terminal)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow)
