# Double Trouble SEU Smasher
A more efficient TMR implementation for large computations

# Requirements
* Boost 1.45
* Clang 8 or higher
* CMake 3.18 or higher
* Ninja 1.11.0 or higher

# Build instructions
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

# Usage

```c++
typedef pair<size_t, void *> DTSSInput;

struct InputData {
  vector<DTSSInput> inputs;
  DTSSInput output;
  bool operator==(InputData& b);
};
    
dtss_compute(unordered_set<InputData *> dataset,
             void (*processor)(InputData *));
```
