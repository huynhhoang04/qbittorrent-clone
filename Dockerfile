# Sử dụng Ubuntu 22.04 làm base image
FROM ubuntu:22.04

# Vô hiệu hóa các thông báo hỏi Yes/No khi cài đặt package
ENV DEBIAN_FRONTEND=noninteractive

# Cập nhật hệ thống và cài đặt các thư viện cần thiết
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    libsqlite3-dev \
    libssl-dev \
    nlohmann-json3-dev \
    libncurses5-dev libncursesw5-dev \
    libgtest-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

# Cài đặt Crow Framework (tải file header vào thư mục include)
RUN wget https://github.com/CrowCpp/Crow/releases/download/v1.0+5/crow_all.h -O /usr/local/include/crow.h

# Build và cài đặt Google Test
RUN cd /usr/src/gtest && cmake CMakeLists.txt && make && cp lib/*.a /usr/lib

# Tạo thư mục làm việc trong container
WORKDIR /app

# Copy toàn bộ source code từ máy bạn vào container
COPY . .

# Tạo thư mục build, cấu hình CMake và biên dịch mã nguồn
RUN rm -rf build && mkdir -p build && cd build && cmake .. && make -j$(nproc)

# Lệnh mặc định
CMD ["bash"]