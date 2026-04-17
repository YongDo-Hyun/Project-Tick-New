{
  lib,
  stdenv,
  openssl,
  zlib,
  pkg-config,
  asciidoc,
  luajit,
  python3,
  perl,

  enableLua ? true,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.gpl2Only,
}:

stdenv.mkDerivation {
  pname = "cgit";
  version = "10.0.3";

  src = ../.;

  nativeBuildInputs = [
    pkg-config
    asciidoc
    python3
    perl
  ];

  buildInputs = [
    openssl
    zlib
  ]
  ++ lib.optional enableLua luajit;

  makeFlags = [
    "prefix=${placeholder "out"}"
    "CGIT_SCRIPT_PATH=${placeholder "out"}/lib/cgit"
    "CGIT_DATA_PATH=${placeholder "out"}/share/cgit"
    "filterdir=${placeholder "out"}/lib/cgit/filters"
    "NO_CURL=1"
  ]
  ++ lib.optional (!enableLua) "NO_LUA=1";

  # cgit builds inside git subdir using cgit.mk
  preBuild = ''
    # Ensure git submodule sources are present
    if [ ! -f git/Makefile ]; then
      echo "Error: git submodule not checked out" >&2
      exit 1
    fi
  '';

  installFlags = [
    "DESTDIR="
  ];

  meta = {
    description = "Fast web interface for Git repositories";
    homepage = "https://projecttick.org/";
    inherit license;
    inherit maintainers;
    mainProgram = "cgit";
    platforms = lib.platforms.linux;
  };
}
