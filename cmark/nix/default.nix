{
  lib,
  stdenv,
  cmake,
  ninja,
}:

stdenv.mkDerivation {
  pname = "cmark";
  version = "0.31.2";

  src = ../.;

  nativeBuildInputs = [
    cmake
    ninja
  ];

  cmakeFlags = [
    "-DBUILD_TESTING=OFF"
  ];

  meta = {
    description = "CommonMark parsing and rendering library and program in C";
    homepage = "https://github.com/commonmark/cmark";
    license = lib.licenses.bsd2;
    platforms = lib.platforms.all;
  };
}
