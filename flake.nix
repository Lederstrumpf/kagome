{
  description = "A Nix-flake for building Kagome";

  inputs.oldnixpkgs.url =
    "https://github.com/NixOS/nixpkgs/archive/c6c17387f7dd5332bec72bb9e76df434ac6b2bff.tar.gz";
  inputs.nixpkgs.url =
    "https://github.com/NixOS/nixpkgs/archive/270dace49bc95a7f88ad187969179ff0d2ba20ed.tar.gz";

  outputs = { self, nixpkgs, oldnixpkgs }:
    let
      supportedSystems =
        [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSupportedSystem = f:
        nixpkgs.lib.genAttrs supportedSystems (system:
          f {
            pkgs = import nixpkgs { inherit system; };
            oldpkgs = import oldnixpkgs { inherit system; };
          });
    in {
      devShells = forEachSupportedSystem ({ pkgs, oldpkgs }: {
        default = pkgs.mkShell.override {
          # Override stdenv in order to change compiler:
          # stdenv = pkgs.clangStdenv;
        } {
          packages = with pkgs; [
            clang-tools
            oldpkgs.cmake
            libseccomp
            ncurses.dev
            python3

            # Rust tools
            rustup
            cargo
            rust-analyzer
          ];

          shellHook = ''
            export RUSTUP_HOME="$PWD/.rustup"
            export CARGO_HOME="$PWD/.cargo"
            export PATH="$CARGO_HOME/bin:$PATH"
            export FLAKE_INITIATED=1

            echo "Using RUSTUP_HOME: $RUSTUP_HOME"
            echo "Using CARGO_HOME: $CARGO_HOME"
            echo "Current PATH: $PATH"

            export RUST_VERSION=1.81.0

            if ! rustup show | grep -q "$RUST_VERSION"; then
                echo "Installing Rust version $RUST_VERSION"
                    rustup toolchain install $RUST_VERSION
                    rustup default $RUST_VERSION
            else
                echo "Rust version $RUST_VERSION already installed and set as default."
                    fi

                    rustup show
          '';

        };
      });
    };
}
