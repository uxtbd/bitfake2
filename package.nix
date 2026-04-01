{
  lib,
  stdenv,
  taglib,
  fftw,
  libebur128,
  libsndfile,
  ffmpeg,
  curl,
}:

stdenv.mkDerivation {
  pname = "bitfake2";
  version = "0.1.8";

  src = lib.cleanSource ./.;

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
