# Declares the window backend selector before install_dependencies() evaluates
# the "condition" fields in dependencies.json. The deps system auto-includes a
# subdir's dependencies.cmake (if present) ahead of reading its dependencies.json,
# so this runs early enough that LCF_WINDOW_BACKEND is in the cache when the
# conditions are tested.
#
# Only seed the default when nothing has set it yet (preset cacheVariables, a -D
# override, or a prior cache entry all take precedence). An unconditional
# set(... CACHE ...) is a no-op once the entry exists, but guarding on
# DEFINED keeps the intent explicit and avoids fighting a caller-chosen value.
if(NOT DEFINED LCF_WINDOW_BACKEND)
    set(LCF_WINDOW_BACKEND "SDL3" CACHE STRING "window backend: SDL3 | GLFW")
endif()
set_property(CACHE LCF_WINDOW_BACKEND PROPERTY STRINGS SDL3 GLFW)
