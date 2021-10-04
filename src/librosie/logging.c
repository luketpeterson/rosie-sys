/*  -*- Mode: C/l; -*-                                                       */
/*                                                                           */
/*  logging.c  Part of librosie.c                                            */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2017.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


/* display() is used in only the most awkward situations, when there
   is no easy way to return a specific error to the caller, AND when
   we do not want to ask the user to recompile with LOGGING in order
   to understand that something very strange and unrecoverable occurred. 
*/
static void display (const char *msg) {
  fprintf(stderr, "librosie: %s\n", msg);
  fflush(stderr);
}

/* ----------------------------------------------------------------------------------------
 * Logging and debugging: Compile with LOGGING=1 to enable logging
 * ----------------------------------------------------------------------------------------
 */

#ifdef LOGGING

#define LOG(msg) \
     do { if (LOGGING) fprintf(stderr, "%s:%d:%s(): %s", __FILE__, \
			       __LINE__, __func__, msg);	   \
	  fflush(stderr);						   \
     } while (0)

#define LOGf(fmt, ...) \
     do { if (LOGGING) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
			       __LINE__, __func__, __VA_ARGS__);     \
	  fflush(stderr);						     \
     } while (0)

#else

#define LOG(msg)

#define LOGf(fmt, ...)

#endif


