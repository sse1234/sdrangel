project(demodcwml)

# CWML_DIR points at a morse-pro checkout: headers come from cpp/,
# onnxruntime from cpp/third_party/ (populated by cpp/fetch_onnxruntime.sh),
# and the default model from models/cw-stream-v1/.
if(NOT CWML_DIR)
    message(FATAL_ERROR "demodcwml requires -DCWML_DIR=<path to morse-pro checkout>")
endif()

file(GLOB CWML_ORT_ROOT "${CWML_DIR}/cpp/third_party/onnxruntime-*")
list(LENGTH CWML_ORT_ROOT CWML_ORT_COUNT)
if(CWML_ORT_COUNT EQUAL 0)
    message(FATAL_ERROR "onnxruntime not found under ${CWML_DIR}/cpp/third_party - run cpp/fetch_onnxruntime.sh")
endif()
list(GET CWML_ORT_ROOT 0 CWML_ORT_DIR)

find_library(CWML_ORT_LIB onnxruntime HINTS "${CWML_ORT_DIR}/lib" REQUIRED)

set(demodcwml_SOURCES
    cwmldemod.cpp
    cwmldemodsettings.cpp
    cwmldemodbaseband.cpp
    cwmldemodsink.cpp
    cwmldemodplugin.cpp
    cwmldemodwebapiadapter.cpp
)

set(demodcwml_HEADERS
    cwmldemod.h
    cwmldemodsettings.h
    cwmldemodbaseband.h
    cwmldemodsink.h
    cwmldemodplugin.h
    cwmldemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CWML_DIR}/cpp
    ${CWML_ORT_DIR}/include
)

add_compile_definitions(CWML_DEFAULT_MODEL_DIR="${CWML_DIR}/models/cw-stream-v1")

if(NOT SERVER_MODE)
    set(demodcwml_SOURCES
        ${demodcwml_SOURCES}
        cwmldemodgui.cpp
        cwmldemodgui.ui
    )
    set(demodcwml_HEADERS
        ${demodcwml_HEADERS}
        cwmldemodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodcwml)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodcwmlsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodcwml_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME CWMLDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodcwml_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    ${CWML_ORT_LIB}
)

# Make the vendored onnxruntime findable at runtime
if(APPLE)
    set_target_properties(${TARGET_NAME} PROPERTIES
        BUILD_RPATH "${CWML_ORT_DIR}/lib"
        INSTALL_RPATH "${CWML_ORT_DIR}/lib"
    )
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
