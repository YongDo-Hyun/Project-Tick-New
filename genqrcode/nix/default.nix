{
  lib,
  stdenv,
  cmake,
  ninja,
  libpng,

  withTools ? true,
  withPNG ? true,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.lgpl21Plus,
}:

stdenv.mkDerivation {
  pname = "genqrcode";
  version = "4.1.1";

  src = ../.;

  nativeBuildInputs = [
    cmake
    ninja
  ];

  buildInputs = lib.optional withPNG libpng;

  cmakeFlags = [
    (lib.cmakeBool "WITH_TOOLS" withTools)
    "-DWITH_TESTS=OFF"
    (lib.cmakeBool "WITHOUT_PNG" (!withPNG))
    "-DBUILD_SHARED_LIBS=YES"
  ];

  meta = {
    description = "QR Code encoding library and CLI tool";
    homepage = "https://projecttick.org/";
    inherit license;
    inherit maintainers;
    mainProgram = "qrencode";
    platforms = lib.platforms.unix;
  };
}
