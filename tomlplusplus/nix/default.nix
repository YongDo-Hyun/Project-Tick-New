{
  lib,
  stdenv,
  cmake,
  ninja,
}:

stdenv.mkDerivation {
  pname = "tomlplusplus";
  version = "10.0.3";

  src = ../.;

  nativeBuildInputs = [
    cmake
    ninja
  ];

  cmakeFlags = [
    "-DBUILD_TESTING=OFF"
  ];

  meta = {
    description = "Header-only TOML config file parser and serializer for C++17";
    homepage = "https://marzer.github.io/tomlplusplus/";
    license = lib.licenses.mit;
    platforms = lib.platforms.all;
  };
}
