# Double Trouble SEU Smasher
An LLVM pass that implements CDMR to mitigate SEUs in commodity SoCs used for space missions

# Requirements
* LLVM 14 or higher (should be pretty obvious imho but you never know)
* CMake 3.18 or higher
* Ninja 1.11.0 or higher

# Build instructions
* Set `LLVM_HOME` to a directory that contains the LLVM build directory, which should include the `bin`, `lib`, and `include` subdirectories (on MacOS, this might be something like `/opt/homebrew/Cellar/llvm/16.0.0`) 
* Run `make build` in the root directory of the repo, which will automatically run CMake and Ninja
* Output files will be in the `build/` subdirectory

For ease of use, a Docker image with all dependencies preinstalled is also available. The image can be built with:

```bash
make docker
```

A container can be started that maps the repository to `/mnt` using:

```bash
make docker-shell
```