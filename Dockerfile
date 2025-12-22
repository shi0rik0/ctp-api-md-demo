# my_cpp_project/Dockerfile

# ----------------------------------------------------------------------------
# Stage 1: Builder - 编译阶段
# ----------------------------------------------------------------------------
# 使用一个包含C++开发工具链的Ubuntu镜像作为基础。
# 'AS builder' 给这个阶段命名为 'builder'，方便后续引用。
FROM ubuntu:24.04 AS builder

# 设置环境变量，避免在apt安装过程中出现交互式提示
ARG DEBIAN_FRONTEND=noninteractive

# 更新apt包列表并安装必要的构建工具：
# - build-essential: 包含g++, make等基本编译工具
# - cmake: 用于构建CMake项目
# - git: 如果你的项目需要从git仓库拉取依赖，可以包含
RUN apt-get update && \
    apt-get install -y --no-install-recommends make g++ && \
    rm -rf /var/lib/apt/lists/*

# 设置工作目录，所有后续命令都将在此目录下执行
WORKDIR /app

# 将当前目录（即你的C++项目文件）复制到容器的/app目录中
# 这一步应该在安装完所有依赖之后，这样可以利用Docker的缓存机制
COPY ./core .

# 创建一个build目录，并在其中执行CMake配置和构建
# -DCMAKE_BUILD_TYPE=Release: 以Release模式编译，通常会进行更多优化
RUN make


# ----------------------------------------------------------------------------
# Stage 2: Runner - 运行阶段
# ----------------------------------------------------------------------------
# 使用一个更小的基础镜像来运行编译好的应用程序。
# Ubuntu 24.04 依然是一个不错的选择，因为它包含了大多数C++运行时库。
# 如果需要更小的镜像，可以考虑 'debian:bookworm-slim' 或 'alpine' (但Alpine需要注意musl libc兼容性)
FROM ubuntu:24.04

# 设置工作目录
WORKDIR /app

# 从 builder 阶段复制编译好的可执行文件。
# 'my_cpp_app' 是你在 CMakeLists.txt 中 'add_executable' 定义的程序名称。
COPY --from=builder /app/build/ctpapi-md-demo .
COPY --from=builder /app/lib/ctpapi_v6.7.11/libthostmduserapi_se.so /lib/

# 如果你的C++应用程序需要其他运行时库（例如，通过apt安装的第三方库），
# 你可能需要在这一阶段安装它们。例如：
# RUN apt-get update && \
#     apt-get install -y --no-install-recommends \
#         libssl-dev \
#         libcurl4-openssl-dev && \
#     rm -rf /var/lib/apt/lists/*

# 安装 python3 和 pip
RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 python3-pip && \
    rm -rf /var/lib/apt/lists/*

COPY ./http_server .

RUN pip3 install --no-cache-dir --break-system-packages -r requirements.txt

# 暴露应用程序可能监听的端口（如果你的应用程序是服务器的话）
# 例如，如果你的C++程序监听8080端口：
EXPOSE 8000

# 定义容器启动时要执行的命令。
# 这里是运行我们编译好的C++应用程序。
CMD ["/bin/sh", "-c", "exec ./ctpapi-md-demo | python3 wrapper.py"]
