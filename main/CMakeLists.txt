set(srcs "main.c"
         "led.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS ".")
