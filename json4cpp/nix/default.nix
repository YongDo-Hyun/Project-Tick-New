{
  lib,
  stdenv,
  cmake,
  ninja,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.mit,
}:

stdenv.mkDerivation {
  pname = "json4cpp";
  version = "10.0.3";

  src = ../.;

  nativeBuildInputs = [
    cmake
    ninja
  ];

  cmakeFlags = [
    "-DJSON_BuildTests=OFF"
    "-DJSON_Install=ON"
    "-DJSON_MultipleHeaders=ON"
  ];

  meta = {
    description = "JSON for Modern C++ - header-only library";
    homepage = "https://json.nlohmann.me/";
    inherit license;
    inherit maintainers;
    platforms = lib.platforms.all;
  };
}
