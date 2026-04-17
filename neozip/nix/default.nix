{
  lib,
  stdenv,
  cmake,
  ninja,
}:

stdenv.mkDerivation {
  pname = "neozip";
  version = "2.3.90";

  src = ../.;

  nativeBuildInputs = [
    cmake
    ninja
  ];

  cmakeFlags = [
    "-DBUILD_TESTING=OFF"
    "-DWITH_GZFILEOP=ON"
    "-DZLIB_COMPAT=OFF"
  ];

  meta = {
    description = "Advanced zlib compression library";
    license = lib.licenses.zlib;
    platforms = lib.platforms.all;
  };
}
