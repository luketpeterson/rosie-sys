/*  -*- Mode: C; -*-                                                         */
/*                                                                           */
/*  json.h                                                                   */
/*                                                                           */
/*  © Copyright Jamie A. Jennings 2018.                                      */
/*  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)  */
/*  AUTHOR: Jamie A. Jennings                                                */


#if !defined(json_h)
#define json_h

//int json_Fullcapture(CapState *cs, Buffer *buf, int count);
int json_Close(CapState *cs, Buffer *buf, int count, const char *start);
int json_Open(CapState *cs, Buffer *buf, int count);

/* Some JSON literals */
#define TYPE_LABEL ("{\"type\":\"")
#define START_LABEL (",\"s\":")
#define END_LABEL (",\"e\":")
#define DATA_LABEL (",\"data\":")
#define COMPONENT_LABEL (",\"subs\":[")

#endif
