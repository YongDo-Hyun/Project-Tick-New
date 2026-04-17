{
  lib,
  stdenv,
  cmake,
  ninja,
  ncurses,
  self,

  # Optional GUI
  gtk3,
  wrapGAppsHook3,
  enableGUI ? false,

  # Optional features
  python3,
  luajit,
  ruby,
  perl,
  tcl,
  libsodium,
  libcanberra-gtk3,
  acl,
  gpm,

  enablePython ? true,
  enableLua ? true,
  enableRuby ? false,
  enablePerl ? false,
  enableTcl ? false,
  enableSodium ? true,
  enableSound ? false,
  enableACL ? stdenv.hostPlatform.isLinux,
  enableGPM ? false,

  # Override points
  maintainers ? [ ],
  license ? lib.licenses.vim,
}:

stdenv.mkDerivation {
  pname = "mnv-unwrapped";
  version = "10.0";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../cmake
      ../src
      ../runtime
      ../pixmaps
      ../nsis
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
  ]
  ++ lib.optional enableGUI wrapGAppsHook3;

  buildInputs = [
    ncurses
  ]
  ++ lib.optional enableGUI gtk3
  ++ lib.optional enablePython python3
  ++ lib.optional enableLua luajit
  ++ lib.optional enableRuby ruby
  ++ lib.optional enablePerl perl
  ++ lib.optional enableTcl tcl
  ++ lib.optional enableSodium libsodium
  ++ lib.optional enableSound libcanberra-gtk3
  ++ lib.optional enableACL acl
  ++ lib.optional enableGPM gpm;

  cmakeFlags = [
    "-DMNV_FEATURE=huge"
    "-DMNV_BUILD_TESTS=OFF"
    (lib.cmakeFeature "MNV_GUI" (if enableGUI then "gtk3" else "none"))
    (lib.cmakeBool "MNV_PYTHON3" enablePython)
    (lib.cmakeBool "MNV_LUA" enableLua)
    (lib.cmakeBool "MNV_RUBY" enableRuby)
    (lib.cmakeBool "MNV_PERL" enablePerl)
    (lib.cmakeBool "MNV_TCL" enableTcl)
    (lib.cmakeBool "MNV_SODIUM" enableSodium)
    (lib.cmakeBool "MNV_SOUND" enableSound)
    (lib.cmakeBool "MNV_ACL" enableACL)
    (lib.cmakeBool "MNV_GPM" enableGPM)
  ];

  meta = {
    description = "MNV - MNV is not Vim, a highly configurable text editor";
    homepage = "https://projecttick.org/";
    inherit license;
    inherit maintainers;
    mainProgram = "mnv";
    platforms = lib.platforms.unix;
  };
}
