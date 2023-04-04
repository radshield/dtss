default: build

TOP := $(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

build:
  mkdir -p build
  cd build && cmake -G Ninja ..
  cd build && ninja

clean:
  rm -rf build

docker:
  docker build -t dtss_builder .

docker-shell:
  env docker run \
    --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
    -e USER="$(USER)" \
    -v "$(TOP)":/mnt \
    -v /var/run/docker.sock:/var/run/docker.sock \
    --privileged \
    --entrypoint='bash' --interactive --tty \
    dtss_builder
