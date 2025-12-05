# Contributing to CppLiveTuner

Thank you for your interest in contributing to CppLiveTuner! 🎉

## How to Contribute

### Reporting Bugs 🐛

1. Check if the bug has already been reported in [Issues](https://github.com/andogensi/CppLiveTuner/issues)
2. If not, create a new issue with:
   - Clear title and description
   - Steps to reproduce
   - Expected vs actual behavior
   - Your environment (OS, compiler, C++ version)

### Suggesting Features 💡

1. Open an issue with the `enhancement` label
2. Describe the feature and its use case
3. Discuss implementation ideas if you have any

### Submitting Pull Requests 🔧

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature`
3. Make your changes
4. Ensure code compiles on all platforms (or at least your target)
5. Add tests if applicable
6. Commit with clear messages: `git commit -m "Add: your feature description"`
7. Push to your fork: `git push origin feature/your-feature`
8. Open a Pull Request

## Development Setup

```bash
# Clone your fork
git clone https://github.com/YOUR_USERNAME/CppLiveTuner.git
cd CppLiveTuner

# Build with CMake
mkdir build && cd build
cmake .. -DLIVETUNER_BUILD_EXAMPLES=ON -DLIVETUNER_BUILD_TESTS=ON
cmake --build .

# Run tests
ctest --output-on-failure
```

## Code Style

- Use C++17 features appropriately
- Keep the header-only design intact
- Follow existing code formatting
- Add documentation comments for public APIs
- Use meaningful variable and function names

## Testing

- Add tests for new features
- Ensure existing tests pass
- Test on multiple platforms if possible

## Commit Messages

Use clear, descriptive commit messages:

- `Add: new feature description`
- `Fix: bug description`
- `Update: what was updated`
- `Docs: documentation changes`
- `Refactor: code refactoring`

## Questions?

Feel free to open an issue or discussion if you have any questions!

---

Thank you for contributing! ❤️
