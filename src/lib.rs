#![crate_name = "rosie_sys"]

#![doc(html_logo_url = "https://rosie-lang.org/images/rosie-circle-blog.png")]
//GOAT, Question for Jamie, can she host a version of this logo that doesn't have a border?  i.e. just the circle occupying the whole frame, with an alpha-channel so the corners are transparent

//! # rosie-sys Overview
//! This crate builds the `librosie` library for the [**Rosie**](https://rosie-lang.org/about/) matching engine and the [**Rosie Pattern Language**](https://gitlab.com/rosie-pattern-language/rosie/-/blob/master/README.md)\(`rpl`\).
//! 
//! The majority of users wishing to use Rosie in Rust should probably use the [rosie-rs crate](https://crates.io/crates/rosie-rs).
//! 

use core::fmt;
use core::fmt::Display;
use std::marker::PhantomData;
use std::ptr;
use std::slice;
use std::str;
use std::convert::TryFrom;
use std::ffi::CString;

extern crate libc;
use libc::{size_t, c_void};

/// A string type used to communicate with librosie (rstr in rosie.h)
/// 
/// Strings in librose can either be allocated by the librosie library or allocated by the client.  The buffer containing
/// the actual bytes therefore must be freed or not freed depending on knowledge of where the string came from.  This
/// makes a straightforward safe wrapper in Rust problematic.  It would be possible to expose a smart wrapper with knowledge
/// about whether a buffer should be freed or not, but this adds extra complexity and overhead.  In fact I already wrote
/// this and then decided against it after seeing how it looked and realizing there was very little need to expose
/// librosie strings to Rust directly.
///
/// RosieString is used for communicating with the C API.  The rosie-rs crate exposes a specialized variant called
/// RosieMessage.  A RosieMessage is a RosieString that was allocated by librosie, but where the librosie client is
/// responsible for freeing it.  Therefore, RosieMessage implements the Rust Drop trait to clean up its buffer when it
/// is no longer needed.
///
/// Simply put, RosieString doesn't own its buffer, and it's basically a glorified pointer.  RosieMessage does own its
/// buffer, and frees it when dropped.  But the memory layout of both structures is identical.
///
#[derive(Debug, Copy, Clone)]
#[repr(C)]
pub struct RosieString<'a> {
    len: u32,
    ptr: *const u8, //This pointer really has a lifetime of 'a, hence the phantom
    phantom: PhantomData<&'a u8>,
}

impl RosieString<'_> {
    pub fn manual_drop(&mut self) {
        if self.ptr != ptr::null() {
            unsafe { rosie_free_string(*self); }
            self.len = 0;
            self.ptr = ptr::null();
        }
    }
    pub fn empty() -> RosieString<'static> {
        RosieString {
            len: 0,
            ptr: ptr::null(),
            phantom: PhantomData
        }
    }
    pub fn into_bytes<'a>(self) -> &'a[u8] {
        if self.ptr != ptr::null() {
            unsafe{ slice::from_raw_parts(self.ptr, usize::try_from(self.len).unwrap()) }
        } else {
            "".as_bytes()
        }
    }
    pub fn into_str<'a>(self) -> &'a str {
        str::from_utf8(self.into_bytes()).unwrap()
    }
    pub fn from_str<'a>(s: &'a str) -> RosieString<'a> {
        unsafe { rosie_string_from(s.as_ptr(), s.len()) }
    }
    pub fn is_valid(&self) -> bool {
        self.ptr != ptr::null()
    }
    pub fn as_bytes(&self) -> &[u8] {
        if self.ptr != ptr::null() {
            unsafe{ slice::from_raw_parts(self.ptr, usize::try_from(self.len).unwrap()) }
        } else {
            "".as_bytes()
        }
    }
    pub fn as_str(&self) -> &str {
        let string_slice = self.as_bytes();
        str::from_utf8(string_slice).unwrap()
    }
    pub fn len(&self) -> usize {
        usize::try_from(self.len).unwrap()
    }
}

impl Display for RosieString<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

/// An error code from a Rosie operation 
//
//WARNING!!!!  This enum is shadowed in the rosie-rs crate, in the `src/sys_shadow.rs` file.  DON'T MODIFY IT WITHOUT UPDATING THE SHADOW
#[derive(Copy, Clone, Debug, Hash, Eq, PartialEq)]
pub enum RosieError {
    /// No error occurred.
    Success = 0,
    /// An unknown error occurred.
    MiscErr = -1,
    /// The Rosie Engine could not allocate the needed memory, either because the system allocator failed or because the limit
    /// set by [rosie_alloc_limit] was reached.
    OutOfMemory = -2,
    /// A system API call failed.
    SysCallFailed = -3,
    /// A failure occurred in the `librosie` engine.
    EngineCallFailed = -4,
    /// An error related to a pattern input has occurred, for example, an `rpl` syntax error.
    PatternError = -1001,
    /// An error related to a package input has occurred, for example a missing package or `.rpl` file,
    /// a missing package declaration in the file, or another syntax error in the package.rpl file.
    PackageError = -1002,
    /// An invalid argument was passed to a rosie function.
    ArgError = -1003,
}

impl RosieError {
    pub fn from(code: i32) -> Self {
        match code {
            0 => RosieError::Success,
            -2 => RosieError::OutOfMemory,
            -3 => RosieError::SysCallFailed,
            -4 => RosieError::EngineCallFailed,
            _ => RosieError::MiscErr
        }
    }
}

/// An Encoder Module used to format the results, when using [rosie_match].
#[derive(Clone, Debug, Hash, Eq, PartialEq)]
pub enum MatchEncoder {
    /// The simplest and fastest encoder.  Outputs `true` if the pattern matched and `false` otherwise.
    Bool,
    /// A compact encoding of the match information into an array of bytes.
    Byte,
    /// A human-readable format using ANSI text coloring for different elements.  The colors associated with each element
    /// can be customized by **Q-04.02 QUESTION FOR A ROSIE EXPERT: Where is this documented?**
    Color,
    /// A json-encoded match structure.
    JSON,
    /// The same data as [JSON](MatchEncoder::JSON), except formatted for better readability.
    JSONPretty,
    /// Each matching line from the input will be a line in the output.
    Line,
    /// The matching subset of each input line will be a line in the output.
    Matches,
    /// The subset of the input matched by each sub-expression of the pattern will be a line in the output.
    Subs,
    /// The output of a custom encoder module, implemented in `Lua`.  Documentation on implementing encoder modules can
    /// be found **Q-04.03 QUESTION FOR A ROSIE EXPERT: Where is this documented?**
    Custom(CString),
}

/// [MatchEncoder] methods to interface with librosie.
/// 
/// The purpose of this trait, as opposed to including the methods in `impl MatchEncoder` is that [MatchEncoder] will
/// be re-exported by rosie-rs, whereas these methods are used inside the implementation of rosie-rs.
pub trait LibRosieMatchEncoder {
    fn as_bytes(&self) -> &[u8];
}

impl LibRosieMatchEncoder for MatchEncoder {
    fn as_bytes(&self) -> &[u8] {
        match self {
            MatchEncoder::Bool => b"bool\0",
            MatchEncoder::Byte => b"byte\0",
            MatchEncoder::Color => b"color\0",
            MatchEncoder::JSON => b"json\0",
            MatchEncoder::JSONPretty => b"jsonpp\0",
            MatchEncoder::Line => b"line\0",
            MatchEncoder::Matches => b"matches\0",
            MatchEncoder::Subs => b"subs\0",
            MatchEncoder::Custom(name) => name.as_bytes_with_nul(),
        }
    }
}

impl MatchEncoder {
    /// Create a MatchEncoder, that calls the `Lua` function name specified by the argument
    pub fn custom(name : &str) -> Self {
        MatchEncoder::Custom(CString::new(name.as_bytes()).unwrap())
    }
}

/// A format for debugging output, to be used with [rosie_trace]
#[derive(Clone, Debug, Hash, Eq, PartialEq)]
pub enum TraceFormat {
    /// The complete trace data, formatted as JSON 
    JSON,
    /// The complete trace data, formatted in a multi-line human-readable format
    Full,
    /// Redacted trace data, containing only the parts of the expression most useful for understanding why a
    /// pattern failed to match, presented in a multi-line human-readable format
    Condensed
}

/// [TraceFormat] methods to interface with librosie.
/// 
/// The purpose of this trait, as opposed to including the methods in `impl TraceFormat` is that [TraceFormat] will
/// be re-exported by rosie-rs, whereas these methods are used inside the implementation of rosie-rs.
pub trait LibRosieTraceFormat {
    fn as_bytes(&self) -> &[u8];
}

impl LibRosieTraceFormat for TraceFormat {
    fn as_bytes(&self) -> &[u8] {
        match self {
            TraceFormat::JSON => b"json\0",
            TraceFormat::Full => b"full\0",
            TraceFormat::Condensed => b"condensed\0"
        }
    }
}

/// A pointer to a Rosie engine.  
/// 
/// EnginePtr should NOT be re-exported as it contains no safe interfaces
/// 
/// **NOTE**: RosieEngines are not internally thread-safe, but you may create more than one RosieEngine in order to use multiple threads.
/// **NOTE**: Cloning / Copying this ptr type does not copy the engine, just the reference to the engine.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EnginePtr {
    pub e: *mut c_void, //This pointer really has a lifetime of 'a, hence the phantom
}

/// A structure containing the match results from a [rosie_match] call.
/// 
/// **NOTE**: A RawMatchResult points to memory inside the engine that is associated with the pattern, therefore you may
/// not perform any additional matching with that pattern until the RawMatchResult has been released.  This is enforced with
/// borrowing semantics in the rosie-rs crate's `Pattern::match_raw` method, but in the sys crate it's on your honor.
//GOAT, This text is appearing twice.  Not good
#[repr(C)]
#[derive(Debug)]
pub struct RawMatchResult<'a> {
    data: RosieString<'a>,
    leftover: i32,
    abend: i32,
    ttotal: i32,
    tmatch: i32
}

/// [RawMatchResult] methods to interface with librosie.
/// 
/// The purpose of this trait, as opposed to including the methods in `impl RawMatchResult` is that [RawMatchResult] will
/// be re-exported by rosie-rs, whereas these methods are used inside the implementation of rosie-rs.
pub trait LibRosieMatchResult {
    fn empty() -> Self;
}

impl LibRosieMatchResult for RawMatchResult<'_> {
    fn empty() -> Self {
        Self {
            data: RosieString::empty(),
            leftover: 0,
            abend: 0,
            ttotal: 0,
            tmatch: 0
        }
    }
}

impl <'a>RawMatchResult<'a> {
    /// Returns `true` if the pattern was matched in the input, otherwise returns `false`.
    pub fn did_match(&self) -> bool {
        if self.data.is_valid() {
            return true;
        }
        //The "bool" encoder returns 1 in the len field to indicate a match, even if the ptr is NULL
        if self.data.len() == 1 {
            return true;
        }
        false
    }
    /// Returns the raw buffer, outputted by the encoder during the match operation
    pub fn as_bytes(&self) -> &[u8] {
        self.data.as_bytes()
    }
    /// Returns the raw buffer, outputted by the encoder during the match operation, consuming the RawMatchResult
    pub fn into_bytes(self) -> &'a [u8] {
        self.data.into_bytes()
    }
    /// Returns the match buffer, interpreted as a UTF-8 string
    pub fn as_str(&self) -> &str {
        self.data.as_str()
    }
    /// Returns the match buffer, interpreted as a UTF-8 string, consuming the RawMatchResult
    pub fn into_str(self) -> &'a str {
        self.data.into_str()
    }
    /// Returns the total time, in microseconds, elapsed during the call to [rosie_match] inside librosie.
    pub fn time_elapsed_total(&self) -> usize {
        usize::try_from(self.ttotal).unwrap()
    }
    /// Returns the time, in microseconds, elapsed matching the pattern against the input.
    /// 
    /// This value excludes time spend encoding the results
    pub fn time_elapsed_matching(&self) -> usize {
        usize::try_from(self.tmatch).unwrap()
    }
}

/// Returns the path to a rosie_home dir, that is valid at the time the rosie-sys crate is built
/// 
/// The purpose of this function is so that a rosie-rs crate can operate without needing to be configured on
/// systems where the rosie_home dir isn't installed.  This is not a substitute for installing the
/// rosie_home dir in a more appropriate location
///
/// TODO: In the future, we should embed the CONTENTS of the rosie_home into the binary, not just the path
pub fn rosie_home_default() -> Option<&'static str> {

    option_env!("ROSIE_HOME")
}

//Interfaces to the raw librosie functions
//NOTE: Not all interfaces are imported by the Rust driver
//NOTE: The 'static lifetime in the returned values is a LIE! The calling code needs to assign the lifetimes appropriately
extern "C" {
    pub fn rosie_new_string(msg : *const u8, len : size_t) -> RosieString<'static>; // str rosie_new_string(byte_ptr msg, size_t len);
    // str *rosie_new_string_ptr(byte_ptr msg, size_t len);
    // str *rosie_string_ptr_from(byte_ptr msg, size_t len);
    pub fn rosie_string_from(msg : *const u8, len : size_t) -> RosieString<'static>; // str rosie_string_from(byte_ptr msg, size_t len);
    pub fn rosie_free_string(s : RosieString); // void rosie_free_string(str s);
    // void rosie_free_string_ptr(str *s);

    pub fn rosie_home_init(home : *const RosieString, messages : *mut RosieString); // void rosie_home_init(str *runtime, str *messages);
    pub fn rosie_new(messages : *mut RosieString) -> EnginePtr; // Engine *rosie_new(str *messages);
    pub fn rosie_finalize(e : EnginePtr); // void rosie_finalize(Engine *e);
    pub fn rosie_libpath(e : EnginePtr, newpath : *mut RosieString) -> i32;// int rosie_libpath(Engine *e, str *newpath);
    pub fn rosie_alloc_limit(e : EnginePtr, newlimit : *mut i32, usage : *mut i32) -> i32;// int rosie_alloc_limit(Engine *e, int *newlimit, int *usage);
    pub fn rosie_config(e : EnginePtr, retvals : *mut RosieString) -> i32;// int rosie_config(Engine *e, str *retvals);
    pub fn rosie_compile(e : EnginePtr, expression : *const RosieString, pat : *mut i32, messages : *mut RosieString) -> i32; // int rosie_compile(Engine *e, str *expression, int *pat, str *messages);
    pub fn rosie_free_rplx(e : EnginePtr, pat : i32) -> i32; // int rosie_free_rplx(Engine *e, int pat);
    pub fn rosie_match(e : EnginePtr, pat : i32, start : i32, encoder : *const u8, input : *const RosieString, match_result : *mut RawMatchResult) -> i32; // int rosie_match(Engine *e, int pat, int start, char *encoder, str *input, match *match);
    pub fn rosie_match2(e : EnginePtr, pat : i32, encoder_name : *const u8, input : *const RosieString, startpos : u32, endpos : u32, match_result : *mut RawMatchResult, collect_times : u8) -> i32; //int rosie_match2(Engine *e, uint32_t pat, char *encoder_name, str *input, uint32_t startpos, uint32_t endpos, struct rosie_matchresult *match, uint8_t collect_times);
    //pub fn rosie_matchfile(e : EnginePtr, pat : i32, encoder : *const u8, wholefileflag : i32, infilename : *const u8, outfilename : *const u8, errfilename : *const u8, cin : *mut i32, cout : *mut i32, cerr : *mut i32, err : *mut RosieString); // int rosie_matchfile(Engine *e, int pat, char *encoder, int wholefileflag, char *infilename, char *outfilename, char *errfilename, int *cin, int *cout, int *cerr, str *err);
    pub fn rosie_trace(e : EnginePtr, pat : i32, start : i32, trace_style : *const u8, input : *const RosieString, matched : &mut i32, trace : *mut RosieString) -> i32; // int rosie_trace(Engine *e, int pat, int start, char *trace_style, str *input, int *matched, str *trace);
    pub fn rosie_load(e : EnginePtr, ok : *mut i32, rpl_text : *const RosieString, pkgname : *mut RosieString, messages : *mut RosieString) -> i32; // int rosie_load(Engine *e, int *ok, str *src, str *pkgname, str *messages);
    pub fn rosie_loadfile(e : EnginePtr, ok : *mut i32, file_name : *const RosieString, pkgname : *mut RosieString, messages : *mut RosieString) -> i32; // int rosie_loadfile(Engine *e, int *ok, str *fn, str *pkgname, str *messages);
    pub fn rosie_import(e : EnginePtr, ok : *mut i32, pkgname : *const RosieString, as_name : *const RosieString, actual_pkgname : *mut RosieString, messages : *mut RosieString) -> i32; // int rosie_import(Engine *e, int *ok, str *pkgname, str *as, str *actual_pkgname, str *messages);
    // int rosie_read_rcfile(Engine *e, str *filename, int *file_exists, str *options, str *messages);
    // int rosie_execute_rcfile(Engine *e, str *filename, int *file_exists, int *no_errors, str *messages);

    pub fn rosie_expression_refs(e : EnginePtr, expression : *const RosieString, refs : *mut RosieString, messages : *mut RosieString) -> i32; // int rosie_expression_refs(Engine *e, str *input, str *refs, str *messages);
    // int rosie_block_refs(Engine *e, str *input, str *refs, str *messages);
    pub fn rosie_expression_deps(e : EnginePtr, expression : *const RosieString, deps : *mut RosieString, messages : *mut RosieString) -> i32; // int rosie_expression_deps(Engine *e, str *input, str *deps, str *messages);
    // int rosie_block_deps(Engine *e, str *input, str *deps, str *messages);
    // int rosie_parse_expression(Engine *e, str *input, str *parsetree, str *messages);
    // int rosie_parse_block(Engine *e, str *input, str *parsetree, str *messages);
}

#[test]
/// Tests RosieString, but not RosieMessage
fn rosie_string() {

    //A basic RosieString, pointing to a static string
    let hello_str = "hello";
    let rosie_string = RosieString::from_str(hello_str);
    assert_eq!(rosie_string.len(), hello_str.len());
    assert_eq!(rosie_string.as_str(), hello_str);

    //A RosieString pointing to a heap-allocated string
    let hello_string = String::from("hi there");
    let rosie_string = RosieString::from_str(hello_string.as_str());
    assert_eq!(rosie_string.len(), hello_string.len());
    assert_eq!(rosie_string.as_str(), hello_string);

    //Ensure we can't deallocate our rust String without deallocating our RosieString first
    drop(hello_string);
    //TODO: Implement a TryBuild harness in order to ensure the line below will not compile 
    //assert!(rosie_string.is_valid());
}

#[test]
/// Tests the native (unsafe) librosie function, mainly to make sure it built and linked properly
fn librosie() {

    //WARNING: I'm not doing a thorough job with error handling and cleanup in this test.
    // This is NOT a good template to use for proper use of Rosie.  You really should
    // use the rosie-rs crate to call Rosie from Rust.

    //Init the Rosie home directory, if we have the rosie_home_default()
    let mut message_buf = RosieString::empty();
    if let Some(rosie_home_dir) = rosie_home_default() {
        unsafe{ rosie_home_init(&RosieString::from_str(&rosie_home_dir), &mut message_buf) };
    }
    message_buf.manual_drop();

    //Create the rosie engine with rosie_new
    let mut message_buf = RosieString::empty();
    let engine = unsafe { rosie_new(&mut message_buf) };
    message_buf.manual_drop();

    //Check the libpath is relative to the directory we set, if we set a path
    let mut path_rosie_string = RosieString::empty();
    let result_code = unsafe { rosie_libpath(engine, &mut path_rosie_string) };
    assert_eq!(result_code, 0);
    if let Some(rosie_home_dir) = rosie_home_default() {
        assert_eq!(path_rosie_string.as_str(), format!("{}/rpl", rosie_home_dir));
    }
    path_rosie_string.manual_drop();

    //Compile a valid rpl pattern, and confirm there is no error
    let mut message_buf = RosieString::empty();
    let mut pat_idx : i32 = 0;
    let expression_rosie_string = RosieString::from_str("{[012][0-9]}");
    let result_code = unsafe { rosie_compile(engine, &expression_rosie_string, &mut pat_idx, &mut message_buf) };
    message_buf.manual_drop();
    assert_eq!(result_code, 0);

    //Match the pattern against a matching input using rosie_match
    let input_rosie_string = RosieString::from_str("21");
    let mut raw_match_result = RawMatchResult::empty();
    let result_code = unsafe{ rosie_match(engine, pat_idx, 1, MatchEncoder::Bool.as_bytes().as_ptr(), &input_rosie_string, &mut raw_match_result) }; 
    assert_eq!(result_code, 0);
    assert_eq!(raw_match_result.did_match(), true);
    assert!(raw_match_result.time_elapsed_matching() <= raw_match_result.time_elapsed_total()); //A little lame as tests go, but validates they are called at least.

    //Make sure we can sucessfully free the pattern
    let result_code = unsafe { rosie_free_rplx(engine, pat_idx) };
    assert_eq!(result_code, 0);

    //Get the refs for a pattern expression that references a symbol from the Standard Pattern Library
    let expression_rosie_string = RosieString::from_str("date.us_long");
    let mut refs_buf = RosieString::empty();
    let mut message_buf = RosieString::empty();
    let result_code = unsafe { rosie_expression_refs(engine, &expression_rosie_string, &mut refs_buf, &mut message_buf) };
    assert_eq!(result_code, 0);
    assert_eq!(message_buf.len(), 0);
    refs_buf.manual_drop();
    message_buf.manual_drop();

    //Get the deps from the pattern
    let mut deps_buf = RosieString::empty();
    let mut message_buf = RosieString::empty();
    let result_code = unsafe { rosie_expression_deps(engine, &expression_rosie_string, &mut deps_buf, &mut message_buf) };
    assert_eq!(result_code, 0);
    assert_eq!(message_buf.len(), 0);
    assert_eq!(deps_buf.as_str(), "[\"date\"]");
    deps_buf.manual_drop();
    message_buf.manual_drop();

    //Clean up the engine with rosie_finalize
    unsafe{ rosie_finalize(engine); }
}
