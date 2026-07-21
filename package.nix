{
  lib,
  stdenv,
  pkg-config,
  taglib,
  fftw,
  libebur128,
  libsndfile,
  ffmpeg,
  curl,
}:

stdenv.mkDerivation {
  pname = "bitfake2";
  version = "2.1";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [
    pkg-config
  ];

  buildInputs = [
    taglib
    fftw
    libebur128
    libsndfile
    ffmpeg
    curl
  ];

  installFlags = [ "PREFIX=${placeholder "out"}" ];

  meta.mainProgram = "bitf";
}
