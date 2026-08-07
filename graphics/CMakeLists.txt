# ============================================================
# KyuzenOS Graphics subsystem — CMake build (optional).
#
# The kernel normally builds via the top-level Makefile (SRC_DIRS
# globbing). This CMakeLists documents the module as a standalone
# static library so it can be unit-tested or ported independently.
#
# Sources are grouped by layer (hal / surface / backend / renderer)
# to keep the dependency boundaries visible.
# ============================================================

cmake_minimum_required(VERSION 3.16)
project(kyuzen_graphics C)

# Consumers must supply INCLUDE_DIRS (kernel `include/` root) plus any
# kernel dependencies (heap, spinlock). This target only needs the
# graphics public headers plus the kernel include root.
set(GRAPHICS_SOURCES
    hal/gpu.c
    hal/surface.c
    surface/surface_manager.c
    renderer/graphics.c
    backend/software/software_gpu.c
)

add_library(kyuzen_graphics STATIC ${GRAPHICS_SOURCES})

target_include_directories(kyuzen_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include        # graphics/ public headers (mirror)
    ${INCLUDE_DIRS}                            # kernel include/ root (heap, spinlock, string)
)

set_target_properties(kyuzen_graphics PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# --- Backend selection -------------------------------------------------------
# Only the software backend is implemented now. Future backends (virtio-gpu,
# svga, bochs, ...) register a second gpu_backend_t; the HAL picks the first
# that initializes. No build-flag surgery is required to add one.
