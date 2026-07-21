{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  inputsFrom = [
    (pkgs.callPackage ./package.nix { })
  ];
}

