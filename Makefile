default: build
.PHONY: build docker docker-shell clean

TOP := $(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

build:
	mkdir -p build
	cd build && cmake -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
	cd build && ninja

clean:
	rm -rf build

docker:
	docker build -t dtss_builder .

docker-shell:
	env docker run \
		--cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
		--rm \
		-e USER="$(USER)" \
		-v "$(TOP)":/mnt \
		--privileged \
		--entrypoint='bash' --interactive --tty \
		dtss_builder
