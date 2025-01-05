## Notes

A collection of all the notes taken during the development of the project.

- GCC, Clang, and MSVC are the three main compilers for C++.
- Compilers implement the C++ standard.
- Code is passed to the compiler to generate machine code.
- There may be compiler-specific primitives in the implementation of the standard.
- Clang is the frontend of the LLVM compiler.
- LLVM represents a more modern and flexible paradigm in compiler design.
- The frontend of a compiler is responsible for parsing the code and generating an intermediate representation (IR).
- GCC and MSVC contain both frontend and backend components.
- The full C++ modules feature is only supported by MSVC at the moment.

- Use `.vscode/c_cpp_properties.json` to configure IntelliSense.

- A Makefile is like a script that runs the compiler's build commands and other related tasks.
- CMake is like a meta-build system that generates Makefiles.
- Automake is similar to CMake but is more complex and specific to GNU build systems.

- Use `2>&1` to redirect error output to standard output.
- `#include` is essentially an instruction for copying and pasting code from header files.

- **libvpx** is the library for the VP8 and VP9 video codecs.
- **FFmpeg** uses libvpx under the hood for VP8 and VP9 encoding.
- **AV1** is the successor to VP9.
- **WebM** and **MP4** are two widely-used video container formats.


- reentrant(re-entrant) means the code can be interrupted and re-entered.
- The compiler flag -D_REENTRANT is used to enable reentrant version of implementations.


### CMake Notes

- find_package() is used to find and load a package and it has to work with existing CMake modules.
    - e.g. `find_package(OpenCV REQUIRED)`
    - `find_packet(libpvx)` will not work because there is no CMake module for libvpx.
    - However, `pkg-config` can be used to find libvpx and there is a CMake module for pkg-config.

- Group subdirectories using `add_subdirectory()`.
    - Compile subdirectories into libraries using `add_library()` in the subdirectories' CMakeLists.txt.
    - By default, the `add_library()` command will build a static library, and the final executable will be linked against it.

- Include path needs to be explicitly specified using `target_include_directories()`.
    - `target_include_directories(target PUBLIC include)` will not search subdirectories.
    - The `PUBLIC` keyword is used to propagate the include path to the target's dependents.

### Tambur Deployment Notes

1. Run the install commands per `tambur_install.sh`
    - `mahimahi` ppa doesn't support Ubuntu 24.04 LTS, so it needs to be compiled from source.
    - Some code would report error when compiling with GCC13, e.g. `#include <cstdint>`
    - `gf-complete`, `Jerasure`, `libtorch`, `maxflow`, `nlohmann`(for json), `ringmaster` should be installed in third_party folder.

2. Modify the reading script `run-config.py` to read the video file as binary:
    ```py
    with open(y4m_path, 'rb') as y4m_fh:
    try:
        line = y4m_fh.readline(MAX_BUFFER)
    except:
        sys.exit(f'ERROR: {y4m_path} is not a valid .y4m file')

    for item in line.split():
        item = item.decode()
        if item[0] == 'W':
            width = int(item[1:])
        elif item[0] == 'H':
            height = int(item[1:])
    ```

3. (Optional) Modify the thread usage in src/ringmaster_app/encoder.cc and decoder.cc to leave enough threads for the fec.
    - e.g. `const unsigned int cpu_used = min(get_nprocs(), 4);`

4. Download the video and run with command `python3 src/ringmaster_scripts/bootstrap.py --num_sender_receiver_pairs 1 --videos_folder ~/codes/tambur/video --config third_party/ringmaster_configs/experiment_random_variable.json --offset 0 > out.log 2>&1`
    - Most outputs are stderr and need to be redirected to stdout(`2>&1`).
    - The `--offset` parameter is used to specify the port.
    - The script will run for `timeout + 5` seconds. The timeout is specified in the json file.

5. Plot the results: `src/plot/generate_all_plots_FEC_only.sh --config=logs_random_variable_GE/experiment_random_variable.json --plot-folder=output`
    - The output folder cannot exist before running the script.
    - (Optional) May need to install the font 'Times New Roman' for the plot.
        - Use `python3 -c "import matplotlib as mpl; print(mpl.matplotlib_fname())"` to find the matplotlibrc cache and clear it.

### Mahimahi Notes

- Mahimahi will create a shell environment for each command.
- Basic commands: `mm-delay` `mm-loss`.
- Each shell would have a virtual network interface.
    - Using `ifconfig` can verify that the virtual the virtual interface would connect to the outer shell via a PPP link:
    ```text
    $ ifconfig
    ingress: flags=81<UP,POINTOPOINT,RUNNING>  mtu 1500
    inet 10.0.0.2  netmask 255.255.255.255  destination 10.0.0.1
    inet6 fe80::a9da:9a55:25c3:33e5  prefixlen 64  scopeid 0x20<link>
    unspec 00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00  txqueuelen 500  (UNSPEC)
    RX packets 1978  bytes 2104768 (2.1 MB)
    RX errors 0  dropped 0  overruns 0  frame 0
    TX packets 1927  bytes 83918 (83.9 KB)
    TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
    ```
    - The address pointing to the host outside all containers can be accessed by the env var `MAHIMAHI_BASE` in the shell.

### C++ Notes

- Use `extern "C" {}` to include C code in C++.
    - C++ code include extra information in the function name for overloading, which is not present in C.
    - The compiler will automatically add this to standard C libraries.

### Tambur Notes

#### Calling Chain

- Encoder -> fec_sender -> frame_generator -> StreamCode -> BlockCode -> jerasure_matrix_encode
- BlockCode -> BlockCodeFactory -> set_cauchy_matrix -> cauchy_original_coding_matrix