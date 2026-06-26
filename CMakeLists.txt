cmake_minimum_required(VERSION 3.16)

# SDKTARGETSYSROOT 환경변수 지원 (Yocto SDK 크로스 컴파일)
# CMAKE_SYSROOT는 project() 전에 설정해야 컴파일러에 반영됨
if(DEFINED ENV{SDKTARGETSYSROOT})
    set(SDKTARGETSYSROOT $ENV{SDKTARGETSYSROOT})
    set(CMAKE_SYSROOT ${SDKTARGETSYSROOT})
    set(CMAKE_FIND_ROOT_PATH ${SDKTARGETSYSROOT})
else()
    set(SDKTARGETSYSROOT "" CACHE PATH "Yocto SDK target sysroot path")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

project(file_trans C CXX)

find_package(Threads REQUIRED)

set(SRCS
    src/main.cpp
    src/file_trans_server.cpp
    src/trans_session.cpp
    src/udp_broadcast_server.cpp
)

add_executable(file_trans ${SRCS})

target_compile_options(file_trans PRIVATE -Wall -O2 -g -Wno-unused-result)

# 인클루드 경로
target_include_directories(file_trans PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio_1.38.0/include
)

target_compile_definitions(file_trans
    PRIVATE
        ASIO_STANDALONE
)

if(SDKTARGETSYSROOT)
    target_include_directories(file_trans PRIVATE
        ${SDKTARGETSYSROOT}/usr/include
    )
    target_link_directories(file_trans PRIVATE
        ${SDKTARGETSYSROOT}/usr/lib
    )
endif()

# 링크 라이브러리
target_link_libraries(file_trans PRIVATE
    Threads::Threads
)

# 디버그 빌드 시 DEBUG 매크로 추가
target_compile_definitions(file_trans PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)
