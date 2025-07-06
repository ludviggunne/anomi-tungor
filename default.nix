{
  pkgs ? import <nixpkgs> {}
}:

pkgs.stdenv.mkDerivation {
  name = "anomi";
  src = ./.;

  buildInputs = with pkgs; [
    pkg-config
    pulseaudio
    libsndfile
    cjson
    portmidi
  ];

  phases = [ "unpackPhase" "buildPhase" "installPhase" ];

  installPhase = ''
    make install PREFIX=$out
  '';
}
