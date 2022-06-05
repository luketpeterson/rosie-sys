/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/* librosie.c    Expose the Rosie API                                        */
/*                                                                           */
/*  © Copyright IBM Corporation 2016, 2017, 2018                             */
/*  © Copyright Jamie A. Jennings 2021                                       */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */

/* Overview
 * 
 * rosie_new() makes a new engine.  An engine is NOT thread-safe, so
 * every thread must have its own engine.  
 *
 * rosie_compile() compiles a pattern and returns an opaque handle
 * which is currently an integer.  (This could change, so it is best
 * to avoid exposing the handle type.)  The handle refers to a
 * compiled RPL pattern, which is called an RPLX object.
 *
 *   NOTE: Each compiled pattern has an associated "match result"
 *   structure that is reused across calls to rosie_match().  The
 *   librosie client code must therefore examine the results of a
 *   match before calling rosie_match() or rosie_matchresult_free().
 *
 * rosie_free_rplx() destroys a compiled pattern (RPLX) and its
 * associated match result structure.
 * 
 * rosie_import(), rosie_load(), rosie_loadfile() are functions which
 * create new pattern definitions, i.e. they create or update names in
 * the engine environment, where each name is bound to a pattern.
 *
 * rosie_match() and rosie_trace() perform matching against a supplied
 * input string using a compiled pattern.  The compiled pattern is
 * represented by a handle earlier returned by rosie_compile().
 * 
 * rosie_config(), rosie_libpath(), rosie_alloc_limit() allow
 * configuration at the engine level.
 *
 * rosie_finalize() destroys an engine and frees its memory.
 *
 * Most functions have an argument 'str *messages':
 *
 * (1) If messages->ptr is NULL after the call, then there were no
 *     messages.
 *
 * (2) If the return code from the call is non-zero, AND messages->ptr
 *     is non-NULL, then: (i) the return code will indicate the kind
 *     of error, and (ii) messages will contain a human-readable
 *     string explaining the error.
 *
 * (3) If the return code is zero, indicating success, AND
 *     messages->ptr is non-NULL, then messages will contain a
 *     JSON-encoded structure (i.e. a string in JSON format).
 *
 * (4) Independent of whether the librosie call succeeded or not,
 *     messages->ptr may not be NULL on return.  When this is the
 *     case, the caller must free messages after examining or copying
 *     its contents.
 *
*/

/* FUTURE: 
 * 
 * - MAYBE add a function that unloads all the dynamic libs, erases
 *   the global information about the libs, and reinitializes the
 *   ready_to_boot lock.
 * 
 *   - If a client unloads rosie, then the client's engines will
 *     become invalid.  Calling an engine function, even finalize(),
 *     will likely cause a crash.
 *
 *   - The responsibility to finalize all engines before unloading
 *     rosie will be left to the librosie client. 
 *
 *   - If the need to load/unload rosie at the dynamic library level
 *     arises, then someone can wrap librosie in a library that
 *     manages a pool of engines.
 */

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>		/* MAXPATHLEN */
#include <pthread.h>
#include <stdatomic.h>

#include "rpeg.h"
#include "librosie.h"

#include <dlfcn.h>
#include <libgen.h>

#define BOOTSCRIPT "/lib/boot.luac"

static char *rosie_home;
static char *bootscript;

#include "logging.c"
#include "registry.c"
#include "rosiestring.c"

/* Symbol visibility in the final library */
#define EXPORT __attribute__ ((visibility("default")))

/* Suppress warnings */
#define UNUSED(x) (void)(x)

#ifdef DEBUG
#define CHECK_TYPE(label, typ, expected)				\
  do { if (DEBUG)							\
      if ((typ) != (expected)) {					\
	LOGf("type mismatch for %s.  received %d, expected %d.\n",	\
	     (label), (typ), (expected));				\
	assert(0);							\
      }									\
  } while (0)
#else
#define CHECK_TYPE(label, typ, expected_typ)
#endif

/* ----------------------------------------------------------------------------------------
 * Locks
 * ----------------------------------------------------------------------------------------
 */

#define ACQUIRE_LOCK(lock) do {				                \
  int r = pthread_mutex_lock(&(lock));					\
  if (r) {								\
    fprintf(stderr, "%s:%d:%s(): pthread_mutex_lock failed with %d\n",	\
	    __FILE__, __LINE__, __func__, r);				\
    abort();								\
  }									\
} while (0)

#define RELEASE_LOCK(lock) do {				                \
  int r = pthread_mutex_unlock(&(lock));				\
  if (r) {								\
    fprintf(stderr, "%s:%d:%s(): pthread_mutex_unlock failed with %d\n", \
	    __FILE__, __LINE__, __func__, r);				\
    abort();								\
  }									\
} while (0)

/* ----------------------------------------------------------------------------------------
 * Engine locks
 * ----------------------------------------------------------------------------------------
 */

#define ACQUIRE_ENGINE_LOCK(e) ACQUIRE_LOCK((e)->lock)
#define RELEASE_ENGINE_LOCK(e) RELEASE_LOCK((e)->lock)

/* ----------------------------------------------------------------------------------------
 * Start-up / boot functions
 * ----------------------------------------------------------------------------------------
 */

static int set_bootscript(str *rosie_home_arg) {
  int remaining;
  size_t rosie_home_len, bootscript_len;
  static char *last, *install_filename, *real_install_filename, *real_install_dir;
  static Dl_info info;
  static char tmp[MAXPATHLEN];	/* Assume same as PATH_MAX. TODO: Investigate. */

  last = tmp;
  remaining = MAXPATHLEN - 1;

  if (rosie_home_arg) {

    //Copy the path from the argument we were passed to the tmp buffer
    int cpy_len = MAXPATHLEN-1 < rosie_home_arg->len ? MAXPATHLEN-1 : rosie_home_arg->len;
    strncpy(tmp, (char*)rosie_home_arg->ptr, cpy_len);

  } else {

    //Construct the path relative to the librosie binary file, in the tmp buffer
    static char *libpath = "../lib/rosie"; /* relative to librosie location */

    if (dladdr(&set_bootscript, &info) != 0) {
      install_filename = strndup(info.dli_fname, MAXPATHLEN);
      LOGf("install filename is %s\n", install_filename);
      real_install_filename = realpath(install_filename, NULL);
      LOGf("realpath returned %s\n", real_install_filename);
      if (!real_install_filename) {
        LOGf("  and set errno to %d\n", errno);
        return 0; /* ERR */
      }
      real_install_dir = dirname(real_install_filename); /* dirname may modify its arg */
      LOGf("real install_dir is %s\n", real_install_dir);
      last = stpncpy(last, real_install_dir, (size_t) remaining);
      *last = '/'; last++;
      remaining = MAXPATHLEN - strnlen(tmp, MAXPATHLEN) - 1;
      free(install_filename); /* dirname returns ptr to its own internal storage */
      free(real_install_filename); /* realpath malloc'd storage */
    } else {
      LOG("dladdr() failed, not using librosie dir to find bootscript\n");
    }
    if (remaining <= 0) return 0; /* ERR */
    last = stpncpy(last, libpath, (size_t) remaining);
    remaining = MAXPATHLEN - strnlen(tmp, MAXPATHLEN) - 1;
    if (remaining <= 0) return 0; /* ERR */
  }
  LOGf("Path to boot script is %s\n", tmp);

  /* get absolute, resolved path (dynamically allocated) */
  rosie_home = realpath(tmp, NULL);
  rosie_home_len = rosie_home ? strnlen(rosie_home, MAXPATHLEN) : 0;
  if (rosie_home_len >= (MAXPATHLEN - 1)) return 0; /* ERR */

  /* set absolute path to boot script */
  bootscript_len = rosie_home_len + strlen(BOOTSCRIPT);
  if (bootscript_len >= (MAXPATHLEN - 1)) return 0; /* ERR */
  bootscript = (char *) malloc(bootscript_len + 1);
  *bootscript = '\0';		/* in case rosie_home is null */
  if (!bootscript) return 0; 	/* ERR */
  if (rosie_home)
    last = stpncpy(bootscript, rosie_home, rosie_home_len + 1);
  else
    last = bootscript;
  remaining = (bootscript_len + 1) - strnlen(bootscript, MAXPATHLEN) - 1;
  if (remaining <= 0) return 0; /* ERR */
  last = stpncpy(last, BOOTSCRIPT, (size_t) remaining);
  *last = '\0'; 
  LOGf("Bootscript filename set to %s\n", bootscript);
  return 1;			/* OK */
}

/* 
   If 'name' is an encoder implemented in C, return its code number.
   Otherwise, we assume it can be handled by Lua code and we return 0
   to indicate this.
*/
static int encoder_name_to_code (const char *name) {
  const r_encoder_t *entry = r_encoders;
  while (entry->name) {
    if (!strncmp(name, entry->name, MAX_ENCODER_NAME_LENGTH)) return entry->code;
    entry++;
  }
  return 0;
}

static pthread_once_t initialized = PTHREAD_ONCE_INIT;
static int all_is_lost = TRUE;
static _Atomic(str *) temp_rosie_home; //This ptr is only valid during the call to rosie_home_init(), used to pass the arg into initialize()

static void initialize () {
  LOG("INITIALIZE start\n");

  str *local_rosie_home_ptr = atomic_load_explicit(&temp_rosie_home, memory_order_relaxed);

  if (!set_bootscript(local_rosie_home_ptr)) return; /* all is lost */
  all_is_lost = FALSE;
  LOG("INITIALIZE finish\n");
  return;
}

#define NO_INSTALLATION_MSG "unable to find rosie installation files"

static pthread_mutex_t booting = PTHREAD_MUTEX_INITIALIZER;

static int boot (lua_State *L, str *messages) {
  char *msg = NULL;
  if (!*bootscript) {
    *messages = rosie_new_string_from_const(NO_INSTALLATION_MSG);
    return FALSE;
  }
  LOGf("Booting rosie from %s\n", bootscript);
  ACQUIRE_LOCK(booting);

  int status = luaL_loadfile(L, bootscript);
  if (status != LUA_OK) {
    RELEASE_LOCK(booting);
    LOG("Failed to read rosie boot code (using loadfile)\n");
    if (asprintf(&msg, "no rosie installation in directory '%s'", rosie_home)) {
      *messages = rosie_string_from((byte_ptr) msg, strlen(msg));
    } else {
      *messages = rosie_new_string_from_const(NO_INSTALLATION_MSG);
    }
    return FALSE;
  }
  LOG("Reading of boot code succeeded (using loadfile)\n");
  status = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (status != LUA_OK) {
    RELEASE_LOCK(booting);
    LOG("Loading of boot code failed\n");
    if (asprintf(&msg, "failed to load %s -- corrupt installation?", bootscript)) {
      *messages = rosie_string_from((byte_ptr) msg, strlen(msg));
    } else {
      *messages = rosie_new_string_from_const("loading of rosie boot code failed");
    }
    return FALSE;
  }
  LOG("Loading of boot code succeeded\n");
  lua_pushlstring(L, (const char *)rosie_home, strnlen(rosie_home, MAXPATHLEN));
  status = lua_pcall(L, 1, LUA_MULTRET, 0);
  if (status!=LUA_OK) {
    RELEASE_LOCK(booting);
    LOG("Boot function failed.\n");
    size_t len;
    const char *intro = "execution of rosie boot loader failed:\n";
    const char *lua_msg = lua_tolstring(L, -1, &len);
    char *msg = malloc(strlen(intro) + strlen(lua_msg) + 1);
    char *last = stpcpy(msg, intro);
    stpcpy(last, lua_msg);
    *messages = rosie_string_from((unsigned char *)msg, len+strlen(intro));
    return FALSE;
  }
  RELEASE_LOCK(booting);
  LOG("Boot function succeeded\n");
  return TRUE;
}

static int to_json_string (lua_State *L, int pos, str *json_string) {
     size_t len;
     byte_ptr str;
     int t;
     int top = lua_gettop(L);
     get_registry(json_encoder_key);
     lua_pushvalue(L, pos-1);                /* offset because we pushed json_encoder */
     if (!lua_istable(L, -1)) return ERR_SYSCALL_FAILED;
     *json_string = rosie_string_from(NULL, 0);
     /* When the messages table is empty, be sure to return a null rosie_string */
     lua_pushnil(L);
     //if (!lua_next(L, pos-1)) { //TODO: LP: Discuss this with Jamie.  I think this was a bug
     if (!lua_next(L, -2)) {
       /* We have an empty table, so an empty string is a valid way to render an empty table
        in JSON.  So would "{ }", but we'll keep the function's existing behavior.
        We want to leave the stack the way we found it, hence the pop off the table and the encoder function */
       lua_pop(L, 2);
       return LUA_OK;
     } else {
       lua_pop(L, 2);
     }
     t = lua_pcall(L, 1, LUA_MULTRET, 0);
     if (t != LUA_OK) {
       LOG("call to json encoder failed\n"); /* more detail may not be useful to the user */
       return ERR_SYSCALL_FAILED;
     }
     if ((lua_gettop(L) - top) > 1) {
       /* Top of stack is error msg */
       LOG("call to json encoder returned more than one value\n");
       if (lua_isstring(L, -1) && lua_isnil(L, -2)) {
        LOGf("error message from json encoder: %s\n", lua_tolstring(L, -1, NULL));
        return ERR_SYSCALL_FAILED;
       }
       else {
        /* Something really strange happened!  Is there any useful info to return? */
        LOG("call to json encoder returned unexpected values\n");
        return ERR_SYSCALL_FAILED;
       }
     }
     str = (byte_ptr) lua_tolstring(L, -1, &len);
     *json_string = rosie_new_string(str, len);
     lua_pop(L, 1); /* Pop the string, so the stack ends up the way we got it */
     return LUA_OK;
}

static int format_violation_messages (lua_State *L) {
  int t;

  get_registry(violation_format_key);

  /* Now have this stack: format_each(), messages[], ...
     And violation.format_each() mutates its argument.
  */
  lua_pushvalue(L, -2);		/* push copy of messages table */
  t = lua_pcall(L, 1, 1, 0);	/* violation.format_each() */
  if (t != LUA_OK) { 
    LOG("violation.format_each() failed\n"); 
    return ERR_ENGINE_CALL_FAILED; 
  } 
  return LUA_OK;
}

static int violations_to_json_string (lua_State *L, str *json_string) {
  CHECK_TYPE("violation messages", lua_type(L, -1), LUA_TTABLE);
  int t = format_violation_messages(L);
  if (t == LUA_OK) {
    t = to_json_string(L, -1, json_string);
    if (t != LUA_OK) {
      LOG("could not convert violations to json\n");
    }
  }
  return t;
} 

int luaopen_lpeg (lua_State *L);
int luaopen_cjson_safe (lua_State *l);

static lua_State *newstate () {
  lua_State *newL = luaL_newstate();
  luaL_checkversion(newL); /* Ensures several critical things needed to use Lua */
  luaL_openlibs(newL);     /* Open lua's standard libraries */
  luaL_requiref(newL, "lpeg", luaopen_lpeg, 0);
  luaL_requiref(newL, "cjson.safe", luaopen_cjson_safe, 0);
  return newL;
}
  

/* ----------------------------------------------------------------------------------------
 * Exported functions
 * ----------------------------------------------------------------------------------------
 */

/* rosie_home_init: Initializes rosie with the supplied home (shared init & runtime data)
 *
 * WARNING: This function must be called before any other librosie calls, otherwise it will
 * have no effect.
 * 
 * Currently the only supported format for `home` is a path to a rosie home directory.
 * In the future it may make sense to support a self-contained format, for example, a zlib-
 * encoded directory structure within a block of static memory compiled into the library.
 *
 * If `home` is NULL, the code will default to "../lib/rosie", relative to librosie location
 */
EXPORT
void rosie_home_init(str *home, str *messages) {
  
  atomic_store_explicit(&temp_rosie_home, home, memory_order_relaxed); //Make an atomic assignment
  // to temp_rosie_home.  That way, if rosie_home_init is called twice from different threads at the
  // same time.  Rosie will be initialized with the home value from one of the threads, but never
  // some half-corrupt ptr.  This should just be a regular assignment on architectures where a
  // ptr is word-sized, which is most of them.

  pthread_once(&initialized, initialize);
  if (all_is_lost) {
    *messages = rosie_new_string_from_const("initialization failed; enable DEBUG output for details");
  }
}

EXPORT
Engine *rosie_new(str *messages) {

  rosie_home_init(NULL, messages);
  if (all_is_lost) {
    return NULL;
  }

  int t;
  Engine *e = malloc(sizeof(Engine));
  lua_State *L = newstate();
  if (L == NULL) {
    *messages = rosie_new_string_from_const("not enough memory to initialize");
    goto fail_engine;
  }

  if (!boot(L, messages)) {
    goto fail_state;		/* messages already set by boot */
  }
  if (!lua_checkstack(L, 6)) {
    LOG("Cannot initialize: not enough memory for stack\n");
    *messages = rosie_new_string_from_const("not enough memory for stack");
    goto fail_state;
  }
  t = lua_getglobal(L, "rosie");
  CHECK_TYPE("rosie", t, LUA_TTABLE);
  set_registry(rosie_key);
  
  t = lua_getfield(L, -1, "engine");
  CHECK_TYPE("engine", t, LUA_TTABLE);
  t = lua_getfield(L, -1, "new");
  CHECK_TYPE("engine.new", t, LUA_TFUNCTION);
  t = lua_pcall(L, 0, 1, 0);
  if (t != LUA_OK) {
    LOG("rosie.engine.new() failed\n");
    *messages = rosie_new_string_from_const("rosie.engine.new() failed");
    goto fail_state;
  }

  /* Engine instance is at top of stack */
  set_registry(engine_key);
  t = lua_getfield(L, -1, "match");
  CHECK_TYPE("engine.match", t, LUA_TFUNCTION);
  set_registry(engine_match_key);

  lua_createtable(L, INITIAL_RPLX_SLOTS, 0);
  set_registry(rplx_table_key);

  lua_getglobal(L, "rosie");
  t = lua_getfield(L, -1, "env");
  CHECK_TYPE("rosie.env", t, LUA_TTABLE);
  t = lua_getfield(L, -1, "cjson");
  CHECK_TYPE("rosie.env.cjson", t, LUA_TTABLE);
  t = lua_getfield(L, -1, "encode");
  CHECK_TYPE("rosie.env.cjson.encode", t, LUA_TFUNCTION);
  set_registry(json_encoder_key);

  lua_getglobal(L, "rosie");
  t = lua_getfield(L, -1, "env");
  CHECK_TYPE("rosie.env", t, LUA_TTABLE);
  t = lua_getfield(L, -1, "violation");
  CHECK_TYPE("rosie.env.violation", t, LUA_TTABLE);
  t = lua_getfield(L, -1, "format_each");
  CHECK_TYPE("rosie.env.violation.format_each", t, LUA_TFUNCTION);
  set_registry(violation_format_key);

  lua_pushinteger(L, 0);
  set_registry(alloc_set_limit_key);

  pthread_mutex_init(&(e->lock), NULL);
  e->L = L;

  lua_settop(L, 0);
  LOGf("Engine %p created\n", e);
  return e;

fail_state:
  lua_close(L);
fail_engine:
  free(e);
  return NULL;
}
     
/* newlimit of -1 means query for current limit */
EXPORT
int rosie_alloc_limit (Engine *e, int *newlimit, int *usage) {
  int memusg, actual_limit;
  lua_State *L = e->L;
  LOGf ("rosie_alloc_limit() called with int pointers %p, %p\n", newlimit, usage);
  ACQUIRE_ENGINE_LOCK(e);
  lua_gc(L, LUA_GCCOLLECT, 0);
  lua_gc(L, LUA_GCCOLLECT, 0);        /* second time to free resources marked for finalization */
  memusg = lua_gc(L, LUA_GCCOUNT, 0); /* KB */
  if (usage) *usage = memusg;
  if (newlimit) {
    int limit = *newlimit;
    if ((limit != -1) && (limit != 0) && (limit < MIN_ALLOC_LIMIT_MB)) {
      RELEASE_ENGINE_LOCK(e);
      return ERR_ENGINE_CALL_FAILED;
    } 
    if (limit == -1) {
      /* query */
      get_registry(alloc_set_limit_key);
      *newlimit = lua_tointeger(L, -1);
    } else {
      /* set new limit */
      lua_pushinteger(L, limit);
      set_registry(alloc_set_limit_key);
      actual_limit = memusg + limit;
      lua_pushinteger(L, (limit == 0) ? 0 : actual_limit);
      set_registry(alloc_actual_limit_key);
      if (limit == 0) {
	LOGf("set alloc limit to UNLIMITED above current usage level of %0.1f MB\n", memusg/1024.0);
      } else {
	LOGf("set alloc limit to %d MB above current usage level of %0.1f MB\n", *newlimit, memusg/1024.0);
      }
    }
  }
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* N.B. Client must free retval */
EXPORT
int rosie_config (Engine *e, str *retval) {
  int t;
  str r;
  lua_State *L = e->L;
  ACQUIRE_ENGINE_LOCK(e);
  get_registry(rosie_key);
  t = lua_getfield(L, -1, "config");
  CHECK_TYPE("config", t, LUA_TFUNCTION);
  get_registry(engine_key);
  t = lua_pcall(L, 1, 1, 0);
  if (t != LUA_OK) {
    LOG("rosie.config() failed\n");
    *retval = rosie_new_string_from_const("rosie.config() failed");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  t = to_json_string(L, -1, &r);
  if (t != LUA_OK) {
    LOGf("in config(), could not convert config information to json (code=%d)\n", t);
    *retval = rosie_new_string_from_const("in config(), could not convert config information to json");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  retval->len = r.len;
  retval->ptr = r.ptr;
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

EXPORT
int rosie_libpath (Engine *e, str *newpath) {
  int t;
  lua_State *L = e->L;
  ACQUIRE_ENGINE_LOCK(e);
  get_registry(engine_key);
  if (newpath->ptr) {
    t = lua_getfield(L, -1, "set_libpath");
    CHECK_TYPE("engine.set_libpath()", t, LUA_TFUNCTION);
  } else {
    t = lua_getfield(L, -1, "get_libpath");
    CHECK_TYPE("engine.get_libpath()", t, LUA_TFUNCTION);
  }    
  lua_pushvalue(L, -2);
  if (newpath->ptr) {
    lua_pushlstring(L, (const char *)newpath->ptr, newpath->len);
    lua_pushstring(L, "API");
  }
  t = lua_pcall(L, (newpath->ptr) ? 3 : 1, (newpath->ptr) ? 0 : 2, 0);
  if (t != LUA_OK) {
    if (newpath->ptr) {
	LOG("engine.set_libpath() failed\n");
      } else {
	LOG("engine.get_libpath() failed\n");
    }      
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
#if LOGGING
  do {
    get_registry(engine_key);
    t = lua_getfield(L, -1, "libpath");
    t = lua_getfield(L, -1, "value");
    LOGf("libpath obtained directly from engine object is: %s\n", lua_tostring(L, -1));
    lua_pop(L, 3);
  } while (0);
#endif
  if (!newpath->ptr) {
    size_t tmplen;
    const char *tmpptr = lua_tolstring(L, -2, &tmplen);
    str tmpstr = rosie_new_string((byte_ptr)tmpptr, tmplen);
    (*newpath).ptr = tmpstr.ptr;
    (*newpath).len = tmpstr.len;
  }
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* GC in languages like Python 3 may collect the engine before the
   rplx objects, so if we cannot obtain the engine lock due to an
   error (as opposed to the lock being held), then we assume the
   engine has been collected and there is nothing that free_rplx needs
   to do.
 */
EXPORT
int rosie_free_rplx (Engine *e, int pat) {
  lua_State *L = e->L;
  LOGf ("freeing rplx object with index %d\n", pat);
  int r = pthread_mutex_lock(&((e)->lock));
  if (!r) {
    get_registry(rplx_table_key);
    luaL_unref(L, -1, pat);
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
  }
  return SUCCESS;
}

/* N.B. Client must free messages */
EXPORT
int rosie_compile (Engine *e, str *expression, int *pat, str *messages) {
  int t;
  str temp_rs;
  lua_State *L;
  
  if (!expression) {
    LOG("null pointer passed to compile for expression argument\n");
    return ERR_ENGINE_CALL_FAILED;
  }  

  if (!expression) {
    LOG("null pointer passed to compile for expression argument\n");
    return ERR_ENGINE_CALL_FAILED;
  }  

  if (!expression->ptr) {
    LOG("null pointer inside expression passed to compile\n");
    return ERR_ENGINE_CALL_FAILED;
  }  

  L = e->L;
  LOGf("compile(): L = %p, expression = %*s\n", L, expression->len, expression->ptr);
  ACQUIRE_ENGINE_LOCK(e);
  if (!pat) {
    LOG("null pointer passed to compile for pattern argument\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }

  *pat = 0;			/* Indicate compilation error */

#if LOGGING
  if (lua_gettop(L)) LOG("Entering compile(), stack is NOT EMPTY!\n");
#endif  

  get_registry(rplx_table_key);
  get_registry(engine_key);
  t = lua_getfield(L, -1, "compile");
  CHECK_TYPE("compile", t, LUA_TFUNCTION);

  lua_replace(L, -2); /* overwrite engine table with compile function */
  get_registry(engine_key);

  lua_pushlstring(L, (const char *)expression->ptr, expression->len);

  t = lua_pcall(L, 2, 2, 0);

  if (t != LUA_OK) {
    LOG("compile() failed\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }

  if ( !lua_toboolean(L, -2) ) {
    t = violations_to_json_string(L, &temp_rs);
    if (t != LUA_OK) {
      lua_settop(L, 0);
      RELEASE_ENGINE_LOCK(e);
      *messages = rosie_new_string_from_const("could not convert compile messages to json");
      return ERR_ENGINE_CALL_FAILED;
    }
    (*messages).ptr = temp_rs.ptr;
    (*messages).len = temp_rs.len;
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return SUCCESS;
  }
  
  lua_pushvalue(L, -2);
  CHECK_TYPE("new rplx object", lua_type(L, -1), LUA_TTABLE);
  *pat = luaL_ref(L, 1);
  assert( *pat != 0 );		/* Implementation of luaL_ref() ensures this. */
  if (*pat == LUA_REFNIL) {
    LOG("error storing rplx object\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }

  LOGf("storing rplx object at index %d\n", *pat);

  t = violations_to_json_string(L, &temp_rs);
  if (t != LUA_OK) {
    LOG("in compile(), could not convert warning information to json\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  
  (*messages).ptr = temp_rs.ptr;
  (*messages).len = temp_rs.len;

  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

static inline void collect_if_needed (lua_State *L) {
  int limit, memusg;
  get_registry(alloc_actual_limit_key);
  limit = lua_tointeger(L, -1);	/* nil will convert to zero */
  lua_pop(L, 1);
  if (limit) {
    memusg = lua_gc(L, LUA_GCCOUNT, 0);
    if (memusg > limit) {
      LOGf("invoking collection of %0.1f MB heap\n", memusg/1024.0);
      lua_gc(L, LUA_GCCOLLECT, 0);
#if (LOGGING)
      memusg = lua_gc(L, LUA_GCCOUNT, 0);
      LOGf("post-collection heap has %0.1f MB\n", memusg/1024.0);
#endif
    }
  }
}

/*
 * Get the initial position for the match, interpreting negative
 * values from the end of the input string, using Lua convention,
 * including 1-based indexing.
 */
static uint32_t initposition (int pos, size_t len) {
  if (pos == 0) return 1;	/* 0 means "default to first char" */
  if (pos > 0) {		/* positive index? */
    if ((size_t)pos <= len)	/* inside the string? */
      return (size_t)pos;	/* retain 1-based indexing */
    else
      return len+1;		/* crop at the end (1-based index) */
  }
  else {				 /* negative index */
    if ((size_t)(-pos) <= len)	         /* inside the string? */
      return len - ((size_t)(-pos)) + 1; /* return position from the end */
    else
      return 1;			         /* crop at the beginning (1-based) */
  }
}

#define DISPLAY(msg)							\
  do { fprintf(stderr, "%s:%d:%s(): %s", __FILE__,			\
	       __LINE__, __func__, msg);				\
       fflush(stderr);							\
  } while (0)

#define DISPLAYf(fmt, ...)						\
  do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__,			\
	       __LINE__, __func__, __VA_ARGS__);			\
       fflush(stderr);							\
  } while (0)

/* 
   Old interface, to be replaced by rosie_match2().  The meaning of
   the 'start' parameter changes in the new interface, which also
   supports an 'endpos' parameter.
*/
EXPORT
int rosie_match (Engine *e, int pat, int start, char *encoder_name, str *input, match *match) {
  LOG("rosie_match called\n");

  /* Parameter 'start' is allowed to be negative (measures from end) */
  uint32_t startpos = initposition(start, input->len);
  assert( startpos >= 1 );
  assert( startpos <= input->len +1 );
  /* Caller should set match->ttotal to >= 0 if they want timing data */
  uint8_t collect_times = (match->ttotal >= 0);
  return rosie_match2(e, pat, encoder_name,
		      input, startpos, (uint32_t) 0,
		      match, collect_times);
}

#define set_match2_error(match, errno) \
  do { match->data.ptr = NULL;         \
       match->data.len = (errno);      \
  } while (0);

EXPORT
int rosie_match2 (Engine *e, uint32_t pat, char *encoder_name,
		  str *input, uint32_t startpos, uint32_t endpos,
		  struct rosie_matchresult *match,
		  uint8_t collect_times) {
  int err, t, encoder, rmatch_encoder;
  lua_State *L = e->L;
  LOG("rosie_match2 called\n");
  ACQUIRE_ENGINE_LOCK(e);
  collect_if_needed(L);

  if (pat <= 0) {
  no_pattern:
    LOGf("rosie_match2() called with invalid compiled pattern reference: %d\n", pat);
    set_match2_error(match, ERR_NO_PATTERN);
    goto call_succeeded;    /* API completed ok, no internal errors */
  }

  get_registry(rplx_table_key);
  /* Stack from top: rplx table */
  t = lua_rawgeti(L, -1, pat);
  if (t != LUA_TTABLE) goto no_pattern;
  /* Stack from top: rplx object, rplx table */

  encoder = encoder_name_to_code(encoder_name);

  if (encoder == 0) {
    /* This encoder is implemented Lua */
    t = lua_getfield(L, -1, "lookup_encoder");
    CHECK_TYPE("rplx.lookup_encoder()", t, LUA_TFUNCTION);
    /* Stack from top: lookup_encoder function, rplx object, rplx table */
    lua_pushstring(L, encoder_name);
    /* Stack from top: encoder_name, lookup_encoder function, rplx object, rplx table */
    if (lua_pcall(L, 1, 2, 0) != LUA_OK){ /* 1 arg, 2 return values */
      LOG("lookup_encoder() failed\n");
      set_match2_error(match, ERR_INTERNAL);
      goto call_failed;
    }
    /* Stack from top: rmatch encoder number, encoder function, rplx object, rplx table */
    CHECK_TYPE("rmatch encoder number", lua_type(L, -1), LUA_TNUMBER);
    rmatch_encoder = lua_tointeger(L, -1);
    lua_pop(L, 1);
    /* Stack from top: lua encoder function, rplx object, rplx table */
    CHECK_TYPE("lua encoder function", lua_type(L, -1), LUA_TFUNCTION);
  } else {
    /* This encoder is implemented in C, not Lua  */
    rmatch_encoder = encoder;
    lua_pushnil(L); /* dummy, to keep stack index numbers consistent below */
    /* Stack from top: nil, rplx object, rplx table */
  }

  t = lua_getfield(L, 2, "pattern");
  CHECK_TYPE("rplx pattern slot", t, LUA_TTABLE);
  /* Stack from top: pattern object, MAYBE lua encoder, rplx object, rplx table */
  t = lua_getfield(L, -1, "peg");
  CHECK_TYPE("rplx pattern peg slot", t, LUA_TUSERDATA);
  /* Stack from top: peg userdata, pattern object, MAYBE lua encoder, rplx object, rplx table */
  void *pattern = extract_pattern(L, -1);
  t = lua_getfield(L, 2, "buf");
  CHECK_TYPE("rplx.buf", t, LUA_TUSERDATA);
  /* Stack from top: output, peg, pattern object, MAYBE lua encoder, rplx object, rplx table */
  RBuffer *output = luaL_checkudata(L, -1, ROSIE_BUFFER);

  err = r_match_C2(pattern, input, startpos, endpos,
		   rmatch_encoder, collect_times,
		   *output, match);

  if (err != 0) {  
    LOG("rosie_match2() failed\n");  
    set_match2_error(match, err);
    goto call_failed;
  }

  if ((encoder == 0) && (match->data.ptr != NULL)) {
    /* Found a match, and the output encoder is written in Lua */
    /* Stack from top: output, peg, pattern object, lua encoder, rplx object, rplx table */
    lua_remove(L, -2);		/* remove peg */
    /* Stack from top: output, pattern object, lua encoder, rplx object, rplx table */
    lua_remove(L, -2);		/* remove pattern object */
    /* Stack from top: output, lua encoder, rplx object, rplx table */
    CHECK_TYPE("lua encoder function", lua_type(L, -1), LUA_TUSERDATA);
    CHECK_TYPE("lua encoder function", lua_type(L, -2), LUA_TFUNCTION);
    /* Don't make a copy of the input.  Wrap it in a Buffer, which will
       be gc'd later (but will not free the original source data). */
    r_newbuffer_wrap(L, (char *)input->ptr, input->len); 
    /* Stack from top: input (as buffer), output, lua encoder, rplx object, rplx table */
    lua_pushinteger(L, startpos);
    /* Stack from top: startpos, input, output, lua encoder, rplx object, rplx table */
    t = lua_getfield(L, 2, "engine");
    CHECK_TYPE("engine", t, LUA_TTABLE);
    /* Stack from top: engine, startpos, input, output, lua encoder, rplx object, rplx table */
    t = lua_getfield(L, -1, "encoder_parms");
    CHECK_TYPE("engine", t, LUA_TTABLE);
    /* Stack from top: encoder parms, engine, startpos, input, output, lua encoder, rplx object, rplx table */
    lua_remove(L, -2);		/* remove engine object */
    /* Stack from top: encoder parms, startpos, input, output, lua encoder, rplx object, rplx table */
    if (lua_pcall(L, 4, 1, 0) != LUA_OK) goto call_failed;
    /* Stack from top: encoder result, rplx object, rplx table */
    t = lua_type(L, -1);
    if (t != LUA_TSTRING) {
      if (t != LUA_TNUMBER) {
	LOGf("unexpected return type from output encoder: %d\n", t);
	goto call_failed;
      }
      assert(lua_tointeger(L, -1) == ERR_NO_ENCODER);
      set_match2_error(match, ERR_NO_ENCODER);
      goto call_succeeded;
    }
    /* Copy lua string to output buffer, and return it */
    size_t temp_len;
    const char *temp_str = lua_tolstring(L, -1, &temp_len);
    assert(output);
    buf_reset(*output);		/* Reset the buffer for reuse */
    buf_addlstring(*output, temp_str, temp_len);
    match->data.len = temp_len;
  } /* end of lua-implemented encoder invocation */

 call_succeeded:
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;

 call_failed:
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return ERR_ENGINE_CALL_FAILED;  

}   /* end rosie_match2() */

/* N.B. Client must free trace */
EXPORT
int rosie_trace (Engine *e, int pat, int start, char *trace_style, str *input, int *matched, str *trace) {
  int t;
  str rs;
  lua_State *L = e->L;
  ACQUIRE_ENGINE_LOCK(e);
  collect_if_needed(L);
  get_registry(engine_key);
  t = lua_getfield(L, -1, "trace");
  CHECK_TYPE("engine.trace()", t, LUA_TFUNCTION);
  get_registry(engine_key);	/* first arg to trace */
  if (!pat)
    LOGf("rosie_trace() called with invalid compiled pattern reference: %d\n", pat);
  else {
    get_registry(rplx_table_key);
    t = lua_rawgeti(L, -1, pat); /* arg 2 to trace*/
    if (t == LUA_TTABLE) goto have_pattern;
  }
  (*trace).ptr = NULL;
  (*trace).len = ERR_NO_PATTERN;
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;

have_pattern:

  lua_replace(L, -2); 		/* overwrite rplx table with rplx object */
  if (!trace_style) {
    LOG("rosie_trace() called with null trace_style arg\n");
    (*trace).ptr = NULL;
    (*trace).len = ERR_NO_ENCODER;
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return SUCCESS;
  }

  lua_pushlstring(L, (const char *)input->ptr, input->len); /* arg 3 */
  lua_pushinteger(L, start);	                            /* arg 4 */
  lua_pushstring(L, trace_style);                           /* arg 5 */

  t = lua_pcall(L, 5, 3, 0); 
  if (t != LUA_OK) {  
    LOG("trace() failed\n");  
    lua_settop(L, 0); 
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;  
  }  

  /* The first return value from trace indicates whether the pattern
     compiled, and we are always sending in a compiled pattern, so the
     first return value is always true. 
  */
  assert( lua_isboolean(L, -3) );

  /* Second return value is the match indicator (boolean).  A number
     indicates an error. 
  */
  int result_type = lua_type(L, -2);
  switch (result_type) {
  case LUA_TBOOLEAN: {
    LOGf("in rosie_trace, trace returned the boolean %s\n", (int) lua_toboolean(L, -2) ? "true" : "false");
    (*matched) = lua_toboolean(L, -2);
    break;
  }
  case LUA_TNUMBER: {
    int trace_code = (int) lua_tointeger(L, -2);
    LOGf("in rosie_trace, trace returned the integer code %d\n", trace_code);
    trace->ptr = NULL;
    trace->len = trace_code;
    goto succeed; /* tracing failed, but rosie_trace completed without errors */
  } }

  if (lua_isstring(L, -1)) {
    byte_ptr temp_str;
    size_t temp_len;
    temp_str = (byte_ptr) lua_tolstring(L, -1, &(temp_len));
    rs = rosie_new_string(temp_str, temp_len);
  }
  else {
    LOG("trace() failed with unexpected return value from engine.trace()\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  (*trace).ptr = rs.ptr;
  (*trace).len = rs.len;
 succeed:
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* N.B. Client must free 'messages' and 'pkgname' */
EXPORT
int rosie_load (Engine *e, int *ok, str *src, str *pkgname, str *messages) {
  int t;
  size_t temp_len;
  unsigned char *temp_str;
  str temp_rs;
  lua_State *L = e->L;
  ACQUIRE_ENGINE_LOCK(e);
  get_registry(engine_key);
  t = lua_getfield(L, -1, "load");
  CHECK_TYPE("engine.load()", t, LUA_TFUNCTION);
  lua_pushvalue(L, -2);		/* push engine object again */
  lua_pushlstring(L, (const char *)src->ptr, src->len);

  t = lua_pcall(L, 2, 3, 0); 
  if (t != LUA_OK) { 
    /* Details will likely not be helpful to the user */
    LOG("engine.load() failed\n"); 
    *messages = rosie_new_string_from_const("engine.load() failed"); 
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED; 
  } 

  *ok = lua_toboolean(L, -3);
  LOGf("engine.load() %s\n", *ok ? "succeeded\n" : "failed\n");
  
  if (lua_isstring(L, -2)) {
    temp_str = (unsigned char *)lua_tolstring(L, -2, &temp_len);
    *pkgname = rosie_new_string(temp_str, temp_len);
  }
  else {
    pkgname->ptr = NULL;
    pkgname->len = 0;
  }
  
  t = violations_to_json_string(L, &temp_rs);
  if (t != LUA_OK) {
    LOG("in load(), could not convert error information to json\n");
    temp_rs = rosie_new_string_from_const("in load(), could not convert error information to json");
    goto fail_load_with_messages;
    }

 fail_load_with_messages:
  (*messages).ptr = temp_rs.ptr;
  (*messages).len = temp_rs.len;

  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* N.B. Client must free 'messages' and 'pkgname' */
EXPORT
int rosie_loadfile (Engine *e, int *ok, str *fn, str *pkgname, str *messages) {
  int t;
  size_t temp_len;
  unsigned char *temp_str;
  str temp_rs;
  lua_State *L = e->L;
  ACQUIRE_ENGINE_LOCK(e);
  get_registry(engine_key);
  t = lua_getfield(L, -1, "loadfile");
  CHECK_TYPE("engine.loadfile()", t, LUA_TFUNCTION);
  lua_pushvalue(L, -2);		/* push engine object again */
  const char *fname = lua_pushlstring(L, (const char *)fn->ptr, fn->len);
  UNUSED(fname);
  LOGf("engine.loadfile(): about to load %s\n", fname);
  t = lua_pcall(L, 2, 3, 0); 
  if (t != LUA_OK) { 
    display("Internal error: call to engine.loadfile() failed"); 
    /* Details will likely not be helpful to the user */
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED; 
  } 

  *ok = lua_toboolean(L, -3);
  LOGf("engine.loadfile() %s\n", *ok ? "succeeded" : "failed");
  
  if (lua_isstring(L, -2)) {
    temp_str = (unsigned char *)lua_tolstring(L, -2, &temp_len);
    str loaded_pkgname = rosie_new_string(temp_str, temp_len);
    (*pkgname).ptr = loaded_pkgname.ptr;
    (*pkgname).len = loaded_pkgname.len;
  }
  else {
    (*pkgname).ptr = NULL;
    (*pkgname).len = 0;
  }
  
  t = violations_to_json_string(L, &temp_rs);
  if (t != LUA_OK) {
    LOG("in load(), could not convert error information to json\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  (*messages).ptr = temp_rs.ptr;
  (*messages).len = temp_rs.len;

  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* N.B. Client must free 'messages' */
EXPORT
int rosie_import (Engine *e, int *ok, str *pkgname, str *as, str *actual_pkgname, str *messages) {
  int t;
  size_t temp_len;
  unsigned char *temp_str;
  str temp_rs;
  lua_State *L = e->L;
  
  ACQUIRE_ENGINE_LOCK(e);
  get_registry(engine_key);
  t = lua_getfield(L, -1, "import");
  CHECK_TYPE("engine.import()", t, LUA_TFUNCTION);
  lua_pushvalue(L, -2);		/* push engine object again */
  lua_pushlstring(L, (const char *)pkgname->ptr, pkgname->len);
  if (as) {
    lua_pushlstring(L, (const char *)as->ptr, as->len);
  }
  else {
    lua_pushnil(L);
  }

  t = lua_pcall(L, 3, 3, 0); 
  if (t != LUA_OK) { 
    LOG("engine.import() failed\n"); 
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED; 
  } 

  *ok = lua_toboolean(L, -3);
  LOGf("import %*s %s\n", pkgname->len, pkgname->ptr, *ok ? "succeeded" : "failed");
  
  if (lua_isstring(L, -2)) {
    temp_str = (unsigned char *)lua_tolstring(L, -2, &temp_len);
    *actual_pkgname = rosie_new_string(temp_str, temp_len);
    LOGf("engine.import reports that package %s was loaded\n", temp_str);
  }
  else {
    (*actual_pkgname).ptr = NULL;
    (*actual_pkgname).len = 0;
  }
  
  t = violations_to_json_string(L, &temp_rs);
  if (t != LUA_OK) {
    LOG("could not convert error information to json\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  (*messages).ptr = temp_rs.ptr;
  (*messages).len = temp_rs.len;

  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* FUTURE: Expose engine_process_file() ? */


/* rosie_matchfile() is implemented here for performance reasons.  The
   CLI is implemented in Lua, and we do not want Lua to be reading an
   input file line by line, and calling the vm to match a pattern
   against each line.  The Lua overhead, if we had left this in Lua,
   would include interning each line read from the input file, eating
   up a lot of memory as well as time. */

/* N.B. Client must free err */
EXPORT
int rosie_matchfile (Engine *e, int pat, char *encoder, int wholefileflag,
		     char *infilename, char *outfilename, char *errfilename,
		     int *cin, int *cout, int *cerr,
		     str *err) {
  int t;
  unsigned char *temp_str;
  size_t temp_len;
  lua_State *L = e->L;
  (*err).ptr = NULL;
  (*err).len = 0;

  ACQUIRE_ENGINE_LOCK(e);
  collect_if_needed(L);
  get_registry(engine_key);
  t = lua_getfield(L, -1, "matchfile");
  CHECK_TYPE("engine.matchfile()", t, LUA_TFUNCTION);
  get_registry(engine_key);	/* first arg */

  get_registry(rplx_table_key);
  t = lua_rawgeti(L, -1, pat); /* arg 2 */
  if (t != LUA_TTABLE) {
    LOGf("rosie_matchfile() called with invalid compiled pattern reference: %d\n", pat);
    (*cin) = -1;
    (*cout) = ERR_NO_PATTERN;
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return SUCCESS;
  }

  lua_replace(L, -2); 		/* overwrite rplx table with rplx object */
  if (!encoder) {
    LOG("rosie_matchfile() called with null encoder name\n");
    (*cin) = -1;
    (*cout) = ERR_NO_ENCODER;
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return SUCCESS;
  }

  lua_pushstring(L, infilename);  /* arg 3 */
  lua_pushstring(L, outfilename); /* arg 4 */
  lua_pushstring(L, errfilename); /* arg 5 */
  lua_pushstring(L, encoder);	  /* arg 6 */
  lua_pushboolean(L, wholefileflag); /* arg 7 */

  t = lua_pcall(L, 7, 3, 0); 
  if (t != LUA_OK) {  
    LOG("matchfile() failed\n");  
    /* FUTURE: return the error, if there's a situation where it may help */
    lua_settop(L, 0); 
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;  
  }  

  if (lua_isnil(L, -1)) {

    /* i/o issue with one of the files */
    (*cin) = -1;
    (*cout) = 3;
    temp_str =  (unsigned char *)lua_tolstring(L, -2, &temp_len);
    str msg = rosie_new_string(temp_str, temp_len);
    (*err).ptr = msg.ptr;
    (*err).len = msg.len;
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return SUCCESS;
  }

  (*cin) = lua_tointeger(L, -3);  /* cerr */
  (*cout) = lua_tointeger(L, -2); /* cout, or error code if error */
  (*cerr) = lua_tointeger(L, -1); /* cin, or -1 if error */
  
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

/* ----------------------------------------------------------------------------- */
/* The librosie API provides access to a number of "internal"
   functions that operate on RPL expressions.  The CLI uses these, for
   example to implement the 'expand' command which shows the user how
   an RPL expression is parsed and macro-expanded.  Also, the CLI
   automatically imports RPL libraries as a convenience.  I.e. if the
   user enters `net.ip` within an RPL expression on the command line,
   the Rosie CLI will attempt to import the `net` library.
 */

static int rosie_syntax_op (const char *fname, Engine *e, str *input, str *f_str_result, int *f_int_result, str *messages) {
  int t;
  str r;
  lua_State *L = e->L;
  LOGf("%s called\n", fname);
  ACQUIRE_ENGINE_LOCK(e);
  lua_pushnil(L);			      /* stack: nil */
  get_registry(engine_key);		      /* stack: engine, nil */
  t = lua_getfield(L, -1, fname); /* stack: function, engine, nil */
  CHECK_TYPE(fname, t, LUA_TFUNCTION);
  lua_replace(L, 1);		/* stack: engine, function */
  lua_pushlstring(L, (const char *)input->ptr, input->len);
  /* stack: input, engine, function */
  assert(lua_gettop(L) == 3);
  t = lua_pcall(L, 2, 2, 0); 
  if (t != LUA_OK) {  
  rosie_syntax_op_failed:
    LOGf("%s failed\n", fname);
    lua_settop(L, 0); 
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;  
  }  
  if (lua_istable(L, -1)) {
    LOG("there are messages\n");
    t = to_json_string(L, -1, &r);
    if (t == LUA_OK) {
      messages->len = r.len;
      messages->ptr = r.ptr;
    } else {
      LOG("could not convert messages to json\n");
      *messages = rosie_new_string_from_const("error: could not convert messages to json");      
      goto rosie_syntax_op_failed;
    }
  }
  if (lua_istable(L, -2)) { // If the Lua function returned a table, convert it to JSON and return it in f_str_result
    if (f_str_result) {
      LOG("there are refs\n"); //LP: Discuss with Jamie: This log might be misleading, but I didn't want to mess with anyone who might be parsing the logs
      t = to_json_string(L, -2, &r);
      if (t == LUA_OK) {
        f_str_result->len = r.len;
        f_str_result->ptr = r.ptr;
      } else {
        LOG("could not convert result table to json\n");
        *messages = rosie_new_string_from_const("error: could not convert result table to json");
        goto rosie_syntax_op_failed;
      }
    }
  }
  if (lua_isinteger(L, -2)) { // If the Lua function returned an integer, return it directly in f_num_result
    if (f_int_result) {
      *f_int_result = lua_tointeger(L, -2);
    }
  }
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

EXPORT
int rosie_expression_refs (Engine *e, str *input, str *refs, str *messages) {
  return rosie_syntax_op("expression_refs", e, input, refs, NULL, messages);
}

EXPORT
int rosie_block_refs (Engine *e, str *input, str *refs, str *messages) {
  return rosie_syntax_op("block_refs", e, input, refs, NULL, messages);
}

EXPORT
int rosie_expression_deps (Engine *e, str *input, str *deps, str *messages) {
  return rosie_syntax_op("expression_dependencies", e, input, deps, NULL, messages);
}

EXPORT
int rosie_block_deps (Engine *e, str *input, str *deps, str *messages) {
  return rosie_syntax_op("block_dependencies", e, input, deps, NULL, messages);
}

EXPORT
int rosie_parse_expression (Engine *e, str *input, str *parsetree, str *messages) {
  return rosie_syntax_op("parse_expression", e, input, parsetree, NULL, messages);
}

EXPORT
int rosie_parse_block (Engine *e, str *input, str *parsetree, str *messages) {
  return rosie_syntax_op("parse_block", e, input, parsetree, NULL, messages);
}

EXPORT
int rosie_import_expression_deps (Engine *e, str *expression, str *pkgs, int *err, str *messages) {
  return rosie_syntax_op("import_expression_deps", e, expression, pkgs, err, messages);
}

static int push_rcfile_args (Engine *e, str *filename) {
  int t __attribute__((unused)); /* unused when DEBUG not set */
  lua_State *L = e->L;		/* for the CHECK_TYPE macro */
  int is_default_rcfile = (filename->ptr == NULL);
  /* Push engine */
  get_registry(engine_key);
  /* Push filename */
  if (!filename->ptr) {
    /* Use default rc filename */
    LOG("using default rc filename\n");
    get_registry(rosie_key);	/* stack: rosie, engine, read_rcfile, rosie */
    t = lua_getfield(L, -1, "default");	/* stack: default, rosie, engine, read_rcfile, rosie */
    CHECK_TYPE("default", t, LUA_TTABLE);
    lua_remove(L, -2); /* stack: default, engine, read_rcfile, rosie */
    t = lua_getfield(L, -1, "rcfile"); /* stack: rcfile, default, engine, read_rcfile, rosie */
    CHECK_TYPE("rcfile", t, LUA_TSTRING);
    lua_remove(L, -2); /* stack: rcfile, engine, read_rcfile, rosie */
    /* stack: rcfile, engine, read_rcfile, rosie */
  } else {
    LOGf("using supplied rc filename: %*s\n",
	 filename->len, filename->ptr);
    is_default_rcfile = FALSE;
    lua_pushlstring(L, (const char *)filename->ptr, filename->len);
    /* stack: filename, rcfile, engine, read_rcfile, rosie */
  }
  /* Push engine maker */
  get_registry(rosie_key);
  t = lua_getfield(L, -1, "engine");
  CHECK_TYPE("engine", t, LUA_TTABLE);
  /* stack: engine, rosie, rcfile, engine, read_rcfile, rosie */
  lua_remove(L, -2); /* stack: engine, rcfile, engine, read_rcfile, rosie */
  t = lua_getfield(L, -1, "new");
  CHECK_TYPE("engine.new", t, LUA_TFUNCTION);
  /* stack: engine_maker, engine, rcfile, engine, read_rcfile, rosie */
  lua_remove(L, -2); /* stack: engine_maker, rcfile, engine, read_rcfile, rosie */
  /* Push is_default_rcfile */
  lua_pushboolean(L, is_default_rcfile);
  return LUA_OK;
}

/* N.B. Client must free options */
EXPORT
int rosie_read_rcfile (Engine *e, str *filename, int *file_exists, str *options, str *messages) {
  str r;
  int t;
  ACQUIRE_ENGINE_LOCK(e);
  lua_State *L = e->L;
  get_registry(engine_key);
  t = lua_getfield(L, -1, "read_rcfile");
  CHECK_TYPE("read_rcfile", t, LUA_TFUNCTION);
  /* Push all the args */
  t = push_rcfile_args(e, filename);
  if (t != LUA_OK) goto read_rcfile_failed;
  t = lua_pcall(L, 4, 3, 0);
  if (t != LUA_OK) {
    LOG("read_rcfile() failed\n");
    *options = rosie_new_string_from_const("read_rcfile() failed");
    goto read_rcfile_failed;
  }
  /* Return values are file_existed (bool), options_table (or false), messages (or nil) */
  *file_exists = lua_toboolean(L, -3);
  if (*file_exists) {
    LOG("rc file exists\n");
  } else {
    LOG("rc file does not exist\n");
  }
  if (lua_istable(L, -2)) {
    LOG("file processed successfully\n");
    t = to_json_string(L, -2, &r);
    if (t == LUA_OK) {
      options->len = r.len;
      options->ptr = r.ptr;
    } else {
      LOGf("could not convert options to json (code=%d)\n", t);
      *options = rosie_new_string_from_const("in read_rcfile(), could not convert options to json");
      goto read_rcfile_failed;
    }
  } else {
    LOG("file FAILED to process without errors\n");
  }
  if (lua_istable(L, -1)) {
    LOG("there are messages\n");
    t = to_json_string(L, -1, &r);
    if (t == LUA_OK) {
      messages->len = r.len;
      messages->ptr = r.ptr;
    } else {
      LOG("could not convert messages to json\n");
      *messages = rosie_new_string_from_const("error: could not convert messages to json");      
    }
  } else {
    LOG("there were no messages\n");
  }
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;

 read_rcfile_failed:
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return ERR_ENGINE_CALL_FAILED;
}

/* N.B. Client must free options */
EXPORT
int rosie_execute_rcfile (Engine *e, str *filename, int *file_exists, int *no_errors, str *messages) {
  int t;
  str r;
  ACQUIRE_ENGINE_LOCK(e);
  lua_State *L = e->L;
  get_registry(engine_key);
  t = lua_getfield(L, -1, "execute_rcfile");
  CHECK_TYPE("execute_rcfile", t, LUA_TFUNCTION);
  /* Push all but the last arg */
  t = push_rcfile_args(e, filename);
  if (t != LUA_OK) goto execute_rcfile_failed;
  /* Push the set_by arg */
  lua_pushstring(L, "API");
  t = lua_pcall(L, 5, 3, 0);
  if (t != LUA_OK) {
  execute_rcfile_failed:
    LOG("execute_rcfile() failed\n");
    lua_settop(L, 0);
    RELEASE_ENGINE_LOCK(e);
    return ERR_ENGINE_CALL_FAILED;
  }
  /* Return values are file_existed, processed_without_error, messages */
  *file_exists = lua_toboolean(L, -3);
  *no_errors = FALSE;
  if (*file_exists) {
    LOG("rc file exists\n");
  } else {
    LOG("rc file does not exist\n");
  }
  if (lua_toboolean(L, -2)) {
    LOG("rc file processed successfully\n");
    *no_errors = TRUE;
  }
  else {
    LOG("file FAILED to process without errors\n");
  }
  if (lua_istable(L, -1)) {
    LOG("there are messages\n");
    t = to_json_string(L, -1, &r);
    if (t == LUA_OK) {
      messages->len = r.len;
      messages->ptr = r.ptr;
    } else {
      LOG("could not convert messages to json\n");
      *messages = rosie_new_string_from_const("error: could not convert messages to json");      
      goto execute_rcfile_failed;
    }
  } else {
    LOG("there were no messages\n");
  }
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

EXPORT
void rosie_finalize (Engine *e) {
  lua_State *L = e->L;
  ACQUIRE_ENGINE_LOCK(e);
  get_registry(prev_string_result_key); 
  if (lua_isuserdata(L, -1)) { 
    str *rs = lua_touserdata(L, -1); 
    if (rs->ptr) rosie_free_string_ptr(rs); 
    lua_pop(L, 1); 
  } 
  LOGf("Finalizing engine %p\n", L);
  lua_close(L);
  /*
   * We do not RELEASE_ENGINE_LOCK(e) here because a waiting thread
   * would then have access to an engine which we have closed, and
   * whose memory we are about to free.
   *
   * The caller should take care to have each engine be created, used,
   * destroyed, and then never used again.  
   *
   * One way to achieve this is to have each thread responsible for
   * creating and destroying its own engines.  In that scenario, a
   * thread's engine should be private to that thread.
   * 
   * Alternatively, an engine pool could be created (in the client
   * code).  The pool manager would be responsible for calling
   * rosie_finalize() when there is no danger of any thread attempting
   * to use the engine being destroyed.
   *
   */
  free(e);
}


/* ----------------------------------------------------------------------------------------
 * Functions to support the Lua implementation of the CLI
 * ----------------------------------------------------------------------------------------
 */

#ifdef ROSIE_CLI

#define CLI_LUAC "/lib/cli.luac"
#include "lua_repl.h"

static void pushargs (lua_State *L, int argc, char **argv) {
  lua_createtable(L, argc+1, 0);
  for (int i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i);
  }
  lua_setglobal(L, "arg");
}

int luaopen_readline (lua_State *L); /* will dynamically load the system libreadline/libedit */

EXPORT
int rosie_exec_cli (Engine *e, int argc, char **argv, char **err) {
  char fname[MAXPATHLEN];
  size_t len = strnlen(rosie_home, MAXPATHLEN);
  char *last = stpncpy(fname, rosie_home, (MAXPATHLEN - len - 1));
  last = stpncpy(last, CLI_LUAC, MAXPATHLEN - len - 1 - strlen(CLI_LUAC));
  *last = '\0';

  LOGf("Entering rosie_exec_cli, computed cli filename is %s\n", fname);

  ACQUIRE_ENGINE_LOCK(e);
  lua_State *L = e->L;
  luaL_requiref(L, "readline", luaopen_readline, 0);

  get_registry(engine_key);
  lua_setglobal(L, "cli_engine");
  
  pushargs(L, argc, argv);

  int status = luaL_loadfile(L, fname);
  if (status != LUA_OK) {
      status = ERR_LUA_CLI_LOAD_FAILED;
      goto cli_fail;
  }
  status = docall(L, 0, 1);
  if (status != LUA_OK) {
    status = ERR_LUA_CLI_EXEC_FAILED;
    goto cli_fail;
  }
  /* The 'status' output encoder returns true or false. */
  status = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : lua_tonumber(L, -1);
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return status;

 cli_fail:
  if (*err) *err = (char *) lua_tostring(L, -1);
  LOGf("CLI failed: loaded from %s, error code #%d, extra info: %s)\n",
       fname, status *err);
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return status;
}

#else /* ROSIE_CLI */

EXPORT
int rosie_exec_cli(Engine *e, int argc, char **argv, char **err) {
  (void)(e); //squash unused parameter warnings.
  (void)(argc);
  (void)(argv);
  (void)(err);

  fprintf(stderr, "fatal error: librosie built without CLI support.\n");
  abort();
}

#endif	/* ROSIE_CLI */

/* ----------------------------------------------------------------------------------------
 * Functions to support the Lua repl for debugging
 * ----------------------------------------------------------------------------------------
 */

#ifdef LUADEBUG

EXPORT
int rosie_exec_lua_repl (Engine *e, int argc, char **argv) {
  LOG("Entering rosie_exec_lua_repl\n");

  ACQUIRE_ENGINE_LOCK(e);
  lua_State *L = e->L;
  luaL_requiref(L, "readline", luaopen_readline, 0);

  get_registry(engine_key);
  lua_setglobal(L, "cli_engine");
  
  pushargs(L, argc, argv);
  lua_repl(L, argv[0]);
  lua_settop(L, 0);
  RELEASE_ENGINE_LOCK(e);
  return SUCCESS;
}

#endif	/* LUADEBUG */


