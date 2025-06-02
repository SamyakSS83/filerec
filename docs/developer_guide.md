# Developer Guide

## Getting Started

This guide will help you set up your development environment and understand the workflow for contributing to FileRec.

## Development Environment Setup

### Prerequisites

- C++17 compatible compiler (GCC 7+ or Clang 5+)
- CMake 3.16 or higher
- Git
- (Optional) Visual Studio Code with C/C++ extension
- (Optional) CLion or other C++ IDE

### Setting Up the Development Environment

1. Clone the repository:
```bash
git clone https://github.com/yourusername/filerec.git
cd filerec
```

2. Create a build directory:
```bash
mkdir -p build && cd build
```

3. Configure the project:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

4. Build the project:
```bash
make
```

5. Run the tests:
```bash
ctest
```

## Project Structure

Refer to the [Project Structure](project_structure.md) document for a detailed overview of the codebase organization.

## Coding Standards

### C++ Guidelines

- Follow C++17 standards and idioms
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) instead of raw pointers
- Prefer `std::string_view` over `const std::string&` for string arguments
- Use `auto` when it improves readability
- Initialize all variables at declaration
- Use `const` where appropriate
- Use C++ algorithms instead of raw loops when possible

### Naming Conventions

- Class names: `PascalCase`
- Function names: `camelCase`
- Variable names: `snake_case`
- Constants: `UPPER_CASE`
- File names: `snake_case.h/cpp`
- Member variables: `snake_case_` (with trailing underscore)

### Code Organization

- One class per header/source file (with exceptions for closely related small classes)
- Group related functions and classes in the same namespace
- Use the `FileRecovery` namespace for all components
- Keep function length reasonable (aim for under 50 lines)
- Limit nesting depth (aim for maximum 3 levels)

### Documentation

- Document all public interfaces with descriptive comments
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document parameters and return values with clear descriptions
- Include examples for non-obvious usage

## Debug Logging

Use the `Logger` class for debug output:

```cpp
// Different log levels are available
LOG_DEBUG("Detailed information for debugging");
LOG_INFO("Important information messages");
LOG_WARNING("Warning conditions");
LOG_ERROR("Error conditions");
LOG_CRITICAL("Critical errors");
```

Consider performance implications:
- Use conditional logging for high-volume messages
- Avoid string concatenation in hot paths
- Consider disabling verbose logging for performance-critical sections

## Testing

Refer to the [Testing Architecture](testing.md) document for guidelines on writing and running tests.

### Running Tests with Memory Checking

```bash
# Build with memory checking enabled
cmake -DENABLE_MEMCHECK=ON ..
make

# Run tests with memory checking
ctest -T memcheck
```

### Adding a New Test

1. Create a new test file
2. Add it to `tests/CMakeLists.txt`
3. Create test functions using the Google Test framework
4. Run the test to verify it passes

## Common Tasks

### Adding a New File Carver

1. Create a header file in `include/carvers/`
2. Create a source file in `src/carvers/`
3. Inherit from `BaseCarver`
4. Implement all required virtual methods
5. Add to `RecoveryEngine` initialization
6. Add tests in the `tests/` directory

### Fixing a Bug

1. Write a failing test that demonstrates the bug
2. Fix the implementation to make the test pass
3. Verify no regression in existing functionality
4. Document the fix in comments

### Improving Performance

1. Profile the application to identify bottlenecks
2. Create a benchmark to measure baseline performance
3. Make targeted improvements
4. Verify the benchmark shows improved performance
5. Ensure no functionality regression

## Pull Request Process

1. Create a feature branch from main
2. Make your changes with appropriate tests
3. Ensure all tests pass
4. Update documentation if needed
5. Submit a pull request with a clear description
6. Address review comments

## Release Process

1. Update version number in `CMakeLists.txt`
2. Update changelog
3. Run full test suite
4. Create a release tag
5. Build release binaries
6. Publish the release
