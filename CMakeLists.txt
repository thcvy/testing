# =====================================================================
#  EEEE2076 - Software Engineering & VR Project (Group Task)
#
#  Build / install / package script.
#
#  Targets exposed to the IDE / command line:
#     FormulaVRviewer        - the application
#     INSTALL           - lays out a self-contained directory tree
#                         (binary + Qt + VTK + OpenVR + assets)
#     PACKAGE           - runs CPack with the NSIS generator to build
#                         a Windows installer (.exe) from that tree
#     Doxygen           - regenerates the HTML documentation in
#                         build/docs/html
# =====================================================================
cmake_minimum_required(VERSION 3.19)
project(FormulaVRviewer
    VERSION 1.0.0
    DESCRIPTION "FormulaVRviewer - VR STL viewer (EEEE2076 Group Task)"
    HOMEPAGE_URL "https://github.com/"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Qt ----
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ---- VTK ----
find_package(VTK REQUIRED)

# ---- The application ------------------------------------------------
qt_add_executable(FormulaVRviewer
    WIN32 MACOSX_BUNDLE

    main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    ModelPart.cpp
    ModelPart.h

    ModelPartList.cpp
    ModelPartList.h

    optiondialog.cpp
    optiondialog.h
    optiondialog.ui

    lightdialog.cpp
    lightdialog.h
    lightdialog.ui

    VRRenderThread.cpp
    VRRenderThread.h

    Icons.qrc
)

target_link_libraries(FormulaVRviewer
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        ${VTK_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS FormulaVRviewer
    MODULES ${VTK_LIBRARIES}
)

# ---- Build-time asset copies (so F5 / Run inside the IDE works) -----
# These keep the build tree self-contained: when you launch the binary
# from the build directory, OpenVR finds its bindings.
add_custom_target(VRBindings ALL)
add_custom_command(TARGET VRBindings PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E
        copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/vrbindings
            ${CMAKE_BINARY_DIR}/
)
add_dependencies(FormulaVRviewer VRBindings)

# =====================================================================
#  Installation - lays out a self-contained directory under the install
#                 prefix.  CPack then uses this layout as the input to
#                 the NSIS installer.
# =====================================================================
include(GNUInstallDirs)

# 1) The executable itself.
install(TARGETS FormulaVRviewer
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 2) Asset folders that the application reads at run-time.  These are
#    deployed alongside the binary so the installed program looks for
#    them in the same place it does in the build tree.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vrbindings/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING PATTERN "*.json")

# 3) Microsoft Visual C++ runtime DLLs.  Marking the installed file set
#    as part of a known component lets us put them in a different
#    install group later if we want to.
if (WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)            # universal C runtime
    set(CMAKE_INSTALL_OPENMP_LIBRARIES FALSE)
    # InstallRequiredSystemLibraries detects whether we built Release or
    # Debug and pulls in the matching MSVC redistributable.
    include(InstallRequiredSystemLibraries)
endif()

# 4) Qt runtime - generated dynamically by windeployqt / macdeployqt at
#    install time.  This is the proper way: it walks the executable's
#    actual import table and pulls only the DLLs that are needed.
qt_generate_deploy_app_script(
    TARGET FormulaVRviewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# 5) Resolve and install all NON-Qt runtime dependencies (VTK, OpenVR,
#    etc).  This runs LAST so the binary plus all of Qt's DLLs are
#    already in <prefix>/bin and we can walk the full DLL graph
#    starting from the exe.
#
#    NOTE: With the Visual Studio multi-config generator the config
#    name (Release/Debug) is part of the path, so we must use
#    CMAKE_INSTALL_CONFIG_NAME which is set at install-time by CMake.
#
#    PRE/POST_EXCLUDE_REGEXES filter out Windows system DLLs so we
#    don't redistribute Microsoft components we aren't licensed to.
if (WIN32)
    # Collect all directories that might contain VTK / OpenVR DLLs so
    # the dependency walker can locate them even if they aren't on PATH.
    # Add more paths here if VTK or OpenVR are installed elsewhere.
    set(_vtk_release_dirs
        "C:/Program Files (x86)/VTK/bin"
        "C:/VTK/VTK9.6.0_MSVC2022/bin/Release"
        "C:/OpenVR/bin/win64"
        "C:/OpenVR/lib/win64"
    )

    install(CODE "
        # With VS generator the exe is under bin/ after install(TARGETS)
        # already copied it there.  Walk from the installed copy so we
        # don't need to know the build-tree config subdirectory.
        set(_exe \"\${CMAKE_INSTALL_PREFIX}/bin/FormulaVRviewer.exe\")

        if (NOT EXISTS \"\${_exe}\")
            message(WARNING \"Cannot find \${_exe} - skipping VTK DLL bundling\")
        else()
            message(STATUS \"Resolving runtime dependencies for \${_exe} ...\")

            file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES   \"\${_exe}\"
                RESOLVED_DEPENDENCIES_VAR   _resolved
                UNRESOLVED_DEPENDENCIES_VAR _unresolved
                PRE_EXCLUDE_REGEXES
                    \"api-ms-.*\"
                    \"ext-ms-.*\"
                POST_EXCLUDE_REGEXES
                    \".*[Ss]ystem32/.*\\\\.[Dd][Ll][Ll]\"
                DIRECTORIES
                    \"\${CMAKE_INSTALL_PREFIX}/bin\"
                    \"C:/Program Files (x86)/VTK/bin\"
                    \"C:/VTK/VTK9.6.0_MSVC2022/bin/Release\"
                    \"C:/OpenVR/bin/win64\"
                    \"C:/OpenVR/lib/win64\"
            )

            message(STATUS \"Resolved \${_resolved}\")

            foreach(_dep \${_resolved})
                # Skip DLLs already copied into the install tree
                get_filename_component(_name \"\${_dep}\" NAME)
                if (NOT EXISTS \"\${CMAKE_INSTALL_PREFIX}/bin/\${_name}\")
                    file(INSTALL
                        DESTINATION \"\${CMAKE_INSTALL_PREFIX}/bin\"
                        TYPE        SHARED_LIBRARY
                        FOLLOW_SYMLINK_CHAIN
                        FILES       \"\${_dep}\"
                    )
                endif()
            endforeach()

            if (_unresolved)
                message(STATUS \"Still unresolved (may need adding to DIRECTORIES):\")
                foreach(_d \${_unresolved})
                    message(STATUS \"    \${_d}\")
                endforeach()
            endif()
        endif()
    ")
endif()

# =====================================================================
#  CPack - build a Windows installer with NSIS
#
#  Usage:
#       cmake --build .  --target package        # NSIS installer (.exe)
#       cmake --build .  --target package_source # source archive
# =====================================================================
set(CPACK_PACKAGE_NAME           "FormulaVRviewer")
set(CPACK_PACKAGE_VENDOR         "EEEE2076 Group")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${CMAKE_PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION_MAJOR  ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR  ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH  ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")

# A little flair for the NSIS pages (optional - works fine without)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
endif()

if (WIN32)
    # NSIS-specific niceties: start menu shortcut, desktop shortcut,
    # uninstaller registered with Add/Remove Programs.
    set(CPACK_GENERATOR              "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME      "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_DISPLAY_NAME      "${CPACK_PACKAGE_NAME} ${PROJECT_VERSION}")
    set(CPACK_NSIS_HELP_LINK         "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_NSIS_URL_INFO_ABOUT    "${PROJECT_HOMEPAGE_URL}")
    set(CPACK_NSIS_CONTACT           "EEEE2076 Group")

    # Treat FormulaVRviewer.exe as the executable to put on the Start Menu.
    set(CPACK_PACKAGE_EXECUTABLES    "FormulaVRviewer" "FormulaVRviewer")
    set(CPACK_CREATE_DESKTOP_LINKS   "FormulaVRviewer")

    # Make the uninstaller wipe the install directory cleanly so things
    # like windeployqt's plugin sub-folders are removed.
    set(CPACK_NSIS_MODIFY_PATH                "OFF")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "ON")

    # If the icons folder contains a .ico we use it for the installer
    # branding.  (Optional - falls back to the NSIS default otherwise.)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Icon/app.ico")
        set(CPACK_NSIS_MUI_ICON   "${CMAKE_CURRENT_SOURCE_DIR}/Icon/app.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/Icon/app.ico")
        set(CPACK_NSIS_INSTALLED_ICON_NAME    "bin/FormulaVRviewer.exe")
    endif()

    set(CPACK_SOURCE_GENERATOR "ZIP")
endif()

include(CPack)

# =====================================================================
#  Doxygen - HTML documentation of the API.
#
#  Run with:
#       cmake --build . --target Doxygen
#  Output:  ${CMAKE_BINARY_DIR}/docs/html/index.html
# =====================================================================
find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
    set(DOXYGEN_IN  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in")
    set(DOXYGEN_OUT "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(Doxygen
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
endif()
