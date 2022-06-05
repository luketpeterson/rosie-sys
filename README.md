
# rosie-sys Overview
This crate builds or links the `librosie` library for the [**Rosie**](https://rosie-lang.org/about/) matching engine and the [**Rosie Pattern Language**](https://gitlab.com/rosie-pattern-language/rosie/-/blob/master/README.md)\(`rpl`\).

The majority of users wishing to use Rosie in Rust should probably use the [rosie crate](https://github.com/luketpeterson/rosie-rs).

# Building vs. Linking librosie
This crate can either build `librosie` from source or link to an existing library.

## Building from source

To build librosie, specify the `build_static_librosie` feature in the `[dependencies]` section of your `Cargo.toml`:

`rosie-sys = { features = ["build_static_librosie"] }`

### Building rosie as a C / FFI dependency

If you're using this crate to build the dependency for another `C` (non-Rust) project, you can access the rosie header files
from your Cargo build script using the `DEP_ROSIE_INCLUDE` environment variable.  Also, the location of the built `librosie` library
will be specified by the `DEP_ROSIE_LIB` environment var.

### Installation & Deployment

Rosie depends on a `rosie_home` directory, containing support files including the Standard Pattern Library. This crate produces
the needed files, which can be obtained from the build `out` directory, or from the location specified by the `ROSIE_HOME`
environment variable, accessible to a Cargo build script.

When deploying an app that uses Rosie, you must ensure that these files are installed somewhere on the target machine.  The
cannonical location on unix-like systems is: `/usr/local/lib/rosie/` although you may install them elsewhere.  For example,
they may be placed inside your app directory or within a Mac OS X `.app` bundle directory.

If the files are installed in a non-standard location, you must initialize Rosie with the path, by calling [rosie_home_init].
Alternatively, if you are using the high-level `rosie` crate, call `Rosie::set_rosie_home_path()`.

## Linking to a shared rosie library

To link against a shared librosie, already installed on the system, add the following to the `[dependencies]` section of your `Cargo.toml`:

`rosie-sys = { features = ["link_shared_librosie"] }`

### Obtaining Rosie outside Cargo

Complete info on obtaining and building Rosie is [here](https://gitlab.com/rosie-pattern-language/rosie#local-installation).
However, Rosie may also be available through your package-manager of choice.  For example, if your OS supports it, you may run one of the following:

<!-- * `apt-get install rosie`  Q-01.05 QUESTION apt packaged needed!! -->
* `dnf install rosie`
* `brew install rosie`

Or if you would prefer to install Rosie via the Makefile, [Look Here](https://rosie-lang.org/blog/2020/05/03/new-build.html).

**NOTE**: This crate has been tested aganst `librosie` version **1.3.0**, although it may be compatible with other versions.

# Updating the Rosie source inside this crate

Many files within this crate are copied from either the main [Rosie source](https://gitlab.com/rosie-pattern-language/rosie)
or the build products.

Here is the script for refreshing the source files.  This assumes the `rosie-sys` project is checked out as a peer of the
`rosie` main project, and that the rosie project has been built sucessfully, which pulls the submodules and creates the
`rosie_home` output files.

```sh
cp -r rosie/src/librosie rosie-sys/src/librosie
cp -r rosie/src/rpeg/compiler rosie-sys/rpeg/compiler
cp -r rosie/src/rpeg/include rosie-sys/rpeg/include
cp -r rosie/src/rpeg/runtime rosie-sys/rpeg/runtime
cp -r rosie/submodules/lua-cjson rosie-sys/src/lua-cjson
cp rosie/build/lib/rosie/CHANGELOG rosie-sys/src/rosie_home/CHANGELOG
cp rosie/build/lib/rosie/CONTRIBUTORS rosie-sys/src/rosie_home/CONTRIBUTORS
cp rosie/build/lib/rosie/LICENSE rosie-sys/src/rosie_home/LICENSE
cp rosie/build/lib/rosie/README rosie-sys/src/rosie_home/README
cp rosie/build/lib/rosie/VERSION rosie-sys/src/rosie_home/VERSION
cp -r rosie/build/lib/rosie/lib rosie-sys/src/rosie_home/lib
cp -r rosie/build/lib/rosie/rpl rosie-sys/src/rosie_home/rpl
```

**NOTE** The lua-cjson files have some changes to squish a few warnings, that are not propagated upstream.  Since it's
unlikely to change, perhaps skip taking lua-cjson from upstream.
