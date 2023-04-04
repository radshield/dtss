FROM ubuntu:jammy

# Install LLVM tools
RUN apt-get update
RUN apt-get install -y clang \
                       llvm \
                       llvm-dev \
                       llvm-14-tools \
                       lld \
                       lldb \
                       libc++-dev \
                       libc++abi-dev \
                       libomp-dev

# Install other build tools
RUN apt-get install -y make \
                       cmake \
                       file \
                       ninja-build \
                       meson \
                       rsync \
                       bash-completion \
                       less

ENV LLVM_HOME=/usr/lib/llvm-14

# By default, run a bash shell
ENTRYPOINT ["/bin/bash"]
