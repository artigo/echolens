cmake_minimum_required(VERSION 3.10)
project(echolens C)

find_program(LINUXDEPLOY_EXECUTABLE linuxdeploy REQUIRED)

add_executable(echolens src/main.c)

install(TARGETS echolens DESTINATION bin)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

add_custom_target(AppImage
    # First, run the 'install' step to move the executable to a clean staging area.
    COMMAND ${CMAKE_COMMAND} --build . --target install

    # Second, run linuxdeploy to package the staged files into an AppImage.
    COMMAND ${LINUXDEPLOY_EXECUTABLE}
        --appdir "${CMAKE_BINARY_DIR}/AppDir"
        -e "${CMAKE_INSTALL_PREFIX}/bin/echolens"
        -d "${CMAKE_CURRENT_SOURCE_DIR}/assets/echolens.desktop"
        -i "${CMAKE_CURRENT_SOURCE_DIR}/assets/echolens.png"
        --output appimage
    
    # This ensures 'echolens' is compiled before this target runs.
    DEPENDS echolens

    COMMENT "Installing and Creating Echolens AppImage..."
)