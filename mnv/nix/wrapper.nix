{
  lib,
  stdenv,
  symlinkJoin,
  makeWrapper,
  mnv-unwrapped,
  wrapGAppsHook3,

  # Runtime dependencies for GUI
  enableGUI ? false,
  additionalRuntimeLibs ? [ ],
}:

symlinkJoin {
  name = "mnv-${mnv-unwrapped.version}";

  paths = [ mnv-unwrapped ];

  nativeBuildInputs = [
    makeWrapper
  ]
  ++ lib.optional enableGUI wrapGAppsHook3;

  postBuild = ''
    # Ensure runtime files are accessible
    for prog in "$out/bin/mnv" "$out/bin/gmnv"; do
      if [ -f "$prog" ]; then
        wrapProgram "$prog" \
          --set MNVRUNTIME "${mnv-unwrapped}/share/mnv/mnv100" \
          ${lib.optionalString (additionalRuntimeLibs != [ ])
            "--prefix LD_LIBRARY_PATH : ${lib.makeLibraryPath additionalRuntimeLibs}"
          }
      fi
    done
  '';

  meta = {
    inherit (mnv-unwrapped.meta)
      description
      homepage
      license
      maintainers
      mainProgram
      platforms
      ;
  };
}
