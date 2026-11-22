# Repository Guidelines

## Project Structure

This checkout has no source files, tests, or assets yet. When adding the first Qt widget, keep its implementation, header, and any Qt Designer form together so contributors can find everything needed to change it. Use `src/` for application code and `tests/` for automated tests if the first implementation needs separate directories. Keep Qt resources in a dedicated directory once the application uses them.

## Build, Test, and Run

Add the required Qt version and build instructions when introducing the first build configuration. For a CMake-based setup, document these commands once the corresponding targets exist:

- `cmake -S . -B build` configures the project.
- `cmake --build build` builds the application and any configured test targets.
- `ctest --test-dir build --output-on-failure` runs tests registered with CTest.

Document the executable name and its path in the build directory so contributors can launch it locally. Keep generated build output out of commits.

## Coding Style

Follow the surrounding code as it develops. For new C++ files, use four spaces for indentation, descriptive type and function names, and matching names for paired headers and source files. Use Qt parent ownership for child widgets and prefer typed signal-slot connections. Document any formatter or linter when adding its configuration.

## Testing

When introducing Qt Test, name test files after the widget or behavior they cover, such as `tst_settingswidget.cpp`. Add regression tests for behavior changes where practical. Include manual verification steps for visual or interactive changes that are difficult to automate.

## Commits and Pull Requests

Use a short, imperative commit subject, such as `Add keyboard navigation to settings`. Keep changes focused. In a pull request, describe the user-visible change, list the checks you ran, and link the issue when one exists. Include a screenshot or short recording for visible UI changes.
