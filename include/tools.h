/*
 * Copyright (C) 2003-2004 E. Will et al.
 * Copyright (C) 2005-2006 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * Misc tools
 */

#ifndef ATHEME_TOOLS_H
#define ATHEME_TOOLS_H

/* email stuff */
extern int sendemail(struct user *u, struct myuser *mu, const char *type, const char *email, const char *param);

/* email types (meaning of param argument) */
#define EMAIL_REGISTER	"register"	/* register an account/nick (verification code) */
#define EMAIL_SENDPASS	"sendpass"	/* send a password to a user (password) */
#define EMAIL_SETEMAIL	"setemail"	/* change email address (verification code) */
#define EMAIL_MEMO	"memo"		/* emailed memos (memo text) */
#define EMAIL_SETPASS	"setpass"	/* send a password change key (verification code) */

#if defined(HAVE_ARC4RANDOM) && defined(HAVE_ARC4RANDOM_BUF) && defined(HAVE_ARC4RANDOM_UNIFORM)
#  define arc4random            arc4random
#  define arc4random_buf        arc4random_buf
#  define arc4random_uniform    arc4random_uniform
#else
/* arc4random.c */
extern uint32_t                 atheme_arc4random(void);
extern void                     atheme_arc4random_buf(void *buf, size_t n);
extern uint32_t                 atheme_arc4random_uniform(uint32_t upper_bound);
#  define arc4random            atheme_arc4random
#  define arc4random_buf        atheme_arc4random_buf
#  define arc4random_uniform    atheme_arc4random_uniform
#endif /* !HAVE_ARC4RANDOM || !HAVE_ARC4RANDOM_BUF || !HAVE_ARC4RANDOM_UNIFORM */

#ifndef HAVE_EXPLICIT_BZERO
#  ifdef HAVE_MEMSET_S
#    define explicit_bzero(p, n) memset_s((p), (n), 0x00, (n))
#  else /* HAVE_MEMSET_S */
/* explicit_bzero.c */
extern void *(* volatile volatile_memset)(void *, int, size_t);
extern void explicit_bzero(void *p, size_t n);
#  endif /* !HAVE_MEMSET_S */
#endif /* !HAVE_EXPLICIT_BZERO */

/* cidr.c */
extern int valid_ip_or_mask(const char *src);

enum log_type
{
	LOG_ANY = 0,
	LOG_INTERACTIVE = 1, /* IRC channels */
	LOG_NONINTERACTIVE = 2 /* files */
};

/* logstreams API --nenolod */
typedef void (*log_write_func_fn)(struct logfile *lf, const char *buf);

/* logger.c */
struct logfile
{
	struct atheme_object parent;
	mowgli_node_t node;

	void *log_file;		/* opaque: can either be struct mychan or FILE --nenolod */
	char *log_path;
	unsigned int log_mask;

	log_write_func_fn write_func;
	enum log_type log_type;
};

extern char *log_path; /* contains path to default log. */
extern int log_force;

extern struct logfile *logfile_new(const char *log_path_, unsigned int log_mask);
extern void logfile_register(struct logfile *lf);
extern void logfile_unregister(struct logfile *lf);

/* general */
#define LG_NONE         0x00000001      /* don't log                */
#define LG_INFO         0x00000002      /* log general info         */
#define LG_ERROR        0x00000004      /* log real important stuff */
#define LG_IOERROR      0x00000008      /* log I/O errors. */
#define LG_DEBUG        0x00000010      /* log debugging stuff      */
#define LG_VERBOSE	0x00000020	/* log a bit more verbosely than INFO or REGISTER, but not as much as DEBUG */
/* commands */
#define LG_CMD_ADMIN    0x00000100 /* oper-only commands */
#define LG_CMD_REGISTER 0x00000200 /* register/drop */
#define LG_CMD_SET      0x00000400 /* change properties of static data */
#define LG_CMD_DO       0x00000800 /* change properties of dynamic data */
#define LG_CMD_LOGIN    0x00001000 /* login/logout */
#define LG_CMD_GET      0x00002000 /* query information */
#define LG_CMD_REQUEST	0x00004000 /* requests made by users */
/* other */
#define LG_NETWORK      0x00010000 /* netsplit/netjoin */
#define LG_WALLOPS      0x00020000 /* NOTYET wallops from opers/other servers */
#define LG_RAWDATA      0x00040000 /* all data sent/received */
#define LG_REGISTER     0x00080000 /* all registration related messages */
#define LG_WARN1        0x00100000 /* NOTYET messages formerly walloped */
#define LG_WARN2        0x00200000 /* NOTYET messages formerly snooped */
#define LG_DENYCMD	0x00400000 /* commands denied by security policy */

#define LG_CMD_ALL      0x0000FF00
#define LG_ALL          0x7FFFFFFF /* XXX cannot use bit 31 as it would then be equal to TOKEN_UNMATCHED */

/* aliases for use with logcommand() */
#define CMDLOG_ADMIN    LG_CMD_ADMIN
#define CMDLOG_REGISTER (LG_CMD_REGISTER | LG_REGISTER)
#define CMDLOG_SET      LG_CMD_SET
#define CMDLOG_REQUEST	LG_CMD_REQUEST
#define CMDLOG_DO       LG_CMD_DO
#define CMDLOG_LOGIN    LG_CMD_LOGIN
#define CMDLOG_GET      LG_CMD_GET

extern void log_open(void);
extern void log_shutdown(void);
extern bool log_debug_enabled(void);
extern void log_master_set_mask(unsigned int mask);
extern struct logfile *logfile_find_mask(unsigned int log_mask);
extern void slog(unsigned int level, const char *fmt, ...) ATHEME_FATTR_PRINTF(2, 3);
extern void logcommand(struct sourceinfo *si, int level, const char *fmt, ...) ATHEME_FATTR_PRINTF(3, 4);
extern void logcommand_user(struct service *svs, struct user *source, int level, const char *fmt, ...) ATHEME_FATTR_PRINTF(4, 5);
extern void logcommand_external(struct service *svs, const char *type, struct connection *source, const char *sourcedesc, struct myuser *login, int level, const char *fmt, ...) ATHEME_FATTR_PRINTF(7, 8);

/* function.c */

typedef void (*email_canonicalizer_fn)(char email[static (EMAILLEN + 1)], void *user_data);

struct email_canonicalizer_item
{
	email_canonicalizer_fn func;
	void *user_data;
	mowgli_node_t node;
};

/* misc string stuff */
extern char *random_string(size_t sz);
extern const char *create_weak_challenge(struct sourceinfo *si, const char *name);
extern void tb2sp(char *line);
extern char *replace(char *s, int size, const char *old, const char *new);
extern const char *number_to_string(int num);
extern int validemail(const char *email);
extern stringref canonicalize_email(const char *email);
extern void canonicalize_email_case(char email[static (EMAILLEN + 1)], void *user_data);
extern void register_email_canonicalizer(email_canonicalizer_fn func, void *user_data);
extern void unregister_email_canonicalizer(email_canonicalizer_fn func, void *user_data);
extern bool email_within_limits(const char *email);
extern bool validhostmask(const char *host);
extern char *pretty_mask(char *mask);
extern bool validtopic(const char *topic);
extern bool has_ctrl_chars(const char *text);
extern char *sbytes(float x);
extern float bytes(float x);

extern unsigned long makekey(void);
extern int srename(const char *old_fn, const char *new_fn);

/* time stuff */
#if HAVE_GETTIMEOFDAY
extern void s_time(struct timeval *sttime);
extern void e_time(struct timeval sttime, struct timeval *ttime);
extern int tv2ms(struct timeval *tv);
#endif
extern char *time_ago(time_t event);
extern char *timediff(time_t seconds);

#ifndef timersub
#define timersub(tvp, uvp, vvp)                                         \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
                if ((vvp)->tv_usec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_usec += 1000000;                      \
                }                                                       \
        } while (0)
#endif

/* tokenize.c */
extern int sjtoken(char *message, char delimiter, char **parv);
extern int tokenize(char *message, char **parv);

/* ubase64.c */
extern const char *uinttobase64(char *buf, uint64_t v, int64_t count);
extern unsigned int base64touint(const char *buf);
extern void decode_p10_ip(const char *b64, char ipstring[HOSTIPLEN + 1]);

/* sharedheap.c */
extern mowgli_heap_t *sharedheap_get(size_t size);
extern void sharedheap_unref(mowgli_heap_t *heap);
extern char *combine_path(const char *parent, const char *child);

#if !HAVE_VSNPRINTF
int rpl_vsnprintf(char *, size_t, const char *, va_list) ATHEME_FATTR_PRINTF(3, 0);
#endif
#if !HAVE_SNPRINTF
int rpl_snprintf(char *, size_t, const char *, ...) ATHEME_FATTR_PRINTF(3, 4);
#endif
#if !HAVE_VASPRINTF
int rpl_vasprintf(char **, const char *, va_list) ATHEME_FATTR_PRINTF(2, 0);
#endif
#if !HAVE_ASPRINTF
int rpl_asprintf(char **, const char *, ...) ATHEME_FATTR_PRINTF(2, 3);
#endif

#endif
