{ pkgs ? import <nixpkgs> {} }:

with pkgs;

let
  # removed from nixpkgs ...
  boost160 = callPackage ./nix/boost/1.60/1.60.nix { };
  # build error: ./boost/math/cstdfloat/cstdfloat_limits.hpp:35:13: error: redefinition of 'class std::numeric_limits<__float128>'
  boost165 = callPackage ./nix/boost/1.65/1.65.nix { };
  # https://github.com/NixOS/nixpkgs/raw/824421b1796332ad1bcb35bc7855da832c43305f/pkgs/development/libraries/boost/1.69.nix
  boost169 = callPackage ./nix/boost/1.69/1.69.nix { };
  # build error: ./boost/math/cstdfloat/cstdfloat_limits.hpp:45:13: error: redefinition of 'class std::numeric_limits<__float128>'
  # https://github.com/NixOS/nixpkgs/raw/824421b1796332ad1bcb35bc7855da832c43305f/pkgs/development/libraries/boost/1.70.nix
  boost170 = callPackage ./nix/boost/1.70/1.70.nix { };
  # https://github.com/NixOS/nixpkgs/raw/824421b1796332ad1bcb35bc7855da832c43305f/pkgs/development/libraries/boost/1.72.nix
  boost172 = callPackage ./nix/boost/1.72/1.72.nix { };
  # https://github.com/NixOS/nixpkgs/raw/824421b1796332ad1bcb35bc7855da832c43305f/pkgs/development/libraries/boost/1.73.nix
  boost173 = callPackage ./nix/boost/1.73/1.73.nix { }; # build error
  # https://github.com/NixOS/nixpkgs/raw/824421b1796332ad1bcb35bc7855da832c43305f/pkgs/development/libraries/boost/1.74.nix
  boost174 = callPackage ./nix/boost/1.74.nix { };
  # https://github.com/NixOS/nixpkgs/blob/7a5cc174569052f935d642b227ecf53584595147/pkgs/development/libraries/boost/1.75.nix
  boost175 = callPackage ./nix/boost/1.75.nix { };
in

mkShell {
  buildInputs = [

    cmake

    # https://github.com/DavidKeller/kademlia/pull/11
    # FIXME build fails with all these boost versions
    # boost160 # Could NOT find Boost: Found unsuitable version "1.60.0", but required is at least "1.65" (found
    # boost165
    # boost169
    # boost170
    # boost172 # fail
    # boost173 # fail
    # boost174
    # boost175
    # boost176 # undefined
    # boost177 # ok
    # boost178
    # boost179
    # boost180
    # boost181
    # boost182
    # boost183
    # boost184 # removed from nixpkgs
    # boost185 # removed from nixpkgs

    # boost186
    # FIXME fix build with boost 1.87
    boost187

    curl

  ];
}
