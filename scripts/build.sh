#!/bin/bash
BUILD_DIR="${BUILD_DIR:-build}"

# Create build directory
if [ -d "$BUILD_DIR" ];
then
  echo Removing old build directory...
  rm -Rf $BUILD_DIR;
fi

echo Creating build directory...
mkdir $BUILD_DIR && cd $BUILD_DIR

# Run CMake
echo Running CMake...
cmake ..

if [ ! $? -eq 0 ]
then
  echo CMake failed
  exit 1
fi

# Build
echo Building...
cmake --build .

if [ ! $? -eq 0 ]
then
  echo Build failed
  exit 1
fi

# Run tests
echo Running tests...
ctest --output-on-failure

if [ ! $? -eq 0 ]
then
  echo Tests failed
  exit 1
fi

# Leave build directory
echo Leaving build directory...
cd ..
