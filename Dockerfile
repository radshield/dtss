FROM rockylinux:9.2

# Install LLVM tools
RUN dnf install -y clang-15 \
                   llvm \
                   llvm-devel \
                   lld \
                   lldb

# Install other tools
RUN dnf install -y make \
                   cmake \
                   file \
                   ninja-build \
                   rsync \
                   bash-completion \
                   less \
                   ca-certificates \
                   wget

# By default, run a bash shell
ENTRYPOINT ["/bin/bash"]
