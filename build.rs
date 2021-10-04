
//GOAT, Try to the cc compiler directly, using cc::Build::try_compile
use std::process::Command;

use std::path::PathBuf;

extern crate pkg_config;
extern crate cc;

//TODO: Test on Windows.  I suspect it won't work, and I may need to follow the example from Teseract,
//  using the vcpkg crate, here:
//https://github.com/ccouzens/tesseract-sys/blob/master/build.rs

fn main() {

    //First, see if we can locate the library using pkg_config
    let librosie = pkg_config::Config::new()
            .cargo_metadata(true)
            .print_system_libs(true)
            .probe("rosie");
    if librosie.is_ok() {
        //pkg_config should output the necessary output for cargo
        return;
    }

    //If pkg_config didn't find librosie, try one more time to see if it's installed by trying to build something that links it
    if librosie_installed() {
        println!("cargo:rustc-link-lib=rosie");
        return;
    }

    //If it's not installed on the system, build librosie from source
    if librosie_src_build() {
        println!("cargo:rustc-link-lib=rosie");
        return;
    }

    //We got here because we failed to find or build librosie.
    panic!("Build Failure: librosie couldn't be found or built");
}

//If we haven't found it using one of the pkg trackers, try to compile the smoke.c file to "smoke it out"
//Thanks to the zlib crate for this idea:  https://github.com/rust-lang/libz-sys/blob/main/build.rs
fn librosie_installed() -> bool {

    //GOAT, Convert this to use cc, so we don't need to import Command, and convert this to use
    //  PathBuf so we can at least be theoretically safe on Windows

    let cfg = cc::Build::new();
    let compiler = cfg.get_compiler();
    let mut cmd = Command::new(compiler.path());
    cmd.arg("src/smoke.c").arg("-o").arg("/dev/null").arg("-lrosie").arg("-lz");

    println!("running {:?}", cmd);
    if let Ok(status) = cmd.status() {
        if status.success() {
            return true;
        }
    }

    false
}

//Build the librosie library from source
fn librosie_src_build() -> bool {

    //The source file locations
    let lua_cjson_dir : PathBuf = ["src", "lua-cjson"].iter().collect();
    let rpeg_src_dir : PathBuf = ["src", "rpeg"].iter().collect();
    let librosie_src_dir : PathBuf = ["src", "librosie"].iter().collect();

    // //GOAT
    // for var in std::env::vars() {
    //     println!("GOAT {} = {}", var.0, var.1);
    // }

    //Locate the LUA headers and library, which should have been built by the mlua crate dependency
    let lua_include_dir = PathBuf::from(std::env::var_os("DEP_LUA_INCLUDE").unwrap());
    let lua_lib_dir = PathBuf::from(std::env::var_os("DEP_LUA_LIB").unwrap());
    // let lua_a : PathBuf = [lua_lib_dir.to_str().unwrap(), "liblua5.3.a"].iter().collect();

    //This is needed so the librosie we're about to build will succeed in linking with the mlua output lib
    println!("cargo:rustc-link-search={}", lua_lib_dir.to_str().unwrap());
    println!("cargo:rustc-link-lib=lua5.3");

    // It looks like the "make-cmd" crate is in really sad shape.  No updates for the last 6 years, and it's just
    //  a very thin wrapper around std::process::Command anyway.
    //
    // I'm at a crossroads.  Should I (1.) use the Rust cc crate directly or should I (2.) call the host system's
    // `make` using the std::process::Command?
    //
    // 1. Rust cc is better supported by Cargo and will likely be more robust against platform iregularities, but
    //  I lose out on getting to leverage the maintainence done by the librosie (and rpeg) Makefiles.
    // 2. Running the Makefile using std::process::Command preserves the rosie Makefile work, but then I have to
    //  do the "autoconfigure" work myself, which sounds like a nightmare.
    //
    // I think I'll choose 1, because it's the devil I know.  In other words, I don't know what I don't know about
    //  possible configs this might end up needing to run on.  But in the case of 1., I can verify that everything
    //  is building as expected.

    let compile_src_files : Vec<PathBuf> = vec![
        // lua-cjson
        [lua_cjson_dir.to_str().unwrap(), "fpconv.c"].iter().collect(),
        [lua_cjson_dir.to_str().unwrap(), "strbuf.c"].iter().collect(),
        [lua_cjson_dir.to_str().unwrap(), "lua_cjson.c"].iter().collect(),

        // rpeg compiler
        [rpeg_src_dir.to_str().unwrap(), "compiler", "rbuf.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "compiler", "lpcap.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "compiler", "lptree.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "compiler", "lpcode.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "compiler", "lpprint.c"].iter().collect(),

        // rpeg runtime
        [rpeg_src_dir.to_str().unwrap(), "runtime", "buf.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "runtime", "capture.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "runtime", "file.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "runtime", "json.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "runtime", "ktable.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "runtime", "rplx.c"].iter().collect(),
        [rpeg_src_dir.to_str().unwrap(), "runtime", "vm.c"].iter().collect(),

        // librosie
        [librosie_src_dir.to_str().unwrap(), "librosie.c"].iter().collect(),
    ];

    //Invoke the C compiler to perform the librosie build
    let mut cfg = cc::Build::new();
    cfg.static_flag(true);
    cfg.include(lua_include_dir);
    cfg.include([rpeg_src_dir.to_str().unwrap(), "include"].iter().collect::<PathBuf>());
    cfg.define("LPEG_DEBUG", None); // Needed by the rpeg compiler build
    cfg.define("NDEBUG", None); // Needed by the rpeg compiler and librosie build
    cfg.define("LUA_COMPAT_5_2", None); // Needed by the librosie build
    cfg.files(compile_src_files.iter());
    cfg.compile("rosie");
    println!("cargo:rerun-if-changed=src");
    
    //Set the env variable so we'll be able to find the local copy of the rosie lib
    let rosie_runtime_files_path : PathBuf = [std::env::var("CARGO_MANIFEST_DIR").unwrap().as_str(), "rosie_home"].iter().collect();
    println!("cargo:rustc-env=ROSIE_HOME_DIR={}", rosie_runtime_files_path.to_str().unwrap());
    





    //GOAT NEED TO clean up trash comments throughout this file 
    //GOAT NEED TO output the line that tells the crates that depend on this lib where to find it 

    //GOAT NEED TO write some tests, to see if this works
    //GOAT, NEED To Bundle the C Rosie headers, and export the env var to access them.
    //GOAT, NEED to write a README file detailing every component that I harvested from the main Rosie repo
    //  GOAT, Document that I use the installed rosie if it exists.
    //GOAT, NEED to test to see if I get a reasonable error (i.e. if the build falls through to my "can't find or build rosie" message, or just panics.)
    //  If I figure out how to make CC not panic, then get rid of the CMD invocation, and instead use CC to build smoke

    //GOAT, Look at fixing the warnings caused by lua-cjson

    //GOAT, Make a Cargo Feature to specify building (with static linking) or dynamic linking

    //GOAT, in the rosie-rs crate, check the ROSIE_LIB_PATH environment variable when we load a new engine with the defaults (i.e. without explicitly specifying a lib)
    
    true
}




