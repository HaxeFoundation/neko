FILE(TO_CMAKE_PATH "$ENV{GTK3_DIR}" TRY1_DIR)
FILE(TO_CMAKE_PATH "${GTK3_DIR}" TRY2_DIR)
FILE(GLOB GTK_DIR ${TRY1_DIR} ${TRY2_DIR})

FIND_PATH(GTK_gtk_3_INCLUDE_DIR gtk/gtk.h
                                ENV INCLUDE DOC "Directory containing gtk/gtk.h include file")

FIND_PATH(GTK_gdk_3_INCLUDE_DIR gdk/gdk.h
                                ENV INCLUDE DOC "Directory containing gdk/gdk.h include file")

FIND_PATH(GTK_gdkconfig_3_INCLUDE_DIR gdkconfig.h
                                      ENV INCLUDE DOC "Directory containing gdkconfig.h include file")

FIND_LIBRARY(GTK_gdk_pixbuf_3_LIBRARY NAMES gdk_pixbuf-3.0
                                      PATHS ${GTK_DIR}/lib ${GTK_DIR}/bin ${GTK_DIR}/win32/bin ${GTK_DIR}/lib ${GTK_DIR}/win32/lib /usr/local/lib /usr/lib
                                      ENV LIB
                                      DOC "gdk_pixbuf library to link with"
                                      NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GTK_gdk_3_LIBRARY NAMES gdk-win32-3.0 gdk-x11-3.0
                                     PATHS ${GTK_DIR}/lib ${GTK_DIR}/bin ${GTK_DIR}/win32/bin ${GTK_DIR}/lib ${GTK_DIR}/win32/lib /usr/lib /usr/local/lib
                                     ENV LIB
                                     DOC "gdk3 library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GTK_gtk_3_LIBRARY NAMES gtk-win32-3.0 gtk-x11-3.0
                                     PATHS ${GTK_DIR}/lib ${GTK_DIR}/bin ${GTK_DIR}/win32/bin ${GTK_DIR}/lib ${GTK_DIR}/win32/lib /usr/lib /usr/local/lib
                                     ENV LIB
                                     DOC "gtk3 library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GTK_gdk_pixbuf_3_STATIC_LIBRARY NAMES libgdk_pixbuf-3.0.a
                                      PATHS ${GTK_DIR}/lib ${GTK_DIR}/bin ${GTK_DIR}/win32/bin ${GTK_DIR}/lib ${GTK_DIR}/win32/lib /usr/local/lib /usr/lib
                                      ENV LIB
                                      DOC "gdk_pixbuf library to link with"
                                      NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GTK_gdk_3_STATIC_LIBRARY NAMES libgdk-x11-3.0.a
                                     PATHS ${GTK_DIR}/lib ${GTK_DIR}/bin ${GTK_DIR}/win32/bin ${GTK_DIR}/lib ${GTK_DIR}/win32/lib /usr/lib /usr/local/lib
                                     ENV LIB
                                     DOC "gdk3 library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)

FIND_LIBRARY(GTK_gtk_3_STATIC_LIBRARY NAMES libgtk-x11-3.0.a
                                     PATHS ${GTK_DIR}/lib ${GTK_DIR}/bin ${GTK_DIR}/win32/bin ${GTK_DIR}/lib ${GTK_DIR}/win32/lib /usr/lib /usr/local/lib
                                     ENV LIB
                                     DOC "gtk3 library to link with"
                                     NO_SYSTEM_ENVIRONMENT_PATH)


IF (GTK_gtk_3_INCLUDE_DIR AND GTK_gdk_3_INCLUDE_DIR AND GTK_gdkconfig_3_INCLUDE_DIR)
  SET(GTK3_INCLUDE_DIR ${GTK_gtk_3_INCLUDE_DIR} ${GTK_gdk_3_INCLUDE_DIR} ${GTK_gdkconfig_3_INCLUDE_DIR})
  list(REMOVE_DUPLICATES GTK3_INCLUDE_DIR)
  SET(GTK3_LIBRARIES ${GTK_gdk_pixbuf_3_LIBRARY} ${GTK_gdk_3_LIBRARY} ${GTK_gtk_3_LIBRARY})
  list(REMOVE_DUPLICATES GTK3_LIBRARIES)
  SET(GTK3_STATIC_LIBRARIES ${GTK_gdk_pixbuf_3_STATIC_LIBRARY} ${GTK_gdk_3_STATIC_LIBRARY} ${GTK_gtk_3_STATIC_LIBRARY})
  list(REMOVE_DUPLICATES GTK3_STATIC_LIBRARIES)
  SET(GTK3_FOUND TRUE)


  message(STATUS "GTK3_INCLUDE_DIR: ${GTK3_INCLUDE_DIR}")
  message(STATUS "GTK3_LIBRARIES: ${GTK3_LIBRARIES}")
  message(STATUS "GTK3_STATIC_LIBRARIES: ${GTK3_STATIC_LIBRARIES}")
ENDIF ()
