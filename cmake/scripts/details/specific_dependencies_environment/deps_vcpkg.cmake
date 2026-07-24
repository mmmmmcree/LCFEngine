# deps_vcpkg.cmake — install dependencies via vcpkg manifest mode.
#
# Reads each dep's "name" plus optional "version", "features", and
# "default-features". When "version" is set, we pin via vcpkg's overrides
# (which requires a `builtin-baseline`, synthesized from the active vcpkg's
# HEAD/embeddedsha). "features" and "default-features" are forwarded into the
# synthesized vcpkg.json's dependency entry.
#
# Structure: all platform/compiler/env-specific routing lives in the private
# helpers below. The public deps_install_vcpkg() consumes only normalized
# variables (VCPKG_ROOT_DIR, VCPKG_TARGET_TRIPLET, VCPKG_EXE, VCPKG_BASELINE)
# and otherwise contains no platform conditionals.

# --- Helpers: platform / environment / vcpkg-deployment routing ----------------

# Resolve which vcpkg installation to use. Order:
#   1. -DVCPKG_ROOT_OVERRIDE=<path>           (explicit project override)
#   2. $ENV{VCPKG_ROOT}, unless it's VS-bundled (read-only snapshot under VS).
#   3. Windows persistent VCPKG_ROOT — User scope first (HKCU\Environment),
#      then Machine scope (HKLM\...\Environment). Both bypass VS Dev Cmd's
#      process-env hijack; this is the value the user set in System
#      Properties → Environment Variables.
#   4. The hijacked process VCPKG_ROOT (last-resort fallback).
#   5. vcpkg(.exe) on PATH; its containing directory.
function(_vcpkg_locate_root OUT_VAR)
    if(VCPKG_ROOT_OVERRIDE AND EXISTS "${VCPKG_ROOT_OVERRIDE}")
        set(${OUT_VAR} "${VCPKG_ROOT_OVERRIDE}" PARENT_SCOPE)
        return()
    endif()

    set(_proc "$ENV{VCPKG_ROOT}")
    set(_is_bundled FALSE)
    if(_proc AND EXISTS "${_proc}/vcpkg-bundle.json")
        set(_is_bundled TRUE)
    endif()
    if(_proc AND NOT _is_bundled)
        set(${OUT_VAR} "${_proc}" PARENT_SCOPE)
        return()
    endif()

    # CMAKE_HOST_WIN32 (instead of WIN32): this helper may run pre-project()
    # for the toolchain auto-setup below, and WIN32 is only set after
    # enable_language. CMAKE_HOST_WIN32 is set unconditionally.
    if(CMAKE_HOST_WIN32)
        foreach(_scope IN ITEMS User Machine)
            execute_process(
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
                        "[System.Environment]::GetEnvironmentVariable('VCPKG_ROOT','${_scope}')"
                OUTPUT_VARIABLE _persistent_root
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_persistent_root AND EXISTS "${_persistent_root}")
                if(_is_bundled)
                    message(STATUS
                        "vcpkg: process VCPKG_ROOT='${_proc}' is the VS-bundled "
                        "deployment (read-only). Using ${_scope}-scope VCPKG_ROOT "
                        "'${_persistent_root}' so all presets share one vcpkg.")
                endif()
                set(${OUT_VAR} "${_persistent_root}" PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endif()

    if(_proc)
        set(${OUT_VAR} "${_proc}" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_HOST_WIN32)
        find_program(_v vcpkg.exe)
    else()
        find_program(_v vcpkg)
    endif()
    if(_v)
        get_filename_component(_dir "${_v}" DIRECTORY)
        set(${OUT_VAR} "${_dir}" PARENT_SCOPE)
        return()
    endif()

    set(${OUT_VAR} "" PARENT_SCOPE)
endfunction()

# Resolve the default triplet. Must work BOTH post-project() (MINGW and
# CMAKE_CXX_COMPILER_ID are populated) AND pre-project() — _vcpkg_setup() calls
# this before project() to seed VCPKG_TARGET_TRIPLET in the cache before the
# vcpkg toolchain runs. The toolchain's own `elseif(MINGW)` detection is useless
# that early: MINGW is only set after compiler identification, so the toolchain
# would default to x64-windows and FORCE-cache it, locking out the mingw triplet
# (and failing with "Unable to find a valid Visual Studio instance"). Pre-
# project we therefore infer "Windows GNU/MinGW" from the requested compiler.
function(_vcpkg_default_triplet OUT_VAR)
    set(_gnu_win FALSE)
    if(MINGW OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32))
        set(_gnu_win TRUE)
    elseif(CMAKE_HOST_WIN32 AND NOT CMAKE_CXX_COMPILER_ID)
        # pre-project(): no compiler-id yet — sniff the configured compiler name.
        get_filename_component(_cxx "${CMAKE_CXX_COMPILER}" NAME_WE)
        get_filename_component(_cc  "${CMAKE_C_COMPILER}"   NAME_WE)
        if(_cxx MATCHES "(^|-)(g\\+\\+|gcc)$" OR _cc MATCHES "(^|-)gcc$"
           OR _cxx MATCHES "mingw" OR _cc MATCHES "mingw")
            set(_gnu_win TRUE)
        endif()
    endif()

    if(_gnu_win)
        set(${OUT_VAR} "x64-mingw-dynamic" PARENT_SCOPE)
    elseif(WIN32 OR CMAKE_HOST_WIN32)
        set(${OUT_VAR} "x64-windows"       PARENT_SCOPE)
    elseif(APPLE)
        set(${OUT_VAR} "x64-osx"           PARENT_SCOPE)
    else()
        set(${OUT_VAR} "x64-linux"         PARENT_SCOPE)
    endif()
endfunction()

# Resolve baseline SHA: git rev-parse → vcpkg-bundle.json embeddedsha → .git/HEAD.
function(_vcpkg_read_baseline VCPKG_ROOT_DIR OUT_VAR)
    set(${OUT_VAR} "" PARENT_SCOPE)

    execute_process(
        COMMAND git -C "${VCPKG_ROOT_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _sha
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _rc
        ERROR_QUIET
    )
    if(_rc EQUAL 0 AND _sha)
        set(${OUT_VAR} "${_sha}" PARENT_SCOPE)
        return()
    endif()

    set(_bundle "${VCPKG_ROOT_DIR}/vcpkg-bundle.json")
    if(EXISTS "${_bundle}")
        file(READ "${_bundle}" _json)
        string(JSON _sha ERROR_VARIABLE _e GET "${_json}" embeddedsha)
        if(NOT _e AND _sha)
            set(${OUT_VAR} "${_sha}" PARENT_SCOPE)
            return()
        endif()
    endif()

    set(_head "${VCPKG_ROOT_DIR}/.git/HEAD")
    if(NOT EXISTS "${_head}")
        return()
    endif()
    file(READ "${_head}" _content)
    string(STRIP "${_content}" _content)
    if(_content MATCHES "^ref: (.+)$")
        set(_ref "${CMAKE_MATCH_1}")
        set(_loose "${VCPKG_ROOT_DIR}/.git/${_ref}")
        if(EXISTS "${_loose}")
            file(READ "${_loose}" _sha)
            string(STRIP "${_sha}" _sha)
            set(${OUT_VAR} "${_sha}" PARENT_SCOPE)
            return()
        endif()
        set(_packed "${VCPKG_ROOT_DIR}/.git/packed-refs")
        if(EXISTS "${_packed}")
            file(STRINGS "${_packed}" _lines)
            foreach(_line IN LISTS _lines)
                if(_line MATCHES "^([0-9a-f]+) ${_ref}$")
                    set(${OUT_VAR} "${CMAKE_MATCH_1}" PARENT_SCOPE)
                    return()
                endif()
            endforeach()
        endif()
    elseif(_content MATCHES "^[0-9a-fA-F]+$")
        set(${OUT_VAR} "${_content}" PARENT_SCOPE)
    endif()
endfunction()

# Render a single dependency JSON entry. Use vcpkg's short string form only
# when neither features nor default-features was specified; otherwise preserve
# both fields in the object form.
function(_vcpkg_render_dep_entry DEPS_JSON INDEX OUT_VAR)
    string(JSON _name GET "${DEPS_JSON}" dependencies ${INDEX} name)

    set(_fields "\"name\": \"${_name}\"")
    set(_needs_object FALSE)

    string(JSON _default_features ERROR_VARIABLE _e_default_features
           GET "${DEPS_JSON}" dependencies ${INDEX} default-features)
    if(NOT _e_default_features)
        set(_needs_object TRUE)
        if(_default_features)
            list(APPEND _fields "\"default-features\": true")
        else()
            list(APPEND _fields "\"default-features\": false")
        endif()
    endif()

    string(JSON _feats_arr ERROR_VARIABLE _e_feat
           GET "${DEPS_JSON}" dependencies ${INDEX} features)
    if(NOT _e_feat)
        string(JSON _f_count LENGTH "${_feats_arr}")
        if(_f_count GREATER 0)
            set(_needs_object TRUE)
            math(EXPR _f_last "${_f_count} - 1")
            set(_feats "")
            foreach(_fi RANGE 0 ${_f_last})
                string(JSON _f GET "${_feats_arr}" ${_fi})
                list(APPEND _feats "\"${_f}\"")
            endforeach()
            list(JOIN _feats ", " _feats_csv)
            list(APPEND _fields "\"features\": [ ${_feats_csv} ]")
        endif()
    endif()

    if(NOT _needs_object)
        set(${OUT_VAR} "    \"${_name}\"" PARENT_SCOPE)
        return()
    endif()

    list(JOIN _fields ", " _fields_csv)
    set(${OUT_VAR} "    { ${_fields_csv} }" PARENT_SCOPE)
endfunction()

# --- Main: from here on, only normalized variables are used. -----------------

function(deps_install_vcpkg DEPS_JSON)
    # 1) Normalize all platform/env-specific inputs into uniform variables.
    _vcpkg_locate_root(VCPKG_ROOT_DIR)
    if(NOT VCPKG_ROOT_DIR)
        message(FATAL_ERROR
            "DEPS_BACKEND=vcpkg but no vcpkg installation found. Set "
            "VCPKG_ROOT (User-scope on Windows so it survives VS Dev Cmd), "
            "or pass -DVCPKG_ROOT_OVERRIDE=<path>.")
    endif()
    set(ENV{VCPKG_ROOT} "${VCPKG_ROOT_DIR}")

    if(NOT VCPKG_TARGET_TRIPLET)
        _vcpkg_default_triplet(_t)
        set(VCPKG_TARGET_TRIPLET "${_t}" CACHE STRING "vcpkg target triplet")
    endif()

    if(WIN32)
        find_program(VCPKG_EXE vcpkg.exe HINTS "${VCPKG_ROOT_DIR}" NO_DEFAULT_PATH)
    else()
        find_program(VCPKG_EXE vcpkg     HINTS "${VCPKG_ROOT_DIR}" NO_DEFAULT_PATH)
    endif()
    if(NOT VCPKG_EXE)
        message(FATAL_ERROR "no vcpkg executable under '${VCPKG_ROOT_DIR}'")
    endif()

    _vcpkg_read_baseline("${VCPKG_ROOT_DIR}" VCPKG_BASELINE)
    if(NOT VCPKG_BASELINE)
        message(FATAL_ERROR
            "vcpkg requires a builtin-baseline but couldn't resolve one for "
            "'${VCPKG_ROOT_DIR}'.")
    endif()

    # 2) Build aggregate manifest from DEPS_JSON.
    set(_install_root "${CMAKE_BINARY_DIR}/vcpkg_installed")
    set(_blds_root    "${_install_root}/vcpkg/blds")
    set(_pkgs_root    "${_install_root}/vcpkg/pkgs")
    set(_manifest_dir "${CMAKE_BINARY_DIR}/_vcpkg_aggregate")

    string(JSON _len LENGTH "${DEPS_JSON}" dependencies)
    math(EXPR _last "${_len} - 1")

    set(_dep_entries "")
    set(_override_entries "")
    foreach(_i RANGE 0 ${_last})
        _vcpkg_render_dep_entry("${DEPS_JSON}" ${_i} _entry)
        list(APPEND _dep_entries "${_entry}")

        string(JSON _name GET "${DEPS_JSON}" dependencies ${_i} name)
        string(JSON _ver  ERROR_VARIABLE _e_ver
               GET "${DEPS_JSON}" dependencies ${_i} version)
        if(NOT _e_ver AND _ver)
            list(APPEND _override_entries
                "    { \"name\": \"${_name}\", \"version\": \"${_ver}\" }")
        endif()
    endforeach()
    list(JOIN _dep_entries ",\n" _deps_block)

    set(_overrides_field "")
    if(_override_entries)
        list(JOIN _override_entries ",\n" _ovr_block)
        set(_overrides_field ",\n  \"overrides\": [\n${_ovr_block}\n  ]")
    endif()

    file(MAKE_DIRECTORY "${_manifest_dir}")
    file(WRITE "${_manifest_dir}/vcpkg.json"
"{
  \"name\": \"aggregate\",
  \"version-string\": \"0.0.0\",
  \"builtin-baseline\": \"${VCPKG_BASELINE}\",
  \"dependencies\": [
${_deps_block}
  ]${_overrides_field}
}
")

    # 3) When DEPS_KEEP_SOURCES is ON, surgically remove any top-level dep
    #    whose buildtree source is empty (typical after a binary-cache hit on
    #    a previous configure). vcpkg's "already installed" check would
    #    otherwise skip the install and leave blds/<port>/src/ empty.
    if(DEPS_KEEP_SOURCES)
        foreach(_i RANGE 0 ${_last})
            string(JSON _pname GET "${DEPS_JSON}" dependencies ${_i} name)
            file(GLOB _src "${_blds_root}/${_pname}/src/*")
            if(NOT _src AND IS_DIRECTORY "${_install_root}/vcpkg/info")
                message(STATUS "vcpkg: removing ${_pname} to force "
                               "from-source re-extraction (DEPS_KEEP_SOURCES=ON)")
                execute_process(
                    COMMAND "${VCPKG_EXE}" remove
                            "--x-install-root=${_install_root}"
                            "--triplet=${VCPKG_TARGET_TRIPLET}"
                            "--recurse" "${_pname}"
                )
            endif()
        endforeach()
    endif()

    # 4) Run vcpkg install. Default behavior: let vcpkg use its global
    #    buildtrees/packages dirs so it can clean them up post-install (and
    #    binary cache hits stay clean — no extracted source under build/).
    #    With DEPS_KEEP_SOURCES=ON, pin both under the install root for
    #    per-preset isolation and pass --no-binarycaching so the build is
    #    forced to extract source.
    # Pin the host triplet to the target. Otherwise vcpkg defaults the host
    # triplet to its detected host (x64-windows on Windows), so any dep with a
    # host-tool dependency forces an x64-windows build — which needs Visual
    # Studio and fails in a pure-MinGW environment. Matching host=target builds
    # those tools with the same toolchain we're already using.
    set(_install_args
        "--x-manifest-root=${_manifest_dir}"
        "--x-install-root=${_install_root}"
        "--triplet=${VCPKG_TARGET_TRIPLET}"
        "--host-triplet=${VCPKG_TARGET_TRIPLET}"
        "--feature-flags=manifests,versions"
    )
    if(DEPS_KEEP_SOURCES)
        list(APPEND _install_args
            "--x-buildtrees-root=${_blds_root}"
            "--x-packages-root=${_pkgs_root}"
            "--no-binarycaching"
        )
    endif()

    execute_process(
        COMMAND "${VCPKG_EXE}" install ${_install_args}
        RESULT_VARIABLE _rc
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "vcpkg install failed (rc=${_rc})")
    endif()

    list(APPEND CMAKE_PREFIX_PATH "${_install_root}/${VCPKG_TARGET_TRIPLET}")

    # vcpkg ports that ship CMake "Find<X>.cmake" modules (rather than full
    # <X>Config.cmake) install them under share/<port>/. The toolchain (when
    # loaded as toolchainFile in top-level CMakeLists) handles these via its
    # find_package wrapper, but we add to CMAKE_MODULE_PATH explicitly so the
    # script-only path (no toolchain) still works. Examples: Stb.
    file(GLOB_RECURSE _find_modules
        "${_install_root}/${VCPKG_TARGET_TRIPLET}/share/Find*.cmake")
    foreach(_fm IN LISTS _find_modules)
        get_filename_component(_d "${_fm}" DIRECTORY)
        list(APPEND CMAKE_MODULE_PATH "${_d}")
    endforeach()
    if(CMAKE_MODULE_PATH)
        list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)
    endif()

    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()

# --- Backend interface: detect / setup ---------------------------------------
#
# deps.cmake calls _vcpkg_detect() during auto-detection, then (if vcpkg is
# selected) calls _vcpkg_setup() exactly once *before* project(). _vcpkg_setup
# is what installs the official toolchain via CMAKE_TOOLCHAIN_FILE so that
# project()/enable_language() picks up vcpkg's add_executable / add_library
# overrides — that's the per-target applocal-deploy hook the runtime needs.
#
# By keeping these as functions invoked from deps.cmake (instead of running at
# module scope), we (a) avoid duplicate root-resolution between auto-detection
# and install, and (b) keep the file pure-declaration: include order doesn't
# trigger side effects.

function(_vcpkg_detect OUT_VAR)
    _vcpkg_locate_root(_root)
    if(_root AND EXISTS "${_root}/scripts/buildsystems/vcpkg.cmake")
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Pre-project hook: install vcpkg's official toolchain so project() loads it.
# Idempotent and a no-op when an outer toolchain is already in play.
function(_vcpkg_setup)
    if(CMAKE_TOOLCHAIN_FILE)
        return()
    endif()
    _vcpkg_locate_root(_root)
    if(NOT _root OR NOT EXISTS "${_root}/scripts/buildsystems/vcpkg.cmake")
        return()
    endif()
    # Seed the triplet BEFORE the toolchain loads. The toolchain FORCE-caches
    # whatever it computes — and pre-compiler-id that's x64-windows, since its
    # MINGW branch can't fire yet — which would shadow our mingw default. Set it
    # first and the toolchain honours it (vcpkg.cmake short-circuits on
    # `if(VCPKG_TARGET_TRIPLET)`). An explicit user/preset override still wins.
    if(NOT VCPKG_TARGET_TRIPLET)
        _vcpkg_default_triplet(_t)
        set(VCPKG_TARGET_TRIPLET "${_t}" CACHE STRING "vcpkg target triplet")
    endif()
    set(CMAKE_TOOLCHAIN_FILE
        "${_root}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "vcpkg toolchain (auto-detected by deps_vcpkg.cmake)")
    set(VCPKG_INSTALLED_DIR    "${CMAKE_BINARY_DIR}/vcpkg_installed"
        CACHE PATH "" FORCE)
    # We install manually via deps_install_vcpkg(), so the toolchain
    # must NOT auto-install from a root vcpkg.json (we don't have one).
    set(VCPKG_MANIFEST_MODE    OFF CACHE BOOL "" FORCE)
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "" FORCE)
    # Per-target applocal-deploy is the whole reason we load the toolchain.
    set(VCPKG_APPLOCAL_DEPS    ON  CACHE BOOL "" FORCE)
endfunction()

list(APPEND _DEPS_BACKENDS "vcpkg")
