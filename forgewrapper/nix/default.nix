{
  lib,
  stdenv,
  jdk17,
  gradle,
  stripJavaArchivesHook,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.lgpl3Plus,
}:

stdenv.mkDerivation {
  pname = "forgewrapper";
  version = "projt";

  src = ../.;

  nativeBuildInputs = [
    jdk17
    gradle
    stripJavaArchivesHook
  ];

  buildPhase = ''
    runHook preBuild

    export GRADLE_USER_HOME="$TMPDIR/gradle-home"
    gradle --no-daemon --offline jar

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/forgewrapper
    cp build/libs/forgewrapper-*.jar $out/share/forgewrapper/forgewrapper.jar

    runHook postInstall
  '';

  meta = {
    description = "Minecraft Forge launcher wrapper";
    homepage = "https://projecttick.org/";
    inherit license;
    inherit maintainers;
    platforms = lib.platforms.all;
  };
}
