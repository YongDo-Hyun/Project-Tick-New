{
  lib,
  stdenvNoCC,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.asl20,
}:

stdenvNoCC.mkDerivation {
  pname = "images4docker";
  version = "0-unstable";

  src = ../.;

  dontBuild = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/images4docker
    cp -r dockerfiles/* $out/share/images4docker/

    runHook postInstall
  '';

  meta = {
    description = "Multi-distro Docker images with Qt6 toolchains";
    homepage = "https://projecttick.org/";
    inherit license;
    inherit maintainers;
    platforms = lib.platforms.all;
  };
}
