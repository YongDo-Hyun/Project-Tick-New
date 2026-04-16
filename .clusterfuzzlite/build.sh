#!/bin/bash -eu

############
# CMark
############
cd $SRC/project-tick/cmark
mkdir -p build && cd build
cmake .. -DCMARK_TESTS=OFF -DCMARK_STATIC=ON
make -j$(nproc)

$CC $CFLAGS -I$SRC/project-tick/cmark/src -I$SRC/project-tick/cmark/build/src \
    $SRC/project-tick/cmark/fuzz/cmark-fuzz.c -o $OUT/cmark-fuzz \
    $SRC/project-tick/cmark/build/src/libcmark.a \
    $LIB_FUZZING_ENGINE

cp $SRC/project-tick/cmark/fuzz/dictionary $OUT/cmark-fuzz.dict

############
# NeoZip
############
cd $SRC/project-tick/neozip
mkdir -p build && cd build

cmake .. \
    -DBUILD_TESTING=ON \
    -DWITH_FUZZERS=ON \
    -DWITH_GTEST=OFF \
    -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)

for fuzzer in fuzzer_checksum fuzzer_compress fuzzer_example_small \
              fuzzer_example_large fuzzer_example_flush fuzzer_example_dict; do
    cp test/fuzz/$fuzzer $OUT/
done

############
# JSON4CPP
############
cd $SRC/project-tick/json4cpp

for fuzzer in parse_json parse_bson parse_cbor parse_msgpack parse_ubjson parse_bjdata; do
    $CXX $CXXFLAGS -std=c++11 -I single_include \
        tests/src/fuzzer-${fuzzer}.cpp \
        -o $OUT/${fuzzer}_fuzzer \
        $LIB_FUZZING_ENGINE
done

############
# TOML++
############
cd $SRC/project-tick/tomlplusplus
mkdir -p build
cmake -S . -B build -DBUILD_FUZZER=ON
cmake --build build --target install

mkdir -p corpus
find $SRC/project-tick/tomlplusplus -name "*.toml" -exec cp {} corpus \;
zip -q -j $OUT/toml_fuzzer_seed_corpus.zip corpus/*
