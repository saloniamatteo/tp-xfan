/*
 * ThinkPad Fan Control
 * using XForms.
*/

#define _GNU_SOURCE
#define _FORTIFY_SOURCE 2
#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_BUGREPORT "saloniamatteo@pm.me"
#define PACKAGE_STRING "tp-xfan 1.0"
#endif

#ifndef PACKAGE_DONATE
#define PACKAGE_DONATE "https://saloniamatteo.top/donate.html"
#endif

#include <errno.h>
#include <forms.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DEFAULT_LOCAL_PATH "/apply.sh"
#define DEFAULT_SYS_PATH "/usr/local/share/tp-xfan/apply.sh"
#define DEBUG(where, ...) if (debug) fprintf(stderr, "Debug [" where "]: " __VA_ARGS__);

static FL_FORM	 *current_form = { 0 };	/* Current active form */
static char	 user_path[PATH_MAX];	/* User-defined directory that contains the script */
static int	 debug = 0;		/* Enable Debugging */
static int	 fan_val = 0;		/* Fan speed value */
static int	 using_custom_path = 0;	/* Are we using a custom directory? */
static int	 verbose = 0;		/* Verbosity */

static char	*INFO_STR = "tp-xfan is a program written in C99 to modify\n\
the speed of the ThinkPad fan, hence the name tp (ThinkPad),\n\
xfan (XForms fan).\n\n\
The only dependencies it has are XForms itself,\n\
the standard C library (which everyone has),\n\
Linux's 'su' command, and TCL's 'expect' command.\n\
\n\
These last two are used to escalate user privileges,\n\
when the user enters the root password. This is necessary,\n\
because we need to send data to /proc/acpi/ibm/fan,\n\
and this requires root privileges.\n\
\n\
Alternatively, you can run this program using\n\
a setuid program, like sudo/doas, or you can run it directly\n\
as root, and it will not ask for a password, since EUID is 0.";

/* Function prototypes */
static void showinfo(FL_OBJECT *, long);
static void checkexit(FL_OBJECT *, long);
static void askexit(FL_OBJECT *, long );
static void set_speed(FL_OBJECT *, long);
static char *ask_pass(void);
static void apply_speed(FL_OBJECT *, long);

/* Show program information */
static void
showinfo(FL_OBJECT *obj, long data)
{
	DEBUG("Show Info", "Showing info via XForms.\n");

	fl_show_messages_f("%s written by Salonia Matteo.\n"
			   "Report any bugs to %s\nSupport this project: %s\n\n"
			   "%s",
			   PACKAGE_STRING, PACKAGE_BUGREPORT, PACKAGE_DONATE, INFO_STR);

	DEBUG("Show Info", "Done showing info via XForms.\n");
}

/* Check if user really wants to exit */
static void
checkexit(FL_OBJECT *obj, long data)
{
	// data = 1 -> yes
	if (data) {
		DEBUG("Check Exit", "User wants to exit, destroying Forms.\n");
		fl_finish();
		DEBUG("Check Exit", "Exiting.\n");
		exit(0);
	// data = 0 -> no
	} else {
		DEBUG("Check Exit", "User does NOT want to exit, destroying Exit Form.\n");
		fl_free_form(current_form);
	}
}

/* Exit Confirmation Dialog */
static void
askexit(FL_OBJECT *obj, long data)
{
	DEBUG("Ask Exit", "Building Exit Form via XForms.\n");

	/* Create new XForms objects */
	FL_FORM *exit_form;
	FL_OBJECT *yes, *no;

	DEBUG("Ask Exit", "Adding Objects to Exit Form.\n");

	exit_form = fl_bgn_form(FL_UP_BOX, 320, 120);
	current_form = exit_form;

	/* Add text to form center */
	fl_add_box(FL_UP_BOX, 160, 40, 0, 0, "Do you really want to Quit?");
	
	/* Add buttons and button handlers */
	yes = fl_add_button(FL_NORMAL_BUTTON, 40, 70, 80, 30, "Yes");
	fl_set_object_callback(yes, checkexit, 1);

	no = fl_add_button(FL_NORMAL_BUTTON, 200, 70, 80, 30, "No");
	fl_set_object_callback(no, checkexit, 0);

	current_form = exit_form;

	fl_end_form();

	DEBUG("Ask Exit", "Done Adding Objects to Exit Form.\n");

	/* Show form */
	fl_show_form(exit_form, FL_PLACE_CENTER, FL_TRANSIENT,
			"Exit Confirmation Dialog");

	fl_do_forms();

	DEBUG("Ask Exit", "Showing Exit Form.\n");

	current_form = exit_form;

	fl_free_form(exit_form);

	DEBUG("Ask Exit", "Exit Form destroyed.\n");
}

/* Set fan speed */
static void
set_speed(FL_OBJECT *obj, long data)
{
	DEBUG("Set Speed", "Setting fan speed variable to %d.\n", fan_val);

	fan_val = fl_get_slider_value(obj);

	DEBUG("Set Speed", "Done Setting fan speed variable.\n");
}

/* Ask password */
static char *
ask_pass(void)
{
askpass:
	DEBUG("Ask Pass", "Asking root password to user.\n");

	char *pass = (char *)fl_show_input("Enter the root password:", "pass");

	if (pass == NULL || !strcmp(pass, " ") || !strcmp(pass, "") || !strcmp(pass, "(null)")) {
		fl_show_alert("Error!", "Enter a valid password!", "", 1);
		goto askpass;
	}
	
	DEBUG("Ask Pass", "Returning password.\n");

	return pass;
}

/* Appy speed */
static void
apply_speed(FL_OBJECT *obj, long data)
{
	char path[PATH_MAX] = { 0 };
	FILE *fd;

	/* If we aren't using a user-defined path,
	 * set path to $PWD/DEFAULT_LOCAL_PATH */
	if (!using_custom_path) {
		DEBUG("Apply Speed", "Not using custom path.\n");

		/* Get current directory */
		char cwd[PATH_MAX] = { 0 };
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "Unable to get current working directory! "
					"Exiting with ENOTRECOVERABLE.\n");
			exit(ENOTRECOVERABLE);
		}

		DEBUG("Apply Speed", "Gotten current working directory: %s.\n", cwd);

		/* Copy default local path to path */
		snprintf(path, PATH_MAX, "%s%s", cwd, DEFAULT_LOCAL_PATH);

		DEBUG("Apply Speed", "Set path to %s.\n", path);

	/* Otherwise, set path to $USER_DIR/DEFAULT_LOCAL_PATH */
	} else {
		DEBUG("Apply Speed", "Using custom directory: %s.\n", user_path);

		if (user_path != NULL)
			snprintf(path, PATH_MAX, "%s%s", user_path, DEFAULT_LOCAL_PATH);
		else {
			fprintf(stderr, "Invalid directory! Exiting with ENOENT.\n");
			exit(ENOENT);
		}

		DEBUG("Apply Speed", "Set path to %s.\n", path);
	}

	/* Check if apply.sh exists */
	fd = fopen(path, "r");

	/* Does not exist */
	if (fd == NULL) {
		DEBUG("Apply Speed", "Warning: could not open %s, trying %s.\n",
			path, DEFAULT_SYS_PATH);

		strncpy(path, DEFAULT_SYS_PATH, strlen(DEFAULT_SYS_PATH) + 1);
		fd = fopen(path, "r");

		/* No other files to try, exit. */
		if (fd == NULL) {
			fprintf(stderr, "Error: unable to find %s nor %s!\n",
				DEFAULT_LOCAL_PATH, DEFAULT_SYS_PATH);

			exit(ENOENT);
		}

	}

	/* File was found */
	fclose(fd);

	/* Append speed value to path */
	char val[3] = { 0 };
	snprintf(val, 3, " %d", fan_val);
	strncat(path, val, strlen(path) + 2);

	DEBUG("Apply Speed", "Added speed value [%d] to path: %s.\n", val, path);

	/* EUID preliminary checks */
	uid_t euid = geteuid();
	int is_root = 0;
	char pass[100] = { 0 };

	DEBUG("Apply Speed", "Gotten EUID: %d\n", euid);

	/* Check if current user is root or not */
	if (euid == 0) {
		is_root = 1;

		DEBUG("Apply Speed", "User is root, not asking password.\n");

	/* User is not root, ask root password */
	} else {
		DEBUG("Apply Speed", "User is NOT root, asking password.\n");

		strncpy(pass, ask_pass(), 100);
	}

	int size = (is_root != 0 ? strlen(path) + 2 : strlen(pass) + strlen(path) + 100);
	char cmd[size];

	DEBUG("Apply Speed", "Set size to %d; is_root: %d\n", size, is_root);

	/* Assemble the command to run */
	if (!is_root) {
		snprintf(cmd, sizeof(cmd),
			"expect -c 'spawn su - root -c \"%s\"; "
			"expect \"Password: \"; send \"%s\n\"; interact;'",
			path, pass);
	} else
		snprintf(cmd, sizeof(cmd), "%s", path);

	DEBUG("Apply Speed", "Set command to run. (For security reasons, it is not printed)\n");

	/* Without verbosity, do not show command output */
	if (verbose == 0)
		strncat(cmd, " > /dev/null", 12);

	DEBUG("Apply Speed", "Running command.\n");

	/* Try to run script */
	int ret = system(cmd);

	DEBUG("Apply Speed", "Command finished.\n");

	if (ret != 0)
		fprintf(stderr, "Unknown error!\n");
}

int
main(int argc, char **argv)
{
	/* Before starting XForms, try to parse options */
	int ind = 0;

	while ((ind = getopt(argc, argv, ":dihp:v")) != 1) {
		switch (ind) {
		/* Enable debugging */
		case 'd':
			debug = 1;
			break;

		/* Print info */
		case 'i':
			printf("%s\n", INFO_STR);
			return 0;
			break;

		/* Show help */
		case '?':
		case 'h':
			printf("%s by Salonia Matteo. Report any bugs to %s.\n"
				"Support this project: %s\n"
				"\n"
				"Available flags:\n"
				"-d		Enable debugging.\n"
				"-i		Print information and exit, without opening XForms.\n"
				"-h		Print this help and exit, without opening XForms.\n"
				"-p path\t	Which directory contains the apply.sh script.\n"
				"-v		Be more verbose. (writes info to the console)\n",
				PACKAGE_STRING, PACKAGE_BUGREPORT, PACKAGE_DONATE);
			return 0;
			break;

		/* Use custom path */
		case 'p':
			strncpy(user_path, optarg, PATH_MAX);
			using_custom_path = 1;
			break;

		/* Be more verbose */
		case 'v':
			verbose = 1;
			break;
		}

		if (ind <= 0)
			break;
	}

	DEBUG("Main", "Finished parsing options, starting XForms.\n");

	FL_FORM *form;	/* Create new form */
	FL_OBJECT *obj;	/* Create new object */

	/* Initialise XForms using argc & argv */
	fl_initialize(&argc, argv, 0, 0, 0);

	/* Create a new form */
	form = fl_bgn_form(FL_UP_BOX, 250, 270);
	current_form = form;

	DEBUG("Main", "Building Main Form via XForms.\n");

	/* Add a slider to select values between 0 (auto) and 8 (max) */
	obj = fl_add_valslider(FL_HOR_SLIDER, 10, 25, 230, 40, "Fan speed (0 = auto, 8 = max)");
	fl_set_slider_bounds(obj, 0, 8);
	fl_set_slider_precision(obj, 0);
	fl_set_slider_value(obj, 0);
	fl_set_object_lalign(obj, FL_ALIGN_TOP);
	fl_set_object_callback(obj, set_speed, 0);
	fl_set_slider_return(obj, FL_RETURN_CHANGED);
	
	/* Align label to left */
	fl_set_object_lalign(obj, FL_ALIGN_LEFT_TOP);

	/* Add an "Apply" button, that applies current speed */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 80, 230, 50, "Apply");
	fl_set_object_callback(obj, apply_speed, 0);

	/* Add an "Info" button, that shows app info */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 140, 230, 50, "Info");
	fl_set_object_callback(obj, showinfo, 0);

	/* Add a "Quit" button, that ends XForms */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 200, 230, 50, "Quit");
	fl_set_object_callback(obj, askexit, 0);

	DEBUG("Main", "Finished building Main Form.\n");

	fl_end_form();

	DEBUG("Main", "Showing Main Form.\n");

	/* Show the form, with title "Greet" */
	fl_show_form(form, FL_PLACE_CENTER, FL_FULLBORDER, "TP-XFan by Salonia Matteo");
	fl_do_forms();

	current_form = form;

	DEBUG("Main", "Exiting Main Form.\n");

	return 0;
}
