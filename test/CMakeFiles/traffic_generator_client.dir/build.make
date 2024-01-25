# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.27

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP

# Include any dependencies generated for this target.
include test/CMakeFiles/traffic_generator_client.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/CMakeFiles/traffic_generator_client.dir/compiler_depend.make

# Include the progress variables for this target.
include test/CMakeFiles/traffic_generator_client.dir/progress.make

# Include the compile flags for this target's objects.
include test/CMakeFiles/traffic_generator_client.dir/flags.make

test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o: test/CMakeFiles/traffic_generator_client.dir/flags.make
test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o: test/traffic_generator_client.c
test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o: test/CMakeFiles/traffic_generator_client.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o"
	cd /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test && /usr/lib64/ccache/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o -MF CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o.d -o CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o -c /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test/traffic_generator_client.c

test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing C source to CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.i"
	cd /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test && /usr/lib64/ccache/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test/traffic_generator_client.c > CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.i

test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling C source to assembly CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.s"
	cd /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test && /usr/lib64/ccache/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test/traffic_generator_client.c -o CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.s

# Object files for target traffic_generator_client
traffic_generator_client_OBJECTS = \
"CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o"

# External object files for target traffic_generator_client
traffic_generator_client_EXTERNAL_OBJECTS =

test/traffic_generator_client: test/CMakeFiles/traffic_generator_client.dir/traffic_generator_client.c.o
test/traffic_generator_client: test/CMakeFiles/traffic_generator_client.dir/build.make
test/traffic_generator_client: lib/libmicrotcp.so
test/traffic_generator_client: test/CMakeFiles/traffic_generator_client.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable traffic_generator_client"
	cd /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/traffic_generator_client.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/CMakeFiles/traffic_generator_client.dir/build: test/traffic_generator_client
.PHONY : test/CMakeFiles/traffic_generator_client.dir/build

test/CMakeFiles/traffic_generator_client.dir/clean:
	cd /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test && $(CMAKE_COMMAND) -P CMakeFiles/traffic_generator_client.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/traffic_generator_client.dir/clean

test/CMakeFiles/traffic_generator_client.dir/depend:
	cd /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test /home/george/Documents/CSD/CS-335_Computer_Networks/Winter_Semester/ProjectPhaseB/A/microTCP/test/CMakeFiles/traffic_generator_client.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : test/CMakeFiles/traffic_generator_client.dir/depend

