![](https://github.com/OpenCppCoverage/OpenCppCoverage/workflows/Unit%20tests/badge.svg)
# OpenCppCoverage

OpenCppCoverage is an open source code coverage tool for C++ under Windows.

The main usage is for unit testing coverage, but you can also use it to know the executed lines in a program for debugging purpose.

---------------------
## Project status: archived and no longer maintained

This project is no longer actively maintained.

I stopped active development and maintenance approximately seven years ago after moving away from C++ in my professional work. At first, I expected this to be temporary, but I am now formally retiring the project.

The existing source code and releases will remain available for historical use. However:

* no further releases, bug fixes, or compatibility updates are planned;
* issues and pull requests will not be reviewed;
* support questions may not receive a response;
* security fixes should not be expected;
* users should evaluate the software carefully before continuing to depend on it.

Forks and independent continuation of the project are welcome, subject to the existing licence.

### Community-maintained forks

**There is currently no designated successor or recommended fork.**

If you actively maintain a fork of this project and would like it to be considered for inclusion here, please contact me through OpenCppCoverage@gmail.com.

I intend to link from this page to the most active community-maintained fork I am aware of. The selected fork may change over time as development activity changes.

Any fork listed here is maintained independently. Its inclusion does not constitute a transfer of ownership, and I cannot provide support or guarantees for it.

### Existing issues and pull requests

Existing issues and pull requests have been closed as part of retiring the project.

Their closure does not necessarily mean that an issue was resolved, that a proposed change was rejected, or that the contribution lacked value. It only means that the original repository is no longer being maintained.

The history and discussion will remain available for reference.

### Licence

The project remains available under the terms described in the [LICENSE](LICENSE) file.

Archiving the project does not change its licence. You may use, modify, fork, and redistribute the code according to those terms.

### Thank you

Thank you to everyone who used the project, reported issues, contributed code or documentation, reviewed changes, answered questions, or otherwise helped improve it.

Maintaining this project and working with its community was a valuable experience. I am grateful for all the time and effort contributed over the years.

---------------------
## Features:
- **Visual Studio support**: Support compiler with program database file (.pdb).
- **Non intrusive**: Just run your program with OpenCppCoverage, no need to recompile your application.
- **HTML reporting**
- **Line coverage**.
- **Branch coverage** (experimental): See [Branch coverage](#branch-coverage-experimental) below.
- **Run as Visual Studio Plugin**: See [here](https://github.com/OpenCppCoverage/OpenCppCoveragePlugin) for more information.
- **Jenkins support**: See [here](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki/Jenkins) for more information.
- **Support optimized build**.
- **Exclude a line based on a regular expression**.
- **Child processes coverage**.
- **Coverage aggregation**: Run several code coverages and merge them into a single report.
 
## Requirements
- Windows Vista or higher.
- Microsoft Visual Studio 2008 or higher all editions **including Express edition**. It should also work with previous version of Visual Studio.

## Download
OpenCppCoverage can be downloaded from [here](../../releases).

## Usage
You can simply run the following command:

```OpenCppCoverage.exe --sources MySourcePath* -- YourProgram.exe arg1 arg2```

For example, *MySourcePath* can be *MyProject*, if your sources are located in *C:\Dev\MyProject*.

See [Getting Started](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki) for more information about the usage.
You can also have a look at [Command-line reference](https://github.com/OpenCppCoverage/OpenCppCoverage/wiki/Command-line-reference).

## Branch coverage (experimental)

In addition to line coverage, OpenCppCoverage can measure branch coverage: for every
conditional jump, it records whether the branch was taken, not taken, or both.
Branch coverage is disabled by default and enabled with `--branch_coverage`:

```
OpenCppCoverage.exe --branch_coverage --sources MySourcePath* --export_type cobertura:coverage.xml -- YourProgram.exe
```

### How it works

Conditional jump instructions are located by disassembling the code of each covered
source line ([Zydis](https://github.com/zyantific/zydis) disassembler, no recompilation
needed). Each conditional jump is instrumented with a breakpoint; when it is hit, the
branch outcome is computed from the CPU flags and the breakpoint stays armed until both
outcomes have been observed. Because of this extra instrumentation, the profiled program
runs slower than with line coverage only.

### Report formats

Branch coverage is exported in the **Cobertura** format:

- `branch-rate`, `branches-covered` and `branches-valid` are filled with real values;
- every line containing conditional jumps gets `branch="true"` and
  `condition-coverage="50% (1/2)"` style attributes, plus a `<conditions>` element with
  one entry per conditional jump.

This output works out of the box with the Jenkins
[Coverage Plugin](https://plugins.jenkins.io/coverage/) and with the VS Code
[Coverage Gutters](https://marketplace.visualstudio.com/items?itemName=ryanluker.vscode-coverage-gutters)
extension (name the report `coverage.xml` or `cobertura.xml` so it is detected
automatically; partially covered branch lines are highlighted in yellow).

The **binary** export format (`--export_type binary`) also carries branch data, so it
survives `--input_coverage` imports and coverage aggregation: outcomes of the same
branch are merged across runs. Old `.cov` files without branch data can still be
imported. The HTML report and the console summary show line coverage only.

### Limitations

- Branches are measured at the assembly level: one source line can contain several
  conditional jumps (e.g. `&&`, `||`, loops), and the compiler can generate additional
  ones. Conversely, some conditional constructs compile to branch-free code
  (e.g. `cmov`) or jump tables (some `switch` statements) and are not measured.
- Aggregating runs with `--input_coverage` matches branches by file, line and index,
  which is only reliable when all runs use the same binaries.
- Export plugins must be rebuilt against the new plugin interface (version 2).
