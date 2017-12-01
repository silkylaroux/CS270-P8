/*                                                                      tab:8
 * "Copyright (c) 2003 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written
 * agreement is hereby granted, provided that the above copyright notice
 * and the following two paragraphs appear in all copies of this software,
 * that the files COPYING and NO_WARRANTY are included verbatim with
 * any distribution, and that the contents of the file README are included
 * verbatim as part of a file named README with any distribution.
 *
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE AUTHOR
 * HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
 * BASIS, AND THE AUTHOR NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT,
 * UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Modified extensively by Fritz Sieker (2012-2016) for cs270 @ Colorado State
 * University. The major work was to break out parts of the simulator into
 * separate files that students could implement as part of a number of
 * programming assignments.
 */

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

/** @mainpage cs270 Programming Assignment - LC3 Simulator
 *  \htmlinclude "LC3SIM.html"
 */

/** @file lc3sim.c
 *  @brief driver for the simulator (do not modify)
 *  @details This program is the driver for the LC3 simulator. The actual
 *  simulator is mostly contained in hardware.c and logic.c. This code provides
 *  a debugger type interface to the simulator. It allows you to
 *  <ul>
 *  <li>load files</li>
 *  <li>examine and set values in registers</li>
 *  <li>exam and set values in memory</li>
 *  <li>set/clear breakpoints</li>
 *  <li>single step</li>
 *  <li>ste inot/over subroutine calls</i>
 *  <li>...</li>
 *  </ul>
 *  It also handles communication with the GUI Front end program lc3sim-tk.
 *  <p>
 *  @author Steven S. Lumetta/Fritz Sieker
 */

#if defined(USE_READLINE)
#include <readline/readline.h>
#include <readline/history.h>
#endif

/* additional files added by fritz */
#include "Debug.h"
#include "symbol.h"
#include "lc3.h"
#include "hardware.h"
#include "logic.h"
#include "install.h"

typedef enum reg_num_t reg_num_t;

enum reg_num_t {
        R_R0 = 0, R_R1, R_R2, R_R3, R_R4, R_R5, R_R6, R_R7,
        R_PC, R_IR, R_PSR,
        NUM_REGS
};

/* instruction flags */

typedef enum inst_flag_t inst_flag_t;
enum inst_flag_t {
    FLG_NONE       = 0,
    FLG_SUBROUTINE = 1,
    FLG_RETURN     = 2
};

//#define INSTALL_DIR "/s/bach/a/class/cs270/lc3tools"

/* Disassembly format specification. */
#define OPCODE_WIDTH 7 

/* NOTE: hardcoded in scanfs! */
#define MAX_CMD_WORD_LEN    41    /* command word limit + 1 */
#define MAX_FILE_NAME_LEN  251    /* file name limit + 1    */
#define MAX_LABEL_LEN       81    /* label limit + 1        */

#define MAX_SCRIPT_DEPTH    10    /* prevent infinite recursion in scripts */
#define MAX_FINISH_DEPTH 10000000 /* avoid waiting to finish subroutine    */
				  /* that recurses infinitely              */

#define TOO_MANY_ARGS     "WARNING: Ignoring excess arguments."
#define BAD_ADDRESS       \
	"Addresses must be labels or values in the range x0000 to xFFFF."

/* 
   Types of breakpoints.  Currently only user breakpoints are
   handled in this manner; the system breakpoint used for the
   "next" command is specified by sys_bpt_addr.
*/
typedef enum bpt_type_t bpt_type_t;
enum bpt_type_t {BPT_NONE, BPT_USER};

#define UNSIGNED

extern char* strdup(const char* s);

static int quiet          = 0; /* fritz */
static int run_os_on_init = 1; /* fritz */

static int launch_gui_connection ();
static char* simple_readline (const char* prompt);

static void init_machine ();
static void print_register (int which);
static void print_registers ();
static void dump_delayed_mem_updates ();
static void show_state_if_stop_visible ();
static int read_obj_file (const char* filename, int* startp, int* endp);
static int read_sym_file (const UNSIGNED char* filename);
static void squash_symbols (int addr_s, int addr_e);
static int execute_instruction ();
static void disassemble_one (int addr);
static void disassemble (int addr_s, int addr_e);
static void dump_memory (int addr_s, int addr_e);
static void run_until_stopped ();
static void clear_breakpoint (int addr);
static void clear_all_breakpoints ();
static void list_breakpoints ();
static void set_breakpoint (int addr);
static void warn_too_many_args ();
static void no_args_allowed (const UNSIGNED char* args);
static int parse_address (const UNSIGNED char* addr);
static int parse_range (const UNSIGNED char* cmd, int* startptr, int* endptr, 
			int last_end, int scale);
static void flush_console_input ();
static void gui_stop_and_dump ();
static int str2reg (const char* name);

static void cmd_break     (const UNSIGNED char* args);
static void cmd_continue  (const UNSIGNED char* args);
static void cmd_dump      (const UNSIGNED char* args);
static void cmd_execute   (const UNSIGNED char* args);
static void cmd_file      (const UNSIGNED char* args);
static void cmd_finish    (const UNSIGNED char* args);
static void cmd_help      (const UNSIGNED char* args);
static void cmd_list      (const UNSIGNED char* args);
static void cmd_memory    (const UNSIGNED char* args);
static void cmd_next      (const UNSIGNED char* args);
static void cmd_option    (const UNSIGNED char* args);
static void cmd_printregs (const UNSIGNED char* args);
static void cmd_quit      (const UNSIGNED char* args);
static void cmd_register  (const UNSIGNED char* args);
static void cmd_reset     (const UNSIGNED char* args);
static void cmd_step      (const UNSIGNED char* args);
static void cmd_translate (const UNSIGNED char* args);
static void cmd_lc3_stop  (const UNSIGNED char* args);

extern LC3_WORD get_PSR (void);
extern void set_PSR (int val);
extern LC3_WORD get_IR (void);

typedef enum cmd_flag_t cmd_flag_t;
enum cmd_flag_t {
    CMD_FLAG_NONE       = 0,
    CMD_FLAG_REPEATABLE = 1, /* pressing ENTER repeats command  */
    CMD_FLAG_LIST_TYPE  = 2, /* pressing ENTER shows more       */
    CMD_FLAG_GUI_ONLY   = 4  /* only valid in GUI mode          */
};

typedef struct command_t command_t;
struct command_t {
    UNSIGNED char* command;  /* string for command                     */
    int min_len;    /* minimum length for abbrevation--typically 1     */
    void (*cmd_func) (const UNSIGNED char*);  
                    /* function implementing command                   */
    cmd_flag_t flags; /* flags for command properties                  */
};

static const struct command_t command[] = {
    {"break",     1, cmd_break,     CMD_FLAG_NONE      },
    {"continue",  1, cmd_continue,  CMD_FLAG_REPEATABLE},
    {"dump",      1, cmd_dump,      CMD_FLAG_LIST_TYPE },
    {"execute",   1, cmd_execute,   CMD_FLAG_NONE      },
    {"file",      1, cmd_file,      CMD_FLAG_NONE      },
    {"finish",    3, cmd_finish,    CMD_FLAG_REPEATABLE},
    {"help",      1, cmd_help,      CMD_FLAG_NONE      },
    {"list",      1, cmd_list,      CMD_FLAG_LIST_TYPE },
    {"memory",    1, cmd_memory,    CMD_FLAG_NONE      },
    {"next",      1, cmd_next,      CMD_FLAG_REPEATABLE},
    {"option",    1, cmd_option,    CMD_FLAG_NONE      },
    {"printregs", 1, cmd_printregs, CMD_FLAG_NONE      },
    {"quit",      4, cmd_quit,      CMD_FLAG_NONE      },
    {"register",  1, cmd_register,  CMD_FLAG_NONE      },
    {"reset",     5, cmd_reset,     CMD_FLAG_NONE      },
    {"step",      1, cmd_step,      CMD_FLAG_REPEATABLE},
    {"translate", 1, cmd_translate, CMD_FLAG_NONE      },
    {"x",         1, cmd_lc3_stop,  CMD_FLAG_GUI_ONLY  },
    {NULL,        0, NULL,          CMD_FLAG_NONE      }
};

static int lc3_show_later[65536];
static bpt_type_t lc3_breakpoints[65536];

/* startup script or file */
static const char* start_script = NULL;
static       char* start_file = NULL;

int should_halt = 1;
static int gui_mode = 0;
static int interrupted_at_gui_request = 0, stop_scripts = 0, in_init = 0;
static int have_mem_to_dump = 0, need_a_stop_notice = 0;
static int sys_bpt_addr = -1, finish_depth = 0;
static inst_flag_t last_flags;
/* options and script recursion level */
static int flush_on_start = 1, keep_input_on_stop = 1;
static int rand_device = 1, delay_mem_update = 1;
static int script_uses_stdin = 1, script_depth = 0;

// initialized in main()
static char* lc3os_obj = NULL;
static char* lc3os_sym = NULL;

static FILE* lc3in;
static FILE* lc3out;
static FILE* sim_in;
static char* (*lc3readline) (const char*) = simple_readline;

static const char* const ccodes[8] = {
    "BAD_CC", "POSITIVE", "ZERO", "BAD_CC",
    "NEGATIVE", "BAD_CC", "BAD_CC", "BAD_CC"
};

static const UNSIGNED char* const rname[NUM_REGS + 1] = {
  "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", 
  "PC", "IR", "PSR", "CC"
};

/*
static void show_error (const char * msg) {
  if (gui_mode)
    printf("ERR {%s}\n", msg);
  else
    puts(msg);
}
*/

static sym_table_t* lc3_sym_tab;

#ifdef WRAP_POLL
int charAvail() { // wrap poll for possible conversion to Windows
  struct pollfd p;
  p.fd = fileno(lc3in);
  p.events = POLLIN;

  return (   (poll(&p, 1, 0) == 1)         /* poll returned an event      */
          && ((p.revents & POLLIN) != 0)); /* something available to read */
}
#endif

static void show_error (const char*  format, ...) {
  if (gui_mode) printf("ERR {");

  va_list arglist;
  va_start(arglist, format);
  vprintf(format, arglist);
  va_end(arglist);

  if (gui_mode) printf("}");

  printf("\n");
}

static int getPC (void) {
  hardware_gate_PC();
  return *lc3_BUS;
}

static int getReg (int i) {
  i = abs(i) % NUM_REGS;

  if (i <= R_R7)
    return logic_read_reg(i); /*REG(i);*/

  else if (i == R_PC)
    return getPC();

  else if (i == R_IR)
    return hardware_get_IR();

  else if (i == R_PSR)
    return get_PSR();

  return 0;
}

static void setReg (int i, LC3_WORD val) {
  i = abs(i) % NUM_REGS;

  if (i <= R_R7)
    logic_write_reg(i, val);

  else if (i == R_PC)
    hardware_set_PC(val);

  else if (i == R_IR)
    show_error("setting the IR disabled");

  else if (i == R_PSR)
    set_PSR(val);
}

int execute_instruction () {
  instruction_t inst;

  if (hardware_step(&inst) != 0) {
    hardware_set_PC(inst.addr);
    show_error("Illegal instruction at x%04X", inst.addr);
    return 0;
  }

  if ((inst.opcode == OP_JSR_JSRR) || (inst.opcode == OP_TRAP)) {
    last_flags = FLG_SUBROUTINE;
  }
  else if ((inst.opcode == OP_JMP_RET) && (inst.SR1 == 7)) {
    last_flags |= FLG_RETURN;
  }
  else {
    last_flags = FLG_NONE;
  }

  int currPC = getPC(); /* now incremented */

  /* Check for user breakpoints. */
  if (lc3_breakpoints[currPC] == BPT_USER) {
    if (!gui_mode)
      printf ("The LC-3 hit a breakpoint...\n");

    return 0;
  }

  /* Check for system breakpoint (associated with "next" command). */
  if (currPC == sys_bpt_addr)
    return 0;

  if (finish_depth > 0) {
        if ((last_flags & FLG_SUBROUTINE) && 
	    ++finish_depth == MAX_FINISH_DEPTH) {
            show_error("Stopping due to possibly infinite recursion.");
	    finish_depth = 0;
	    return 0;
	} else if ((last_flags & FLG_RETURN) && --finish_depth == 0) {
	    /* Done with finish command; stop execution. */
	    return 0;
	}
    }

    /* Check for GUI needs. */
    if (!in_init && gui_mode) {
	struct pollfd p;

	p.fd = fileno (sim_in);
	p.events = POLLIN;
	if (poll (&p, 1, 0) == 1 && (p.revents & POLLIN) != 0) {
	    interrupted_at_gui_request = 1;
	    return 0;
	}
    }

    return 1;
}


void halt_lc3 (int sig) {
    /* Set the signal handler again, which has the effect of overriding
       the Solaris behavior and making signal look like sigset, which
       is non-standard and non-portable, but the desired behavior. */
    signal (SIGINT, halt_lc3);

    /* has no effect unless LC-3 is running... */
    should_halt = 1;

    /* print a stop notice after ^C */
    need_a_stop_notice = 1;
}


static int launch_gui_connection () {
    u_short port;
    int fd;                   /* server socket file descriptor   */
    struct sockaddr_in addr;  /* server socket address           */

    /* wait for the GUI to tell us the portfor the LC-3 console socket */
    if (fscanf (sim_in, "%hd", &port) != 1)
        return -1;

    /* don't buffer output to GUI */
    if (setvbuf (stdout, NULL, _IONBF, 0) == -1)
    	return -1;

    /* create a TCP socket */
    if ((fd = socket (PF_INET, SOCK_STREAM, 0)) == -1)
	return -1;

    /* bind the port to the loopback address with any port */
    bzero (&addr, sizeof (addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    addr.sin_port        = 0;
    if (bind (fd, (struct sockaddr*)&addr, sizeof (addr)) == -1) {
	close (fd);
	return -1;
    }

    /* now connect to the given port */
    addr.sin_port = htons (port);
    if (connect (fd, (struct sockaddr*)&addr, 
    	         sizeof (struct sockaddr_in)) == -1) {
	close (fd);
	return -1;
    }

    /* use it for LC-3 keyboard and display I/O */
    if ((lc3in = fdopen (fd, "r")) == NULL ||
	(lc3out = fdopen (fd, "w")) == NULL ||
	setvbuf (lc3out, NULL, _IONBF, 0) == -1) {
	close (fd);
        return -1;
    }

    return 0;
}


static char* simple_readline (const char* prompt) {
    char buf[200];
    char* strip_nl;
    struct pollfd p;

    /* If we exhaust all commands after being interrupted by the
       GUI, start running again... */
    if (gui_mode) {
	p.fd = fileno (sim_in);
	p.events = POLLIN;
	if ((poll (&p, 1, 0) != 1 || (p.revents & POLLIN) == 0) &&
	    interrupted_at_gui_request) {
	    /* flag is reset to 0 in cmd_continue */
	    return strdup ("c");
	}
    }

    /* Prompt and read a line until successful. */
    while (1) {

#if !defined(USE_READLINE)
	if (!gui_mode && script_depth == 0)
	    printf ("%s", prompt);
#endif
	/* read a line */
	if (fgets (buf, 200, sim_in) != NULL)
	    break;

	/* no more input? */
    	if (feof (sim_in))
	    return NULL;

    	/* Otherwise, probably a CTRL-C, so print a blank line and
	   (possibly) another prompt, then try again. */
    	puts ("");
    }

    /* strip carriage returns and linefeeds */
    for (strip_nl = buf + strlen (buf) - 1;
    	 strip_nl >= buf && (*strip_nl == '\n' || *strip_nl == '\r');
	 strip_nl--);
    *++strip_nl = 0;

    return strdup (buf);
}


static void command_loop () {
    int cword_len;
    UNSIGNED char* cmd = NULL;
    UNSIGNED char* start;
    UNSIGNED char* last_cmd = NULL;
    UNSIGNED char cword[MAX_CMD_WORD_LEN];
    const command_t* a_command;

    while (!stop_scripts && (cmd = lc3readline ("(lc3sim) ")) != NULL) {
	/* Skip white space. */
	for (start = cmd; isspace (*start); start++);
	if (*start == '\0') {
	    /* An empty line repeats the last command, if allowed. */
	    free (cmd);
	    if ((cmd = last_cmd) == NULL)
	    	continue;
	    /* Skip white space. */
	    for (start = cmd; isspace (*start); start++);
	} else if (last_cmd != NULL)
	    free (last_cmd);
	last_cmd = NULL;

	/* Should never fail; just ignore the command if it does. */
	/* 40 below == MAX_CMD_WORD_LEN - 1 */
	if (sscanf (start, "%40s", cword) != 1) {
	    free (cmd);
	    break;
	}

	/* Record command word length, then point to arguments. */
	cword_len = strlen (cword);
	for (start += cword_len; isspace (*start); start++);
		
	/* Match command word to list of commands. */
	a_command = command; 
	while (1) {
	    if (a_command->command == NULL) {
		/* No match found--complain! */
		free (cmd);
		printf ("Unknown command.  Type 'h' for help.\n");
		break;
	    }

	    /* Try to match a_command. */
	    if (strncasecmp (cword, a_command->command, cword_len) == 0 &&
	        cword_len >= a_command->min_len &&
	        (gui_mode || (a_command->flags & CMD_FLAG_GUI_ONLY) == 0)) {

		/* Execute the command. */
		(*a_command->cmd_func) (start);

		/* Handle list type and repeatable commands. */
		if (a_command->flags & CMD_FLAG_LIST_TYPE) {
		    UNSIGNED char buf[MAX_CMD_WORD_LEN + 5];

		    strcpy (buf, cword);
		    strcat (buf, " more");
		    last_cmd = strdup (buf);
		} else if (a_command->flags & CMD_FLAG_REPEATABLE &&
		           script_depth == 0) {
		    last_cmd = cmd;
		} else {
		    free (cmd);
		}
		break;
	    }
	    a_command++;
	}
    }
}

static char* make_path (char* name) {
  char* path = malloc(strlen(install_dir) + strlen(name) +2);
  sprintf(path, "%s/%s", install_dir, name);
  return path;
}

int main (int argc, const char* argv[]) {
    debugInit(&argc, argv);

    /* check for quiet/norun/gui flags - fritz */
    while (argc > 1) {
      if (strcmp(argv[1], "-quiet") == 0) {
        quiet = 1;
        argc--;
        argv++;
      }
      else if (strcmp(argv[1], "-norun") == 0) {
        run_os_on_init = 0;
        argc--;
        argv++;
      }
      else if (strcmp (argv[1], "-gui") == 0) {
	gui_mode = 1;
        argc--;
        argv++;
      }
      else {
	break;
      }
    } /* fritz */

    /* check for -gui argument */
    sim_in = stdin;
    if (gui_mode) {
	quiet = 0; /* quiet and -gui are not comaptabile */
    	if (launch_gui_connection () != 0) {
	    printf ("failed to connect to GUI\n");
	    return 1;
	}
    } else {
        lc3out = stdout;
	lc3in = stdin;
	gui_mode = 0;
#if defined(USE_READLINE)
	lc3readline = readline;
#endif
    }

    lc3os_obj   = make_path("lc3os.obj");
    lc3os_sym   = make_path("lc3os.sym");
    lc3_sym_tab = symbol_init (SYM_TAB_SIZE);

    /* used to simulate random device timing behavior */
    srandom (time (NULL));

    /* used to halt LC-3 when CTRL-C pressed */
    signal (SIGINT, halt_lc3);

    /* load any object, symbol, or script files requested on command line */
    if (argc == 3 && strcmp (argv[1], "-s") == 0) {
	start_script = argv[2];
	init_machine (); /* also executes script */
	return 0;
    } else if (argc == 2 && strcmp (argv[1], "-h") != 0) {
	start_file = strdup (argv[1]);
	init_machine (); /* also loads file */
    } else if (argc != 1) {
	/* argv[0] may not be valid if -gui entered */
	printf ("syntax: lc3sim [<object file>|<symbol file>]\n");
	printf ("        lc3sim [-s <script file>]\n");
	printf ("        lc3sim -h\n");
	return 0;
    } else
    	init_machine ();

    command_loop ();

    puts ("");
    return 0;
}

/* This method simulates the variable time an I/O operation may take. If
 * rand_device is 0, I/O completes immediately. If it is 1 (the default),
 * the time to complete an I/O is variable. The LC3 OS will end up busy
 * waiting until it "completes". The value of rand_device is controlled with
 * the command: device on|off
 */
static int io_complete(void) {
  return (! rand_device || (random() & 15) == 0);
}

LC3_WORD get_keyboard_status (void) {
  int status = 0;                     /* no keystroke available */
  struct pollfd p;
  p.fd = fileno(lc3in);
  p.events = POLLIN;

  if (   (poll(&p, 1, 0) == 1)        /* poll returned an event      */
      && ((p.revents & POLLIN) != 0)  /* something available to read */
      && io_complete()) {             
      status = 0x8000;                /* key has been pressed        */
  }

  return status;
}

LC3_WORD get_keystroke (void) {
  int ch = fgetc(lc3in);

  if (ch != -1)
    return ch;

  /* Error in fgetc() */
  /* Should not happen in GUI mode. */
  /* FIXME: This won't show up correctly in GUI.
     Exit is likely to be detected first, and error message
     given (LC-3 sim. died), followed by message below 
     (read past end), then Tcl/Tk error caused by bad
     window access after sim died.  Confusing sequence
     if it occurs. */
  show_error("LC-3 read past end of input stream.");
  exit(3);
  return 0;
}

LC3_WORD get_display_status (void) {
  int status = 0;

  if (io_complete())
    status = 0x8000; /* display ready for more data */

  return status;
}

void display_char (LC3_WORD ch) {
  fprintf (lc3out, "%c", ch);
  fflush (lc3out);
}

void memory_updated (LC3_WORD addr) {
  if (gui_mode) {
    if (! delay_mem_update)
      disassemble_one (addr);
    else {
      lc3_show_later[addr] = 1;
      have_mem_to_dump = 1; /* a hint */
    }
  }
}

static int read_obj_file (const char* filename, int* startp, int* endp) {
  int start, addr, val;
  FILE* f = fopen(filename, "r");

  if (f == NULL)
    return -1;

  lc3_set_obj_file_mode(filename);

  start = addr = lc3_read_LC3_word(f);

  while ((val = lc3_read_LC3_word(f)) != -1) {
    logic_write_memory(addr, val);
    addr = (addr + 1) & 0xFFFF;
  }

  fclose (f);
  squash_symbols (start, addr);
  *startp = start;
  *endp = addr;

  return 0;
}

static int read_sym_file (const char* filename) {
  FILE* f;

  if ((f = fopen (filename, "r")) == NULL)
    return -2;

  lc3_read_sym_table(f, lc3_sym_tab);
  fclose (f);
  return 0;
}

static void squash_symbols (int addr_s, int addr_e)
{
    while (addr_s != addr_e) {
//	symbol_remove_at_addr (addr_s);
	addr_s = (addr_s + 1) & 0xFFFF;
    }
}

void hardware_reset(void); /* fritz */

static void 
init_machine ()
{
    in_init = 1;

    hardware_reset();
    bzero (lc3_show_later, sizeof (lc3_show_later));
    symbol_reset(lc3_sym_tab);
    clear_all_breakpoints ();
    int os_start, os_end;

    if (read_obj_file (lc3os_obj, &os_start, &os_end) == -1) {
      show_error("Failed to read LC-3 OS code.");
      show_state_if_stop_visible ();
    } else {
        if (read_sym_file (lc3os_sym) == -1) {
          show_error("Failed to read LC-3 OS symbols.");
        }
        if (gui_mode) /* load new code into GUI display */
            disassemble (os_start, os_end);
        /* REG (R_PC) = 0x0200; */
        hardware_set_PC(0x0200);

        if (quiet) { /* fritz */
          symbol_t* quietFlag = symbol_find_by_name(lc3_sym_tab, "OS_QUIET");

          if (quietFlag)
            logic_write_memory(quietFlag->addr, 1);
        } /* fritz */

        if (run_os_on_init) /* fritz */
          run_until_stopped ();
    }

    in_init = 0;

    if (start_script != NULL)
	cmd_execute (start_script);
    else if (start_file != NULL)
	cmd_file (start_file);
}

static void print_register (int which) { /* only called in GUI mode */
    LC3_WORD value = getReg(which);
    printf ("REG R%d x%04X\n", which, value);
    /* condition codes are not stored outside of PSR */
    if (which == R_PSR)
      printf ("REG R%d %s\n", NUM_REGS, ccodes[hardware_get_CC()]);
    if (gui_mode)
      printf ("TOCODE\n");
}

static void
print_registers ()
{
    int regnum;
    LC3_WORD pc  = getPC();
    LC3_WORD psr = get_PSR();
    LC3_WORD ir  = getReg(R_IR);

    if (!gui_mode) {
      if (! quiet) { /* fritz */
	printf ("PC=x%04X IR=x%04X PSR=x%04X (%s)\n", pc, ir, psr,
                 ccodes[hardware_get_CC()]);
	for (regnum = 0; regnum < R_PC; regnum++)
	    printf ("R%d=x%04X ", regnum, getReg (regnum));
	puts ("");
      } /* fritz */
     disassemble_one (pc);
    } else {
	for (regnum = 0; regnum <= R_R7; regnum++)
	    printf ("REG R%d x%04X\n", regnum, getReg (regnum));

	printf ("REG R8 x%04X\n", pc);
	printf ("REG R9 x%04X\n", ir);
	printf ("REG R10 x%04X\n", psr);
    	printf ("REG R11 %s\n", ccodes[hardware_get_CC()]);
    }
}

static void 
dump_delayed_mem_updates ()
{
    int addr;

    if (!have_mem_to_dump)
        return;
    have_mem_to_dump = 0;

    /* FIXME: Could use a hash table here, but hint is probably enough. */
    for (addr = 0; addr < 65536; addr++) {
        if (lc3_show_later[addr]) {
	    disassemble_one (addr);
	    lc3_show_later[addr] = 0;
	}
    }
}

static void
show_state_if_stop_visible ()
{
    /* 
       If the GUI has interrupted the simulator (e.g., to set or clear
       a breakpoint), print nothing.  The simulator restarts automatically
       unless a new file is loaded, in which case cmd_file performs the
       updates. 
    */
    if (interrupted_at_gui_request)
        return;

    if (gui_mode && delay_mem_update)
	dump_delayed_mem_updates ();

    print_registers ();
}

#include "decode.def"
#include "disassemble.def"

static void disassemble (int addr_s, int addr_e) {
  do {
    disassemble_one (addr_s);
    addr_s = (addr_s + 1) & 0xFFFF;
  } while (addr_s != addr_e);
}

static void dump_memory (int addr_s, int addr_e) {
    int start, addr, i;
    int a[12];

    if (addr_s >= addr_e)
        addr_e += 0x10000;
    for (start = (addr_s / 12) * 12; start < addr_e; start = start + 12) {
        printf ("%04X: ", start & 0xFFFF);
	for (i = 0, addr = start; i < 12; i++, addr++) {
	    if (addr >= addr_s && addr < addr_e)
	        printf ("%04X ", (a[i] = logic_read_memory (addr & 0xFFFF)));
	    else
	        printf ("     ");
	}
	printf (" ");
	for (i = 0, addr = start; i < 12; i++, addr++) {
	    if (addr >= addr_s && addr < addr_e)
	        printf ("%c", (a[i] < 0x100 && isprint (a[i])) ? a[i] : '.');
	    else
	        printf (" ");
	}
	puts ("");
    }
}


static void run_until_stopped () {
    struct termios tio;
    int old_lflag, old_min, old_time, tty_fail;

    should_halt = 0;
    if (gui_mode) {
	/* removes PC marker in GUI */
	printf ("CONT\n");
        tty_fail = 1;
    } else if (!isatty (fileno (lc3in)) || 
    	       tcgetattr (fileno (lc3in), &tio) != 0)
        tty_fail = 1;
    else {
        tty_fail = 0;
	old_lflag = tio.c_lflag;
	old_min = tio.c_cc[VMIN];
	old_time = tio.c_cc[VTIME];
	tio.c_lflag &= ~(ICANON | ECHO);
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	(void)tcsetattr (fileno (lc3in), TCSANOW, &tio);
    }

    while (!should_halt && execute_instruction ());

    if (!tty_fail) {
	tio.c_lflag = old_lflag;
	tio.c_cc[VMIN] = old_min;
	tio.c_cc[VTIME] = old_time;
	(void)tcsetattr (fileno (lc3in), TCSANOW, &tio);
	/* 
	   Discard any remaining input if requested.  This flush occurs
	   when the LC-3 stops, in which case any remaining input
	   to the console will be treated as simulator commands if it
	   is not discarded.

	   However, discarding can interfere with command sequences that 
	   include moderately long execution periods.

	   As with gdb, not discarding is the default, since typing in
	   a bunch of random junk that happens to look like valid
	   commands happens less frequently than the case above, although
	   I myself have been bitten a few times in gdb by pressing
	   return once too often after issuing a repeatable command.
	*/
	if (!keep_input_on_stop)
	    (void)tcflush (fileno (lc3in), TCIFLUSH);
    }

    /* stopped by CTRL-C?  Check if we need a stop notice... */
    if (need_a_stop_notice) {
        printf ("\nLC-3 stopped.\n\n");
	need_a_stop_notice = 0;
    }

    /* 
       If stopped for any reason other than interruption by GUI,
       clear system breakpoint and terminate any "finish" command.
    */
    if (!interrupted_at_gui_request) {
	sys_bpt_addr = -1;
	finish_depth = 0;
    }

    /* Dump memory and registers if necessary. */
    show_state_if_stop_visible ();
}


static void clear_breakpoint (int addr) {
    if (lc3_breakpoints[addr] != BPT_USER) {
	if (!gui_mode)
	    printf ("No such breakpoint was set.\n");
    } else {
	if (gui_mode)
	    printf ("BCLEAR %d\n", addr + 1);
	else
	    printf ("Cleared breakpoint at x%04X.\n", addr);
    }
    lc3_breakpoints[addr] = BPT_NONE;
}


static void clear_all_breakpoints () {
    /* 
       If other breakpoint types are to be supported,
       this code needs to avoid clobbering non-user
       breakpoints.
    */
    bzero (lc3_breakpoints, sizeof (lc3_breakpoints));
}


static void list_breakpoints () {
    int i, found = 0;

    /* A bit hokey, but no big deal for this few. */
    for (i = 0; i < 65536; i++) {
    	if (lc3_breakpoints[i] == BPT_USER) {
	    if (!found) {
		printf ("The following instructions are set as "
			"breakpoints:\n");
		found = 1;
	    }
	    disassemble_one (i);
	}
    }

    if (!found)
    	printf ("No breakpoints are set.\n");
}


static void set_breakpoint (int addr) {
    if (lc3_breakpoints[addr] == BPT_USER) {
	if (!gui_mode)
	    printf ("That breakpoint is already set.\n");
    } else {
	lc3_breakpoints[addr] = BPT_USER;
	if (gui_mode)
	    printf ("BREAK %d\n", addr + 1);
	else
	    printf ("Set breakpoint at x%04X.\n", addr);
    }
}


static void cmd_break (const UNSIGNED char* args) {
    UNSIGNED char opt[11], addr_str[MAX_LABEL_LEN], trash[2];
    int num_args, opt_len, addr;

    /* 80 == MAX_LABEL_LEN - 1 */
    num_args = sscanf (args, "%10s%80s%1s", opt, addr_str, trash);

    if (num_args > 0) {
	opt_len = strlen (opt);
	if (strncasecmp (opt, "list", opt_len) == 0) {
	    if (num_args > 1)
		warn_too_many_args ();
	    list_breakpoints ();
	    return;
	}
	if (num_args > 1) {
	    if (num_args > 2)
		warn_too_many_args ();
	    addr = parse_address (addr_str);
	    if (strncasecmp (opt, "clear", opt_len) == 0) {
		if (strcasecmp (addr_str, "all") == 0) {
		    clear_all_breakpoints ();
		    if (!gui_mode)
			printf ("Cleared all breakpoints.\n");
		    return;
		}
		if (addr != -1)
		    clear_breakpoint (addr);
		else
		    puts (BAD_ADDRESS);
		return;
	    } else if (strncasecmp (opt, "set", opt_len) == 0) {
		if (addr != -1)
		    set_breakpoint (addr);
		else
		    puts (BAD_ADDRESS);
		return;
	    }
	}
    }

    printf ("breakpoint options include:\n");
    printf ("  break clear <addr>|all -- clear one or all breakpoints\n");
    printf ("  break list             -- list all breakpoints\n");
    printf ("  break set <addr>       -- set a breakpoint\n");
}


static void warn_too_many_args () {
    /* Spaces in entry boxes in the GUI appear as
       extra arguments when handed to the command line;
       we silently ignore them. */
    if (!gui_mode)
        puts (TOO_MANY_ARGS);
}


static void no_args_allowed (const UNSIGNED char* args) {
    if (*args != '\0')
        warn_too_many_args ();
}


static void cmd_continue (const UNSIGNED char* args) {
    no_args_allowed (args);
    if (interrupted_at_gui_request)
	interrupted_at_gui_request = 0;
    else
	flush_console_input ();
    run_until_stopped ();
}


static void cmd_dump (const UNSIGNED char* args) {
    static int last_end = 0;
    int start, end;

    if (parse_range (args, &start, &end, last_end, 48) == 0) {
	dump_memory (start, end);
	last_end = end;
	return;
    }

    printf ("dump options include:\n");
    printf ("  dump               -- dump memory around PC\n");
    printf ("  dump <addr>        -- dump memory starting from an "
    	    "address or label\n");
    printf ("  dump <addr> <addr> -- dump a range of memory\n");
    printf ("  dump more          -- continue previous dump (or press "
	    "<Enter>)\n");
}


static void cmd_execute (const UNSIGNED char* args) {
    FILE* previous_input;
    FILE* script;

    if (script_depth == MAX_SCRIPT_DEPTH) {
	/* Safer to exit than to bury a warning arbitrarily deep. */
        printf ("Cannot execute more than %d levels of scripts!\n",
		MAX_SCRIPT_DEPTH);
	stop_scripts = 1;
	return;
    }

    if ((script = fopen (args, "r")) == NULL) {
        printf ("Cannot open script file \"%s\".\n", args);
	stop_scripts = 1;
	return;
    }

    script_depth++;
    previous_input = sim_in;
    sim_in = script;
    if (!script_uses_stdin)
	lc3in = script;
#if defined(USE_READLINE)
    lc3readline = simple_readline;
#endif
    command_loop ();
    sim_in = previous_input;
    if (--script_depth == 0) {
	if (gui_mode) {
	    lc3in = lc3out;
	} else {
	    lc3in = stdin;
#if defined(USE_READLINE)
	    lc3readline = readline;
#endif
	}
    	stop_scripts = 0;
    } else if (!script_uses_stdin) {
	/* executing previous script level--take LC-3 console input 
	   from script */
	lc3in = previous_input;
    }
    fclose (script);
}


static void cmd_file (const UNSIGNED char* args) {
    /* extra 4 chars in buf for ".obj" possibly added later */ 
    UNSIGNED char buf[MAX_FILE_NAME_LEN + 4];
    UNSIGNED char* ext;
    int len, start, end, warn = 0;

    len = strlen (args);
    if (len == 0 || len > MAX_FILE_NAME_LEN - 1) {
	if (gui_mode)
	    printf ("ERR {Could not parse file name!}\n");
	else
	    printf ("syntax: file <file to load>\n");
	return;
    }
    strcpy (buf, args);
    /* FIXME: Need to use portable path element separator characters
       rather than assuming use of '/'. */
    if ((ext = strrchr (buf, '.')) == NULL || strchr (ext, '/') != NULL) {
	ext = buf + len;
        strcat (buf, ".obj");
    } else {
	if (!gui_mode && strcasecmp (ext, ".sym") == 0) {
	    if (read_sym_file (buf))
		printf ("Failed to read symbols from \"%s.\"\n", buf);
	    else
		printf ("Read symbols from \"%s.\"\n", buf);
	    return;
	}
	if (strcasecmp (ext, ".obj") != 0) {
	  show_error("Only .obj or .sym files can be loaded.");
	  return;
	}
    }
    if (read_obj_file (buf, &start, &end) == -1) {
      return;
    }
    /* Success: reload same file next time machine is reset. */
    if (start_file != NULL)
    	free (start_file);
    start_file = strdup (buf);

    strcpy (ext, ".sym");
    if (read_sym_file (buf))
        warn = 1;
    hardware_set_PC(start);

    /* GUI requires printing of new PC to reorient code display to line */
    if (gui_mode) {
	/* load new code into GUI display */
	disassemble(start, end);
	/* change focus in GUI */
	printf ("TOCODE\n");
    	print_register (R_PC);
    } else  {
	strcpy (ext, ".obj");
	printf ("Loaded \"%s\" and set PC to x%04X\n", buf, start);
    }
    if (warn)
      show_error("WARNING: No symbols are available.");

    /* Should not immediately start, even if we stopped simulator to
       load file.  We do need to update registers and dump delayed
       memory changes in that case, though.  Similarly, loading a
       file forces the simulator to forget completion of an executing
       "next" command. */
    if (interrupted_at_gui_request)
	gui_stop_and_dump ();
}


static void cmd_finish (const UNSIGNED char* args) {
    no_args_allowed (args);
    flush_console_input ();
    finish_depth = 1;
    run_until_stopped ();
}


static void cmd_help (const UNSIGNED char* args) {
    printf ("file <file>           -- file load (also sets PC to start of "
    	    "file)\n\n");

    printf ("break ...             -- breakpoint management\n\n");

    printf ("continue              -- continue execution\n");
    printf ("finish                -- execute to end of current subroutine\n");
    printf ("next                  -- execute next instruction (full "
    	    "subroutine/trap)\n");
    printf ("step                  -- execute one step (into "
    	    "subroutine/trap)\n\n");

    printf ("list ...              -- list instructions at the PC, an "
    	    "address, a label\n");
    printf ("dump ...              -- dump memory at the PC, an address, "
    	    "a label\n");
    printf ("translate <addr>      -- show the value of a label and print the "
    	    "contents\n");
    printf ("printregs             -- print registers and current "
    	    "instruction\n\n");

    printf ("memory <addr> <val>   -- set the value held in a memory "
    	    "location\n");
    printf ("register <reg> <val>  -- set a register to a value\n\n");


    printf ("execute <file name>   -- execute a script file\n\n");

    printf ("reset                 -- reset LC-3 and reload last file\n\n");

    printf ("quit                  -- quit the simulator\n\n");

    printf ("help                  -- print this help\n\n");

    printf ("All commands except quit can be abbreviated.\n");
}


static int parse_address (const UNSIGNED char* addr) {
    symbol_t* label;
    UNSIGNED char* fmt;
    int value, negated;
    UNSIGNED char trash[2];

    /* default matching order: symbol, hexadecimal */
    /* hexadecimal can optionally be preceded by x or X */
    /* decimal must be preceded by # */

    if (addr[0] == '-') {
	addr++;
	negated = 1;
    } else
	negated = 0;

    label = symbol_find_by_name(lc3_sym_tab, addr);
    if (label != NULL)
        value = label->addr;
    else {
	if (*addr == '#')
	    fmt = "#%d%1s";
	else if (tolower (*addr) == 'x')
	    fmt = "x%x%1s";
	else
	    fmt = "%x%1s";
	if (sscanf (addr, fmt, &value, trash) != 1 || value > 0xFFFF ||
	    ((negated && value < 0) || (!negated && value < -0xFFFF)))
	    return -1;
    }
    if (negated)
        value = -value;
    if (value < 0)
	value += 0x10000;
    return value;
}


static int
parse_range (const UNSIGNED char* args, int* startptr, int* endptr, 
             int last_end, int scale)
{
    UNSIGNED char arg1[MAX_LABEL_LEN], arg2[MAX_LABEL_LEN], trash[2];
    int num_args, start, end;

    /* Split and count the arguments. */
    /* 80 == MAX_LABEL_LEN - 1 */
    num_args = sscanf (args, "%80s%80s%1s", arg1, arg2, trash);

    /* If we have no automatic scaling for the range, we
       need both the start and the end to be specified. */
    if (scale < 0 && num_args < 2)
	return -1;

    /* With no arguments, use automatic scaling around the PC. */
    if (num_args < 1) {
        LC3_WORD pc = getPC();
	start = (pc + 0x10000 - scale) & 0xFFFF;
	end = (pc + scale) & 0xFFFF;
	goto success;
    }

    /* If the first argument is "more," start from the last stopping
       point.   Note that "more" also requires automatic scaling. */
    if (last_end >= 0 && strcasecmp (arg1, "more") == 0) {
	start = last_end;
	end = (start + 2 * scale) & 0xFFFF;
	if (num_args > 1)
	    warn_too_many_args ();
	goto success;
    }

    /* Parse the starting address. */
    if ((start = parse_address (arg1)) == -1)
	return -1;

    /* Scale to find the ending address if necessary. */
    if (num_args < 2) {
	end = (start + 2 * scale) & 0xFFFF;
	goto success;
    }

    /* Parse the ending address. */
    if ((end = parse_address (arg2)) == -1)
        return -1;

    /* For ranges, add 1 to specified ending address for inclusion 
       in output. */
    if (scale >= 0)
	end = (end + 1) & 0xFFFF;

    /* Check for superfluous arguments. */
    if (num_args > 2)
	warn_too_many_args ();

    /* Store the results and return success. */ 
success:
    *startptr = start;
    *endptr = end;
    return 0;
}


static void cmd_list (const UNSIGNED char* args) {
    static int last_end = 0;
    int start, end;

    if (parse_range (args, &start, &end, last_end, 10) == 0) {
	disassemble (start, end);
	last_end = end;
	return;
    }

    printf ("list options include:\n");
    printf ("  list               -- list instructions around PC\n");
    printf ("  list <addr>        -- list instructions starting from an "
	    "address or label\n");
    printf ("  list <addr> <addr> -- list a range of instructions\n");
    printf ("  list more          -- continue previous listing (or press "
	    "<Enter>)\n");
}


static void cmd_memory (const UNSIGNED char* args) {
    int addr, value;

    if (parse_range (args, &addr, &value, -1, -1) == 0) {
	logic_write_memory (addr, value);
	if (gui_mode) {
	    printf ("TRANS x%04X x%04X\n", addr, value);
	    disassemble_one (addr);
	} else
	    printf ("Wrote x%04X to address x%04X.\n", value, addr);
    } else {
	if (gui_mode) {
	    /* Address is provided by the GUI, so only the value can
	       be bad in this case. */
	    printf ("ERR {No address or label corresponding to the "
	    	    "desired value exists.}\n");
	} else
	    printf ("syntax: memory <addr> <value>\n");
    }
}


static void cmd_option (const UNSIGNED char* args) {
    UNSIGNED char opt[11], onoff[6], trash[2];
    int num_args, opt_len, oval;

    num_args = sscanf (args, "%10s%5s%1s", opt, onoff, trash);
    if (num_args >= 2) {
	opt_len = strlen (opt);
	if (strcasecmp (onoff, "on") == 0)
	    oval = 1;
	else if (strcasecmp (onoff, "off") == 0)
	    oval = 0;
	else
	    goto show_syntax;
        if (num_args > 2)
	    warn_too_many_args ();
        if (strncasecmp (opt, "flush", opt_len) == 0) {
	    flush_on_start = oval;
	    if (!gui_mode)
		printf ("Will %sflush the console input when starting.\n",
			oval ? "" : "not ");
	    return;
	}
        if (strncasecmp (opt, "keep", opt_len) == 0) {
	    keep_input_on_stop = oval;
	    if (!gui_mode)
		printf ("Will %skeep remaining input when the LC-3 stops.\n", 
			oval ? "" : "not ");
	    return;
	}
        if (strncasecmp (opt, "device", opt_len) == 0) {
	    rand_device = oval;
	    if (!gui_mode)
		printf ("Will %srandomize device interactions.\n",
			oval ? "" : "not ");
	    return;
	}
	/* GUI-only option: Delay memory updates to GUI until LC-3 stops? */
        if (gui_mode && strncasecmp (opt, "delay", opt_len) == 0) {
	    /* Make sure that if the option is turned off while the GUI
	       thinks that the processor is running, state is dumped
	       immediately. */
	    if (delay_mem_update && oval == 0) 
		dump_delayed_mem_updates ();
	    delay_mem_update = oval;
	    return;
	}
	/* Use stdin for LC-3 console input while running script? */
        if (strncasecmp (opt, "stdin", opt_len) == 0) {
	    script_uses_stdin = oval;
	    if (!gui_mode)
		printf ("Will %suse stdin for LC-3 console input during "
			"script execution.\n", oval ? "" : "not ");
	    if (script_depth > 0) {
	        if (!oval)
		    lc3in = sim_in;
		else if (!gui_mode)
		    lc3in = stdin;
		else
		    lc3in = lc3out;
	    }
	    return;
	}
    }

show_syntax:
    printf ("syntax: option <option> on|off\n   options include:\n");
    printf ("      device -- simulate random device (keyboard/display)"
    	    "timing\n");
    printf ("      flush  -- flush console input each time LC-3 starts\n");
    printf ("      keep   -- keep remaining input when the LC-3 stops\n");
    printf ("      stdin  -- use stdin for LC-3 console input during script "
    	    "execution\n");
    printf ("NOTE: all options are ON by default\n");
}


static void cmd_next (const UNSIGNED char* args) {
    int next_pc = (getPC() + 1) & 0xFFFF;

    no_args_allowed (args);
    flush_console_input ();

    /* Note that we might hit a breakpoint immediately. */
    if (execute_instruction ()) {  
	if ((last_flags & FLG_SUBROUTINE) != 0) {
	    /* 
	       Mark system breakpoint.  This approach allows the GUI
	       to interrupt the simulator without the simulator 
	       forgetting about the completion of this command (i.e., 
	       next).  Nesting of such commands is not supported,
	       and should not be possible to issue with the GUI.
	    */
	    sys_bpt_addr = next_pc;
	    run_until_stopped ();
	    return;
	}
    }

    /* Dump memory and registers if necessary. */
    show_state_if_stop_visible ();
}


static void cmd_printregs (const UNSIGNED char* args) {
#if 1
    char arg1[MAX_LABEL_LEN], trash[2];
    int num_args, rnum;

    if ((num_args = sscanf (args, "%80s%1s", arg1, trash)) < 1)
      print_registers ();
    else if ((rnum = str2reg(arg1)) >= 0) {
      LC3_WORD value = getReg(rnum);
      if (rnum < NUM_REGS)
        printf("REG %s x%04x\n", rname[rnum], value);
      else {
        printf("REG %s %s\n", rname[rnum], ccodes[hardware_get_CC()]);
      }
    }
#else
    no_args_allowed (args);
    print_registers ();
#endif
}


static void cmd_quit (const UNSIGNED char* args) {
    no_args_allowed (args);
    exit (0);
}

static int str2reg (const char* name) {
  int i;
  for (i = 0; i <= NUM_REGS; i++) {
    if (strcasecmp (rname[i], name) == 0)
       return i;
  }

  puts ("Registers are R0...R7, PC, IR, PSR, and CC.");
  return -1;
}

static void cmd_register (const UNSIGNED char* args) {
    static const UNSIGNED char* const cc_val[4] = {
	"POSITIVE", "ZERO", "", "NEGATIVE"
    };
    UNSIGNED char arg1[MAX_LABEL_LEN], arg2[MAX_LABEL_LEN], trash[2];
    int num_args, rnum, value, len;

    /* 80 == MAX_LABEL_LEN - 1 */
    num_args = sscanf (args, "%80s%80s%1s", arg1, arg2, trash);
    if (num_args < 2) {
	/* should never happen in GUI mode */
	printf ("syntax: register <reg> <value>\n");
	return;
    }

    /* Determine which register is to be set. */
#if 1
   rnum = str2reg(arg1);
   if (rnum < 0)
     return;
#else
    for (rnum = 0; ; rnum++) {
	if (rnum == NUM_REGS + 1) {
	    /* No match (should never happen in GUI mode). */
	    puts ("Registers are R0...R7, PC, IR, PSR, and CC.");
	    return;
	}
	if (strcasecmp (rname[rnum], arg1) == 0)
	    break;
    }
#endif

    /* Condition codes are a special case. */
    if (rnum == NUM_REGS) {
	len = strlen (arg2);
	for (value = 0; value < 4; value++) {
	    if (strncasecmp (arg2, cc_val[value], len) == 0) {
                LC3_WORD psr = get_PSR() & ~0x0E00;
		setReg(R_PSR, (psr | (value + 1) << 9));
		if (gui_mode)
		    /* printing PSR prints both PSR and CC */
		    print_register (R_PSR);
		else
		    printf ("Set CC to %s.\n", cc_val[value]);
		return;
	    }
	}
	show_error ("CC can only be set to NEGATIVE, ZERO, or POSITIVE.");
	return;
    } 

    if (rnum == R_IR) {
      show_error("setting the IR disabled");
      return;
    }
    /* Parse the value and set the register, or complain if it's a bad
       value. */
    if ((value = parse_address (arg2)) != -1) {
	setReg(rnum, value);
	if (gui_mode)
	    print_register (rnum);
	else
	    printf ("Set %s to x%04X.\n", rname[rnum], value);
    } else
	show_error("No address or label corresponding to the "
		   "desired value exists.");
}

static void cmd_reset (const UNSIGNED char* args) {
    int addr;

    if (script_depth > 0) {
	/* Should never be executing a script in GUI mode, but check... */
	show_error ("Cannot reset the LC-3 from within a script.");
    	return;
    }
    no_args_allowed (args);

    /* 
       If in GUI mode, we need to write over all memory with zeroes
       rather than just setting (so that disassembly info gets sent
       to GUI).
    */
    if (gui_mode) {
	interrupted_at_gui_request = 0;
        for (addr = 0; addr < 65536; addr++)
	    logic_write_memory (addr, 0);
    	gui_stop_and_dump ();
    }

    /* various bits of state to reset */
    have_mem_to_dump = 0;
    need_a_stop_notice = 0;
    sys_bpt_addr = -1;
    finish_depth = 0;

    init_machine ();

    /* change focus in GUI, and turn off delay cursor */
    if (gui_mode)
	printf ("TOCODE\n");
}


static void cmd_step (const UNSIGNED char* args) {
    no_args_allowed (args);
    flush_console_input ();
    execute_instruction ();
    /* Dump memory and registers if necessary. */
    show_state_if_stop_visible ();
}


static void cmd_translate (const UNSIGNED char* args) {
    UNSIGNED char arg1[81], trash[2];
    int num_args, value;

    /* 80 == MAX_LABEL_LEN - 1 */
    if ((num_args = sscanf (args, "%80s%1s", arg1, trash)) > 1)
    	warn_too_many_args ();

    if (num_args < 1) {
        puts ("syntax: translate <addr>");
	return;
    }

    /* Try to translate the value. */
    if ((value = parse_address (arg1)) == -1) {
	if (gui_mode)
	    printf ("ERR {No such address or label exists.}\n");
	else
	    puts (BAD_ADDRESS);
    	return;
    }

    char* label  = symbol_find_by_addr(lc3_sym_tab, value);
    int   memVal = logic_read_memory (value);

    if (gui_mode)
	printf ("TRANS x%04X x%04X\n", value, memVal);
    else if (label)
	printf ("Address %s has value x%04x.\n", label, memVal);
    else
	printf ("Address x%04X has value x%04x.\n", value, memVal);
}

static void gui_stop_and_dump () {
    /* Do not restart simulator automatically. */
    interrupted_at_gui_request = 0;

    /* Clear any breakpoint from an executing "next" command. */
    sys_bpt_addr = -1;

    /* Clear any "finish" command state. */
    finish_depth = 0;

    /* Tell the GUI about any changes to memory or registers. */
    dump_delayed_mem_updates ();
    print_registers ();
}


static void cmd_lc3_stop (const UNSIGNED char* args) {
    /* GUI only, so no need to warn about args. */
    /* Stop execution and dump state. */
    gui_stop_and_dump ();
}


static void flush_console_input () {
    struct pollfd p;

    /* Check option and script level.  Flushing would consume 
       remainder of a script. */
    if (!flush_on_start || script_depth > 0)
        return;

    /* Read a character at a time... */
    p.fd = fileno (lc3in);
    p.events = POLLIN;
    while (poll (&p, 1, 0) == 1 && (p.revents & POLLIN) != 0)
	fgetc (lc3in);
}

