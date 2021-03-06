#
#   Copyright 2017 Garrett D'Amore <garrett@damore.org>
#   Copyright 2017 Capitar IT Group BV <info@capitar.com>
#   Copyright (c) 2012 Martin Sustrik  All rights reserved.
#   Copyright (c) 2013 GoPivotal, Inc.  All rights reserved.
#   Copyright (c) 2015-2016 Jack R. Dunaway. All rights reserved.
#   Copyright 2016 Franklin "Snaipe" Mathieu <franklinmathieu@gmail.com>
#   Copyright 2017 Garrett D'Amore <garrett@damore.org>
#
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom
#   the Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#   IN THE SOFTWARE.
#

cmake_minimum_required (VERSION 3.1)

project (nng C)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckLibraryExists)
include (CheckCSourceCompiles)
include (GNUInstallDirs)

if (POLICY CMP0042)
    # Newer cmake on MacOS should use @rpath
    cmake_policy (SET CMP0042 NEW)
endif ()

set(CMAKE_C_STANDARD 99)
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list (FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" isSystemDir)
if ("${isSystemDir}" STREQUAL "-1")
    set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endif ("${isSystemDir}" STREQUAL "-1")

set (NNG_DESCRIPTION "High-Performance Scalability Protocols NextGen")
set (ISSUE_REPORT_MSG "Please consider opening an issue at https://github.com/nanomsg/nng")

# Determine library versions.
set (NNG_ABI_VERSION "0.0.0")

# Determine package version.
find_package (Git QUIET)
if (DEFINED ENV{TRAVIS_TAG})
    set (NNG_PACKAGE_VERSION "$ENV{TRAVIS_TAG}")
elseif (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Working off a git repo, using git versioning

    # Get version from last tag
    execute_process (
        COMMAND             "${GIT_EXECUTABLE}" describe --always# | sed -e "s:v::"
        WORKING_DIRECTORY   "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE     NNG_PACKAGE_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    # If the sources have been changed locally, add -dirty to the version.
    execute_process (
        COMMAND             "${GIT_EXECUTABLE}" diff --quiet
        WORKING_DIRECTORY   "${PROJECT_SOURCE_DIR}"
        RESULT_VARIABLE     res)
    if (res EQUAL 1)
        set (NNG_PACKAGE_VERSION "${NNG_PACKAGE_VERSION}-dirty")
    endif()

elseif (EXISTS ${PROJECT_SOURCE_DIR}/.version)
    #  If git is not available (e.g. when building from source package)
    #  we can extract the package version from .version file.
    file (STRINGS .version NNG_PACKAGE_VERSION)
else ()
    set (NNG_PACKAGE_VERSION "Unknown")
endif()

# User-defined options.

#option (NNG_STATIC_LIB "Build static library instead of shared library." OFF)
option (NNG_ENABLE_DOC "Enable building documentation." ON)
option (NNG_TESTS "Build and run tests" ON)
option (NNG_TOOLS "Build extra tools" OFF)
option (NNG_ENABLE_NNGCAT "Enable building nngcat utility." ${NNG_TOOLS})
option (NNG_ENABLE_COVERAGE "Enable coverage reporting." OFF)
option (NNG_ENABLE_ZEROTIER "Enable ZeroTier transport (requires libzerotiercore)." OFF)
set (NNG_ZEROTIER_SOURCE "" CACHE PATH "Location of ZeroTier source tree.")
# Enable access to private APIs for our own use.
add_definitions (-DNNG_PRIVATE)

#  Platform checks.

if (NNG_ENABLE_COVERAGE)
    # NB: This only works for GCC and Clang 3.0 and newer.  If your stuff
    # is older than that, you will need to find something newer.  For
    # correct reporting, we always turn off all optimizations.
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_C_FLAGS "-g -O0 --coverage")
        set(CMAKE_CXX_FLAGS "-g -O0 --coverage")
    elseif (CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_C_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
        set(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
    elseif (CMAKE_COMPILER_ID STREQUAL "AppleClang")
        set(CMAKE_C_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
        set(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
    else()
        message(FATAL_ERROR "Unable to enable coverage for your compiler.")
    endif()
endif()

find_package (Threads REQUIRED)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_definitions (-DNNG_PLATFORM_POSIX)
    add_definitions (-DNNG_PLATFORM_LINUX)
    add_definitions (-DNNG_USE_EVENTFD)
    set(NNG_PLATFORM_POSIX ON)

elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    add_definitions (-DNNG_PLATFORM_POSIX)
    add_definitions (-DNNG_PLATFORM_POSIX)
    # macOS 10.12 and later have getentropy, but the older releases
    # have ARC4_RANDOM, and that is sufficient to our needs.
    add_definitions (-DNNG_USE_ARC4_RANDOM)

    # macOS added some of CLOCK_MONOTONIC, but the implementation is
    # broken and unreliable, so don't use it.
    add_definitions (-DNNG_USE_CLOCKID=CLOCK_REALTIME)
    set(NNG_PLATFORM_POSIX ON)

elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    add_definitions (-DNNG_PLATFORM_POSIX)
    add_definitions (-DNNG_PLATFORM_FREEBSD)
    set(NNG_PLATFORM_POSIX ON)

elseif (CMAKE_SYSTEM_NAME MATCHES "NetBSD")
    add_definitions (-DNNG_PLATFORM_POSIX)
    add_definitions (-DNNG_PLATFORM_NETBSD)
    set(NNG_PLATFORM_POSIX ON)

elseif (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    add_definitions (-DNNG_PLATFORM_POSIX)
    add_definitions (-DNNG_PLATFORM_OPENBSD)
    set(NNG_PLATFORM_POSIX ON)

elseif (CMAKE_SYSTEM_NAME MATCHES "SunOS")
    add_definitions (-DNNG_PLATFORM_POSIX)
    add_definitions (-DNNG_PLATFORM_SUNOS)

elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_definitions (-DNNG_PLATFORM_WINDOWS)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_CRT_RAND_S)
    set(NNG_PLATFORM_WINDOWS ON)

    # Target Windows Vista and later
    add_definitions (-D_WIN32_WINNT=0x0600)
    list (APPEND CMAKE_REQUIRED_DEFINITIONS -D_WIN32_WINNT=0x0600)

else ()
    message (AUTHOR_WARNING "WARNING: This platform may not be supported: ${CMAKE_SYSTEM_NAME}")
    message (AUTHOR_WARNING "${ISSUE_REPORT_MSG}")
    # blithely hope for POSIX to work
    find_package (Threads REQUIRED)
    add_definitions (-DPLATFORM_POSIX)
endif ()

if (NNG_STATIC_LIB)
    add_definitions (-DNNG_DECL=extern)
endif ()

macro (nng_check_func SYM DEF)
    check_function_exists (${SYM} ${DEF})
    if (${DEF})
        add_definitions (-D${DEF}=1)
    endif ()
endmacro (nng_check_func)

macro (nng_check_sym SYM HDR DEF)
    check_symbol_exists (${SYM} ${HDR} ${DEF})
    if (${DEF})
        add_definitions (-D${DEF}=1)
    endif ()
endmacro (nng_check_sym)

macro (nng_check_lib LIB SYM DEF)
    check_library_exists (${LIB} ${SYM} "" ${DEF})
    if (${DEF})
        add_definitions (-D${DEF}=1)
        set(NNG_REQUIRED_LIBRARIES ${NNG_REQUIRED_LIBRARIES} ${LIB})
    endif ()
endmacro (nng_check_lib)

macro (nng_check_struct_member STR MEM HDR DEF)
    check_struct_has_member ("struct ${STR}" ${MEM} ${HDR} ${DEF})
    if (${DEF})
        add_definitions (-D${DEF}=1)
    endif ()
endmacro (nng_check_struct_member)

if (WIN32)
    # Windows is a special snowflake.
    set(NNG_REQUIRED_LIBRARIES ${NNG_REQUIRED_LIBRARIES} ws2_32)
    set(NNG_REQUIRED_LIBRARIES ${NNG_REQUIRED_LIBRARIES} mswsock)
    set(NNG_REQUIRED_LIBRARIES ${NNG_REQUIRED_LIBRARIES} advapi32)
    nng_check_sym (InitializeConditionVariable windows.h NNG_HAVE_CONDVAR)
    nng_check_sym (snprintf stdio.h NNG_HAVE_SNPRINTF)
    if (NOT NNG_HAVE_CONDVAR OR NOT NNG_HAVE_SNPRINTF)
        message (FATAL_ERROR
	    "Modern Windows API support is missing. "
	    "Versions of Windows prior to Vista are not supported.  "
	    "Further, the 32-bit MinGW environment is not supported. "
	    "Ensure you have at least Windows Vista or newer, and are "
	    "using either Visual Studio 2013 or newer or MinGW-W64.")
    endif()
else ()
    # Unconditionally declare the following feature test macros.  These are
    # needed for some platforms (glibc and SunOS/illumos) and are harmless
    # on the others.
    add_definitions (-D_GNU_SOURCE)
    add_definitions (-D_REENTRANT)
    add_definitions (-D_THREAD_SAFE)
    add_definitions (-D_POSIX_PTHREAD_SEMANTICS)

    nng_check_lib (rt clock_gettime  NNG_HAVE_CLOCK_GETTIME)
    nng_check_lib (pthread sem_wait  NNG_HAVE_SEMAPHORE_PTHREAD)
    nng_check_lib (nsl gethostbyname NNG_HAVE_LIBNSL)
    nng_check_lib (socket socket NNG_HAVE_LIBSOCKET)

    nng_check_sym (AF_UNIX sys/socket.h NNG_HAVE_UNIX_SOCKETS)
    nng_check_sym (backtrace_symbols_fd execinfo.h NNG_HAVE_BACKTRACE)
    nng_check_struct_member(msghdr msg_control sys/socket.h NNG_HAVE_MSG_CONTROL)
endif ()

nng_check_sym (strdup string.h NNG_HAVE_STRDUP)
nng_check_sym (strlcat string.h NNG_HAVE_STRLCAT)
nng_check_sym (strlcpy string.h NNG_HAVE_STRLCPY)
nng_check_sym (strnlen string.h NNG_HAVE_STRNLEN)

# Search for ZeroTier
# We use the libzerotiercore.a library, which is unfortunately a C++ object
# even though it exposes only public C symbols.  It would be extremely
# helpful if libzerotiercore didn't make us carry the whole C++ runtime
# behind us.  The user must specify the location of the ZeroTier source
# tree (dev branch for now, and already compiled please) by setting the
# NNG_ZEROTIER_SOURCE macro.
# NB: This needs to be the zerotierone tree, not the libzt library.
# This is because we don't access the API, but instead use the low
# level zerotiercore functionality directly.
# NB: As we wind up linking libzerotiercore.a into the application,
# this means that your application will *also* need to either be licensed
# under the GPLv3, or you will need to have a commercial license from
# ZeroTier permitting its use elsewhere.
if (NNG_ENABLE_ZEROTIER)
    enable_language(CXX)
    find_library(NNG_LIBZTCORE zerotiercore PATHS ${NNG_ZEROTIER_SOURCE})
    if (NNG_LIBZTCORE)
        set(CMAKE_REQUIRED_INCLUDES ${NNG_ZEROTIER_SOURCE}/include)
#        set(CMAKE_REQUIRED_LIBRARIES ${NNG_LIBZTCORE} c++)
#        set(NNG_REQUIRED_LIBRARIES ${NNG_REQUIRED_LIBRARIES} ${NNG_LIBZTCORE} c++)
        message(STATUS "C++ ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES}")
        set(CMAKE_REQUIRED_LIBRARIES ${NNG_LIBZTCORE} ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
        set(NNG_REQUIRED_LIBRARIES ${NNG_REQUIRED_LIBRARIES} ${NNG_LIBZTCORE} ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
        set(NNG_REQUIRED_INCLUDES ${NNG_REQUIRED_INCLUDES} ${NNG_ZEROTIER_SOURCE}/include)
        nng_check_sym(ZT_Node_join ZeroTierOne.h NNG_HAVE_ZEROTIER)
    endif()
    if (NOT NNG_HAVE_ZEROTIER)
        message (FATAL_ERROR "Cannot find ZeroTier components")
    endif()
    message(STATUS "Found ZeroTier at ${NNG_LIBZTCORE}")
endif()

add_subdirectory (src)

if (NNG_TESTS)
	enable_testing()
	set(all_tests, "")
	add_subdirectory (tests)
    add_subdirectory (perf)
endif()


#  Build the tools

if (NNG_ENABLE_NNGCAT)
    add_executable (nanocat tools/nngcat.c tools/options.c)
    target_link_libraries (nanocat ${PROJECT_NAME})
endif ()

if (NNG_ENABLE_DOC)
    find_program (ASCIIDOCTOR_EXE asciidoctor)
    if (NOT ASCIIDOCTOR_EXE)
        message (WARNING "Could not find asciidoctor: skipping docs")
        set (NNG_ENABLE_DOC OFF)
    else ()
        message (STATUS "Using asciidoctor at ${ASCIIDOCTOR_EXE}")
    endif ()
endif ()


# Build the documenation
if (NNG_ENABLE_DOC)

    set (NNG_DOCDIR ${CMAKE_CURRENT_SOURCE_DIR}/doc)
    set (NNG_STYLESHEET ${NNG_DOCDIR}/stylesheet.css)
    set (NNG_TITLE ${PROJECT_NAME} ${NNG_PACKAGE_VERSION})
    set (NNG_A2M ${ASCIIDOCTOR_EXE} -b manpage -amanmanual='${NNG_TITLE}')
    set (NNG_A2H ${ASCIIDOCTOR_EXE} -d manpage -b html5 -a stylesheeet=${NNG_STYLESHEET} -aversion-label=${PROJECT_NAME} -arevnumber=${NNG_PACKAGE_VERSION})

    macro (add_libnng_man NAME SECT)
        add_custom_command (
            OUTPUT ${NAME}.${SECT}
            COMMAND ${NNG_A2M} -o ${NAME}.${SECT} ${NNG_DOCDIR}/${NAME}.adoc
            MAIN_DEPENDENCY ${NNG_DOCDIR}/${NAME}.adoc
        )

        add_custom_command (
            OUTPUT ${NAME}.html
            COMMAND ${NNG_A2H} -o ${NAME}.html ${NNG_DOCDIR}/${NAME}.adoc
            DEPENDS ${NNG_STYLESHEET}
            MAIN_DEPENDENCY ${NNG_DOCDIR}/${NAME}.adoc
        )

        set(NNG_MANS ${NNG_MANS} ${NAME}.${SECT})
        set(NNG_HTMLS ${NNG_HTMLS} ${NAME}.html)

        install (
            FILES ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.html
            DESTINATION ${CMAKE_INSTALL_DOCDIR}
        )
        install (
            FILES ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.${SECT}
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man${SECT}
        )

    endmacro (add_libnng_man)

    if (NNG_ENABLE_NNGCAT)
        add_libnng_man (nngcat 1)
    endif ()

    #add_libnng_man (nn_errno 3)
    #add_libnng_man (nn_strerror 3)
    #add_libnng_man (nn_symbol 3)
    #add_libnng_man (nn_symbol_info 3)
    #add_libnng_man (nn_allocmsg 3)
    #add_libnng_man (nn_reallocmsg 3)
    #add_libnng_man (nn_freemsg 3)
    #add_libnng_man (nn_socket 3)
    #add_libnng_man (nn_close 3)
    #add_libnng_man (nn_get_statistic 3)
    #add_libnng_man (nn_getsockopt 3)
    #add_libnng_man (nn_setsockopt 3)
    #add_libnng_man (nn_bind 3)
    #add_libnng_man (nn_connect 3)
    #add_libnng_man (nn_shutdown 3)
    #add_libnng_man (nn_send 3)
    #add_libnng_man (nn_recv 3)
    #add_libnng_man (nn_sendmsg 3)
    #add_libnng_man (nn_recvmsg 3)
    #add_libnng_man (nn_device 3)
    #add_libnng_man (nn_cmsg 3)
    #add_libnng_man (nn_poll 3)
    #add_libnng_man (nn_term 3)

    #add_libnng_man (nanomsg 7)
    #add_libnng_man (nn_pair 7)
    #add_libnng_man (nn_reqrep 7)
    #add_libnng_man (nn_pubsub 7)
    #add_libnng_man (nn_survey 7)
    #add_libnng_man (nn_pipeline 7)
    #add_libnng_man (nn_bus 7)
    #add_libnng_man (nn_inproc 7)
    #add_libnng_man (nn_ipc 7)
    #add_libnng_man (nn_tcp 7)
    #add_libnng_man (nn_ws 7)
    #add_libnng_man (nn_env 7)

    add_custom_target (man ALL DEPENDS ${NNG_MANS})
    add_custom_target (html ALL DEPENDS ${NNG_HTMLS})

endif ()

install (TARGETS LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install (TARGETS ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install (FILES src/nng.h DESTINATION include/nng)

if (NNG_ENABLE_NNGCAT)
    install (TARGETS nngcat RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

set (CPACK_PACKAGE_NAME ${PROJECT_NAME})
set (CPACK_PACKAGE_VERSION ${NNG_PACKAGE_VERSION})
set (CPACK_SOURCE_GENERATOR "TBZ2;TGZ;ZIP")
set (CPACK_SOURCE_IGNORE_FILES "/build/;/.git/;~$;${CPACK_SOURCE_IGNORE_FILES}")
set (CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${NNG_PACKAGE_VERSION}")
add_custom_target(dist COMMAND ${CMAKE_MAKE_PROGRAM} package_source)
include (CPack)
