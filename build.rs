
//Depending on the Cargo features, some parts of this file are not run
#![allow(dead_code)]
#![allow(unreachable_code)]

use std::fs; // Used to copy the C headers
use std::path::{PathBuf, Path};

extern crate cc;

#[cfg(feature = "link_shared_librosie")]
extern crate pkg_config;

//TODO: Test on Windows.  I suspect it won't work, and I may need to follow the example from Teseract,
//  using the vcpkg crate, here:
//https://github.com/ccouzens/tesseract-sys/blob/master/build.rs

fn main() {

    //If both or neither "build-mode" features are specified then it's an error
    #[cfg(all(feature = "link_shared_librosie", feature = "build_static_librosie"))]
    {
        panic!("Error: both link_shared_librosie and build_static_librosie are specified");
    }
    #[cfg(not(any(feature = "link_shared_librosie", feature = "build_static_librosie")))]
    {
        panic!("Error: either link_shared_librosie or build_static_librosie must be specified");
    }

    //See if we're linking a shared librosie
    #[cfg(feature = "link_shared_librosie")]
    {
        //First, see if we canc locate the library using pkg_config
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

        //We got here because we failed to find an existing librosie
        panic!("Error: link_shared_librosie specified, but librosie couldn't be found");
    }

    //If we're not linking the shared lib, then we're building a private copy
    #[cfg(feature = "build_static_librosie")]
    {
        //Build librosie from source
        if librosie_src_build() {
            println!("cargo:rustc-link-lib=rosie");
            return;
        }

        //We got here because we failed to find or build librosie.
        panic!("Error: librosie build failure");
    }
}

//If we haven't found it using one of the pkg trackers, try to compile the smoke.c file to "smoke it out"
//Thanks to the zlib crate for this idea:  https://github.com/rust-lang/libz-sys/blob/main/build.rs
fn librosie_installed() -> bool {

    let smoke_file : PathBuf = ["src", "smoke.c"].iter().collect();
    let mut cfg = cc::Build::new();
    cfg.file(smoke_file);
    if cfg.try_compile("smoke").is_ok() {
        return true;
    }

    false
}

//Build the librosie library from source
//
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
fn librosie_src_build() -> bool {

    //Incoming from Cargo
    let manifest_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let out_dir = PathBuf::from(std::env::var_os("OUT_DIR").unwrap());

    //The build products
    let rosie_lib_dir = out_dir.clone(); //cc's default is to put the build lib in the out_dir, and that's fine with us
    let rosie_include_dir = out_dir.join("include");
    let rosie_home_dir = out_dir.join("rosie_home");

    //The C source file locations
    let lua_cjson_dir : PathBuf = ["src", "lua-cjson"].iter().collect();
    let rpeg_src_dir : PathBuf = ["src", "rpeg"].iter().collect();
    let rpeg_compiler_dir = rpeg_src_dir.join("compiler");
    let rpeg_runtime_dir = rpeg_src_dir.join("runtime");
    let librosie_src_dir : PathBuf = ["src", "librosie"].iter().collect();

    //The extra artifacts, included in the manifest
    let rosie_include_src_dir = manifest_dir.join(&librosie_src_dir);
    let rosie_home_src_dir = manifest_dir.join("src").join("rosie_home");

    //Locate the LUA headers and library, which should have been built by the mlua crate dependency
    let lua_include_dir = PathBuf::from(std::env::var_os("DEP_LUA_INCLUDE").unwrap());
    let lua_lib_dir = PathBuf::from(std::env::var_os("DEP_LUA_LIB").unwrap());
    // let lua_a : PathBuf = lua_lib_dir.join("liblua5.3.a");
    
    //This is needed so the librosie we're about to build will succeed in linking with the mlua output lib
    println!("cargo:rustc-link-search={}", lua_lib_dir.display());
    println!("cargo:rustc-link-lib=lua5.3");

    //The C src files needed to build librosie
    let compile_src_files : Vec<PathBuf> = vec![
        // lua-cjson
        lua_cjson_dir.join("fpconv.c"),
        lua_cjson_dir.join("strbuf.c"),
        lua_cjson_dir.join("lua_cjson.c"),

        // rpeg compiler
        rpeg_compiler_dir.join("rbuf.c"),
        rpeg_compiler_dir.join("lpcap.c"),
        rpeg_compiler_dir.join("lptree.c"),
        rpeg_compiler_dir.join("lpcode.c"),
        rpeg_compiler_dir.join("lpprint.c"),

        // rpeg runtime
        rpeg_runtime_dir.join("buf.c"),
        rpeg_runtime_dir.join("capture.c"),
        rpeg_runtime_dir.join("file.c"),
        rpeg_runtime_dir.join("json.c"),
        rpeg_runtime_dir.join("ktable.c"),
        rpeg_runtime_dir.join("rplx.c"),
        rpeg_runtime_dir.join("vm.c"),

        // librosie
        librosie_src_dir.join("librosie.c"),
    ];

    //Invoke the C compiler to perform the librosie build
    let mut cfg = cc::Build::new();
    cfg.static_flag(true);
    cfg.include(lua_include_dir);
    cfg.include(rpeg_src_dir.join("include"));
    cfg.define("LPEG_DEBUG", None); // Needed by the rpeg compiler build
    cfg.define("NDEBUG", None); // Needed by the rpeg compiler and librosie build
    cfg.define("LUA_COMPAT_5_2", None); // Needed by the librosie build
    cfg.files(compile_src_files.iter());
    cfg.compile("rosie");
    println!("cargo:rerun-if-changed=src");
    
    //rosie_home files, to copy into the build output.  Currently these are just copies of the
    //files built by the main Rosie project's Makefile.  This is ok because they all have
    //platform-independent formats.
    let rosie_home_files : Vec<PathBuf> = vec![
        PathBuf::from("CHANGELOG"),
        PathBuf::from("CONTRIBUTORS"),
        PathBuf::from("LICENSE"),
        PathBuf::from("README"),
        PathBuf::from("VERSION"),
        
        //QUESTION: It's highly debatable whether we should include the "extra" files in this crate.
        //CONCLUSION: NO.  They're almsot 1MB, and they aren't used automatically by anything else in
        // rosie-sys or rosie-rs.  If people want them, it's easy enough to get them from the upstream
        // rosie repository.

        //"extra" directory within rosie_home.  Kept for completeness.
        //
        // PathBuf::from("extra").join("WSL").join("rosie_install.sh"),

        // PathBuf::from("extra").join("docker").join("README.md"),
        // PathBuf::from("extra").join("docker").join("arch"),
        // PathBuf::from("extra").join("docker").join("centos"),
        // PathBuf::from("extra").join("docker").join("elementary"),
        // PathBuf::from("extra").join("docker").join("fedora"),
        // PathBuf::from("extra").join("docker").join("nixos"),
        // PathBuf::from("extra").join("docker").join("run"),
        // PathBuf::from("extra").join("docker").join("syslog-2018-09-05.rpl"),
        // PathBuf::from("extra").join("docker").join("ubuntu"),
        // PathBuf::from("extra").join("docker").join("valgrind-fedora"),

        // PathBuf::from("extra").join("emacs").join("rpl-mode.el"),

        // PathBuf::from("extra").join("examples").join("README.md"),
        // PathBuf::from("extra").join("examples").join("anbmcndm.rpl"),
        // PathBuf::from("extra").join("examples").join("anbn.rpl"),
        // PathBuf::from("extra").join("examples").join("anbncn.rpl"),
        // PathBuf::from("extra").join("examples").join("anbncndn.rpl"),
        // PathBuf::from("extra").join("examples").join("backref.rpl"),
        // PathBuf::from("extra").join("examples").join("distinct.rpl"),
        // PathBuf::from("extra").join("examples").join("dyck.rpl"),
        // PathBuf::from("extra").join("examples").join("html.rpl"),
        // PathBuf::from("extra").join("examples").join("ipv4.py"),
        // PathBuf::from("extra").join("examples").join("reverse.rpl"),
        // PathBuf::from("extra").join("examples").join("sloc.py"),
        // PathBuf::from("extra").join("examples").join("sloc_C.sh"),
        // PathBuf::from("extra").join("examples").join("sloc_lua.sh"),

        // PathBuf::from("extra").join("examples").join("images").join("p1.gif"),
        // PathBuf::from("extra").join("examples").join("images").join("readme-fig1.png"),
        // PathBuf::from("extra").join("examples").join("images").join("readme-fig2.png"),
        // PathBuf::from("extra").join("examples").join("images").join("readme-fig3.png"),
        // PathBuf::from("extra").join("examples").join("images").join("readme-fig4.png"),
        // PathBuf::from("extra").join("examples").join("images").join("readme-fig5.png"),

        // PathBuf::from("extra").join("vim").join("ftdetect").join("rosie.vim"),

        // PathBuf::from("extra").join("vim").join("syntax").join("rosie.vim"),

        PathBuf::from("lib").join("argparse.luac"),
        PathBuf::from("lib").join("ast.luac"),
        PathBuf::from("lib").join("boot.luac"),
        PathBuf::from("lib").join("builtins.luac"),
        PathBuf::from("lib").join("cli-common.luac"),
        PathBuf::from("lib").join("cli-match.luac"),
        PathBuf::from("lib").join("cli-parser.luac"),
        PathBuf::from("lib").join("cli.luac"),
        PathBuf::from("lib").join("color.luac"),
        PathBuf::from("lib").join("common.luac"),
        PathBuf::from("lib").join("compile.luac"),
        PathBuf::from("lib").join("engine_module.luac"),
        PathBuf::from("lib").join("environment.luac"),
        PathBuf::from("lib").join("expand.luac"),
        PathBuf::from("lib").join("expr.luac"),
        PathBuf::from("lib").join("infix.luac"),
        PathBuf::from("lib").join("init.luac"),
        PathBuf::from("lib").join("list.luac"),
        PathBuf::from("lib").join("loadpkg.luac"),
        PathBuf::from("lib").join("parse.luac"),
        PathBuf::from("lib").join("parse_core.luac"),
        PathBuf::from("lib").join("rcfile.luac"),
        PathBuf::from("lib").join("recordtype.luac"),
        PathBuf::from("lib").join("repl.luac"),
        PathBuf::from("lib").join("strict.luac"),
        PathBuf::from("lib").join("submodule.luac"),
        PathBuf::from("lib").join("thread.luac"),
        PathBuf::from("lib").join("trace.luac"),
        PathBuf::from("lib").join("ui.luac"),
        PathBuf::from("lib").join("unittest.luac"),
        PathBuf::from("lib").join("ustring.luac"),
        PathBuf::from("lib").join("util.luac"),
        PathBuf::from("lib").join("violation.luac"),
        PathBuf::from("lib").join("writer.luac"),

        PathBuf::from("rpl").join("all.rpl"),
        PathBuf::from("rpl").join("char.rpl"),
        PathBuf::from("rpl").join("csv.rpl"),
        PathBuf::from("rpl").join("date.rpl"),
        PathBuf::from("rpl").join("id.rpl"),
        PathBuf::from("rpl").join("json.rpl"),
        PathBuf::from("rpl").join("net.rpl"),
        PathBuf::from("rpl").join("num.rpl"),
        PathBuf::from("rpl").join("os.rpl"),
        PathBuf::from("rpl").join("re.rpl"),
        PathBuf::from("rpl").join("time.rpl"),
        PathBuf::from("rpl").join("ts.rpl"),
        PathBuf::from("rpl").join("ver.rpl"),
        PathBuf::from("rpl").join("word.rpl"),

        PathBuf::from("rpl").join("Unicode").join("Ascii.rpl"),
        PathBuf::from("rpl").join("Unicode").join("Block.rpl"),
        PathBuf::from("rpl").join("Unicode").join("Category.rpl"),
        PathBuf::from("rpl").join("Unicode").join("GraphemeBreak.rpl"),
        PathBuf::from("rpl").join("Unicode").join("LineBreak.rpl"),
        PathBuf::from("rpl").join("Unicode").join("NumericType.rpl"),
        PathBuf::from("rpl").join("Unicode").join("Property.rpl"),
        PathBuf::from("rpl").join("Unicode").join("Script.rpl"),
        PathBuf::from("rpl").join("Unicode").join("SentenceBreak.rpl"),
        PathBuf::from("rpl").join("Unicode").join("WordBreak.rpl"),

        PathBuf::from("rpl").join("builtin").join("prelude.rpl"),

        PathBuf::from("rpl").join("rosie").join("rcfile.rpl"),
        PathBuf::from("rpl").join("rosie").join("rpl_1_1.rpl"),
        PathBuf::from("rpl").join("rosie").join("rpl_1_2.rpl"),
        PathBuf::from("rpl").join("rosie").join("rpl_1_3.rpl"),
    ];

    //Copy the rosie_home files to the build artifacts output dir
    create_empty_dir(&rosie_home_dir).unwrap();
    for file in &rosie_home_files {
        let dest_path = rosie_home_dir.join(file);
        create_parent_dir(&dest_path).unwrap();
        fs::copy(rosie_home_src_dir.join(file), dest_path).unwrap();
    }

    //Set the env variable so we'll be able to find the copy of the rosie_home in the build output
    println!("cargo:rustc-env=ROSIE_HOME_DIR={}", rosie_home_dir.display());
    
    //Copy the rosie header file(s) so anyone who needs to use this crate for a C dependency build against it
    create_empty_dir(&rosie_include_dir).unwrap();
    for include_file in &["librosie.h"] {
        fs::copy(rosie_include_src_dir.join(include_file), rosie_include_dir.join(include_file)).unwrap();
    }
    //Tell cargo how to set `DEP_ROSIE_INCLUDE`
    println!("cargo:include={}", rosie_include_dir.display());

    //Tell cargo so anyone who depends on this crate can find our lib.  I.e. so cargo will set `DEP_ROSIE_LIB` 
    println!("cargo:lib={}", rosie_lib_dir.display());


    //GOAT, NEED to write a README file detailing every component that I harvested from the main Rosie repo
    //  GOAT, Document the link_shared_librosie & build_static_librosie Cargo features.  2 ways to use this crate.
    //  GOAT, Document the DEP_ROSIE_INCLUDE and DEP_ROSIE_LIB env vars, but only if link_shared_librosie is not set

    //GOAT, Look at fixing the warnings caused by lua-cjson

    //GOAT, in the rosie-rs crate, check the ROSIE_HOME_DIR environment variable when we load a new engine with the defaults (i.e. without explicitly specifying a lib)

    true
}

fn create_empty_dir<P: AsRef<Path>>(path: P) -> Result<(), std::io::Error> {
    if path.as_ref().exists() {
        fs::remove_dir_all(&path)?;
    }
    fs::create_dir_all(&path)
}

fn create_parent_dir<P: AsRef<Path>>(path: P) -> Result<(), std::io::Error> {
    let parent = path.as_ref().parent().ok_or(std::io::Error::new(std::io::ErrorKind::InvalidInput, format!("Can't get parent of path: {}", path.as_ref().display())))?;
    fs::create_dir_all(&parent)
}



