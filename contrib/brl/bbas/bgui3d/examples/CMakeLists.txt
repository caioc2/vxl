# brl/bbas/bgui3d/examples/CMakeLists.txt

INCLUDE_DIRECTORIES( ${GEL_INCLUDE_DIR})
INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR} )
IF (HAS_BGUI3D)

ADD_EXECUTABLE(bgui3d_example_basic MACOSX_BUNDLE example_basic.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_basic bgui3d)

ADD_EXECUTABLE(bgui3d_example_basic_animate MACOSX_BUNDLE example_basic_animate.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_basic_animate bgui3d)

ADD_EXECUTABLE(bgui3d_example_basic_trackball MACOSX_BUNDLE example_basic_trackball.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_basic_trackball bgui3d)

ADD_EXECUTABLE(bgui3d_example_project2d MACOSX_BUNDLE example_project2d.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_project2d bgui3d vil vnl)

ADD_EXECUTABLE(bgui3d_example_examiner_animate MACOSX_BUNDLE example_examiner_animate.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_examiner_animate bgui3d)

ADD_EXECUTABLE(bgui3d_example_examiner_27balls MACOSX_BUNDLE example_examiner_27balls.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_examiner_27balls bgui3d)

ADD_EXECUTABLE(bgui3d_example_examiner_helix MACOSX_BUNDLE example_examiner_helix.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_examiner_helix bgui3d)

ADD_EXECUTABLE(bgui3d_example_examiner_file MACOSX_BUNDLE example_examiner_file.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_examiner_file bgui3d)

ADD_EXECUTABLE(bgui3d_example_grid_examiner MACOSX_BUNDLE example_grid_examiner.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_grid_examiner bgui3d)

ADD_EXECUTABLE(bgui3d_example_point_set MACOSX_BUNDLE example_point_set.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_point_set bgui3d vnl)

ADD_EXECUTABLE(bgui3d_example_grid_multiscene MACOSX_BUNDLE example_grid_multiscene.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_grid_multiscene bgui3d)

ADD_EXECUTABLE(bgui3d_example_examiner_cylinders MACOSX_BUNDLE example_examiner_cylinders.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_examiner_cylinders bgui3d vnl)

ADD_EXECUTABLE(bgui3d_example_interact MACOSX_BUNDLE example_interact.cxx)
TARGET_LINK_LIBRARIES(bgui3d_example_interact bgui3d vgl)

ENDIF (HAS_BGUI3D)
