{
  description = " Project Tick is a project dedicated to providing developers with ease of use and users with long-lasting software.";

  nixConfig = {
    extra-substituters = [ "https://meshmc.cachix.org" ];
    extra-trusted-public-keys = [
      "meshmc.cachix.org-1:6ZNLcfqjVDKmN9/XNWGV3kcjBTL51v1v2V+cvanMkZA="
    ];
  };

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixos-unstable/nixexprs.tar.xz";
  };

  outputs =
    {
      self,
      nixpkgs,
    }:

    let
      inherit (nixpkgs) lib;

      # While we only officially support aarch and x86_64 on Linux and MacOS,
      # we expose a reasonable amount of other systems for users who want to
      # build for most exotic platforms
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      forAllSystems = lib.genAttrs systems;
      nixpkgsFor = forAllSystems (system: nixpkgs.legacyPackages.${system});
    in

    {
      overlays.default =
        final: prev:

        let
          llvm = final.llvmPackages_22 or prev.llvmPackages_22;
        in

        {
          # ── Tier 0: No external dependencies ──────────────────────────
          project-tick-tomlplusplus = prev.callPackage ./tomlplusplus/nix/default.nix { };
          optional-bare = prev.callPackage ./optional-bare/nix/default.nix { };
          xz-embedded = prev.callPackage ./xz-embedded/nix/default.nix { };
          project-tick-cmark = prev.callPackage ./cmark/nix/default.nix { };
          neozip = prev.callPackage ./neozip/nix/default.nix { };
          json4cpp = prev.callPackage ./json4cpp/nix/default.nix { };
          genqrcode = prev.callPackage ./genqrcode/nix/default.nix { };
          images4docker = prev.callPackage ./images4docker/nix/default.nix { };

          # ── Tier 1: Qt6 / external dependencies ──────────────────────
          rainbow = prev.callPackage ./rainbow/nix/default.nix { };
          iconfix = prev.callPackage ./iconfix/nix/default.nix { };
          LocalPeer = prev.callPackage ./LocalPeer/nix/default.nix { };
          katabasis = prev.callPackage ./katabasis/nix/default.nix { };
          systeminfo = prev.callPackage ./systeminfo/nix/default.nix { };
          classparser = prev.callPackage ./classparser/nix/default.nix { };

          # ── Tier 2: Internal dependencies ─────────────────────────────
          libnbtplusplus = prev.callPackage ./libnbtplusplus/nix/default.nix {
            inherit (final) neozip;
          };
          ganalytics = prev.callPackage ./ganalytics/nix/default.nix {
            inherit (final) systeminfo;
          };

          # ── Java components ───────────────────────────────────────────
          javacheck = prev.callPackage ./javacheck/nix/default.nix { };
          javalauncher = prev.callPackage ./javalauncher/nix/default.nix { };
          forgewrapper = prev.callPackage ./forgewrapper/nix/default.nix { };

          # ── C applications ────────────────────────────────────────────
          corebinutils = prev.callPackage ./corebinutils/nix/default.nix { };
          cgit = prev.callPackage ./cgit/nix/default.nix { };

          # ── MNV ───────────────────────────────────────────────────────
          mnv-unwrapped = prev.callPackage ./mnv/nix/unwrapped.nix {
            inherit self;
          };
          mnv = final.callPackage ./mnv/nix/wrapper.nix { };

          # ── MeshMC ────────────────────────────────────────────────────
          meshmc-unwrapped = prev.callPackage ./meshmc/nix/unwrapped.nix {
            inherit (llvm) stdenv;
            inherit self;
            inherit (final)
              project-tick-cmark
              project-tick-tomlplusplus
              neozip
              libnbtplusplus
              systeminfo
              ganalytics
              rainbow
              iconfix
              LocalPeer
              classparser
              optional-bare
              xz-embedded
              katabasis
              javacheck
              javalauncher
              ;
          };

          meshmc = final.callPackage ./meshmc/nix/wrapper.nix { };
        };

      packages = forAllSystems (
        system:

        let
          pkgs = nixpkgsFor.${system};

          # Build a scope from our overlay
          tickPackages = lib.makeScope pkgs.newScope (final: self.overlays.default final pkgs);

          packages = {
            # Internal libraries
            inherit (tickPackages)
              project-tick-tomlplusplus
              optional-bare
              xz-embedded
              project-tick-cmark
              neozip
              json4cpp
              genqrcode
              images4docker
              rainbow
              iconfix
              LocalPeer
              katabasis
              systeminfo
              classparser
              libnbtplusplus
              ganalytics
              javacheck
              javalauncher
              forgewrapper
              ;

            # Applications
            inherit (tickPackages) corebinutils cgit;

            # MNV
            inherit (tickPackages) mnv-unwrapped mnv;

            # MeshMC
            inherit (tickPackages) meshmc-unwrapped meshmc;
            default = tickPackages.meshmc;
          };
        in

        # Only output them if they're available on the current system
        lib.filterAttrs (_: lib.meta.availableOn pkgs.stdenv.hostPlatform) packages
      );

      # We put these under legacyPackages as they are meant for CI, not end user consumption
      legacyPackages = forAllSystems (
        system:

        let
          packages' = self.packages.${system};
        in

        rec {
          meshmc-unwrapped-debug = packages'.meshmc-unwrapped.overrideAttrs {
            cmakeBuildType = "Debug";
            dontStrip = true;
          };

          meshmc-debug = packages'.meshmc.override {
            meshmc-unwrapped = meshmc-unwrapped-debug;
          };

          mnv-unwrapped-debug = packages'.mnv-unwrapped.overrideAttrs {
            cmakeBuildType = "Debug";
            dontStrip = true;
          };

          mnv-debug = packages'.mnv.override {
            mnv-unwrapped = mnv-unwrapped-debug;
          };
        }
      );

      devShells = forAllSystems (
        system:

        let
          pkgs = nixpkgsFor.${system};
          llvm = pkgs.llvmPackages_22;
          python = pkgs.python3;
          mkShell = pkgs.mkShell.override { inherit (llvm) stdenv; };

          packages' = self.packages.${system};

          welcomeMessage = ''
            Welcome to Project Tick!
          '';
        in

        {
          default = mkShell {
            name = "project-tick";

            packages = [

              (pkgs.stdenvNoCC.mkDerivation {
                pname = "clang-tidy-diff";
                inherit (llvm.clang) version;

                nativeBuildInputs = [
                  pkgs.installShellFiles
                  python.pkgs.wrapPython
                ];

                dontUnpack = true;
                dontConfigure = true;
                dontBuild = true;

                postInstall = "installBin ${llvm.libclang.python}/share/clang/clang-tidy-diff.py";
                postFixup = "wrapPythonPrograms";
              })
            ];

            shellHook = ''
              git submodule update --init --force

              echo ${lib.escapeShellArg welcomeMessage}
            '';
          };
        }
      );

      checks = forAllSystems (
        system:

        let
          pkgs = nixpkgsFor.${system};
          llvm = pkgs.llvmPackages_22;
        in

        {
          clang-format = pkgs.runCommand "meshmc-clang-format-check"
            {
              nativeBuildInputs = [ llvm.clang-tools pkgs.bash pkgs.ripgrep ];
              src = self;
            }
            ''
              cd "$src"
              bash ./meshmc/scripts/check-clang-format.sh
              touch "$out"
            '';
        }
      );

      formatter = forAllSystems (system: nixpkgsFor.${system}.nixfmt-rfc-style);
    };
}
