{
  lib,
  stdenv,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.bsd3,
}:

stdenv.mkDerivation {
  pname = "corebinutils";
  version = "0-unstable";

  src = ../.;

  # Uses its own configure + GNUmakefile, not autotools
  dontAddPrefix = true;

  configurePhase = ''
    runHook preConfigure

    ./configure --prefix=$out --allow-glibc

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild

    make -f GNUmakefile -j$NIX_BUILD_CORES all

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    make -f GNUmakefile install PREFIX=$out

    runHook postInstall
  '';

  meta = {
    description = "Collection of BSD/POSIX core utilities";
    homepage = "https://projecttick.org/";
    inherit license;
    inherit maintainers;
    platforms = lib.platforms.linux;
  };
}
