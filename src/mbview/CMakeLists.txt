find_package(X11 REQUIRED)
find_package(Motif REQUIRED)
find_package(OpenGL REQUIRED)

message("OPENGL_FOUND ${OPENGL_FOUND}")
message("OpenGL_OpenGL_FOUND ${OpenGL_OpenGL_FOUND}")
message("OPENGL_opengl_LIBRARY ${OPENGL_opengl_LIBRARY}")
message("OPENGL_INCLUDE_DIR ${OPENGL_INCLUDE_DIR}")
message("OpenGL_GLX_FOUND ${OpenGL_GLX_FOUND}")
message("OPENGL_glx_LIBRARY ${OPENGL_glx_LIBRARY}")
message("OPENGL_GLX_INCLUDE_DIR ${OPENGL_GLX_INCLUDE_DIR}")

add_library(
  mbview
  MB3DNavList.c
  MBpedit.c
  mbview_primary.c
  MB3DRouteList.c
  mbpingedit_callbacks.c
  mbview_process.c
  Mb3dsdg.c
  mbview_bxutils.c
  mbview_profile.c
  MB3DSiteList.c
  mbview_callbacks.c
  mbview_route.c
  mb3dsoundings_callbacks.c
  mbview_nav.c
  mbview_secondary.c
  MB3DView.c
  mbview_pick.c
  mbview_site.c
  mb_glwdrawa.c
  mbview_plot.c
  mbview_vector.c)

target_include_directories(mbview PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mbview PRIVATE OpenGL::GL OpenGL::GLU OpenGL::GLX mbio Motif::Motif X11::Xt)

install(TARGETS mbview DESTINATION ${CMAKE_INSTALL_LIBDIR})
