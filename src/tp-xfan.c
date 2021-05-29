/* See LICENSE file for copyright and license details.
 *
 * tp-xfan (ThinkPad XForms Fan) is a program which
 * uses the XForms C library to display in a beautiful
 * way some controls, in order to modify the speed of
 * the ThinkPad's fan. It supports automatic speed
 * (level 0), all the way up to maximum speed (level 8).
 *
 * It also provides AFSM, or Auto Fan Speed Management,
 * which lets users modify in a powerful way
 * how should the fan run, etc.
 *
 * Made by Salonia Matteo <saloniamatteo@pm.me>
 *
 */

#define _GNU_SOURCE
#define _FORTIFY_SOURCE 2
#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define PACKAGE_BUGREPORT "saloniamatteo@pm.me"
#define PACKAGE_STRING "tp-xfan 1.1"
#endif

/* For some reason, the following isn't defined in config.h. */
#ifndef PACKAGE_DONATE
#define PACKAGE_DONATE "https://saloniamatteo.top/donate.html"
#endif

#include <errno.h>
#include <forms.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "temp.h"

#define DEFAULT_LOCAL_PATH "/apply.sh"
#define DEFAULT_SYS_PATH "/usr/local/share/tp-xfan/apply.sh"
#define DEBUG(where, ...) if (debug) fprintf(stderr, "Debug [" where "]: " __VA_ARGS__);

#define DEF_TEMP 50	/* Default valslider temperature */
#define MAX_TEMP 100	/* Maximum valslider temperature */
#define MIN_TEMP 40	/* Minimum valslider temperature */
#define PASS_MAX 100	/* Password maximum length */

static FL_FORM	 *current_form = {0};	/* Current active form */
static char	 pass[PASS_MAX] = {-1};	/* Cache password, without prompting user */
static char	 path[PATH_MAX] = {0};	/* Path containing apply.sh script */
static char	 user_path[PATH_MAX];	/* User-defined directory that contains the script */
static int	 debug = 0;		/* Enable Debugging */
static int	 fan_rec = 0;		/* Copy of fan value (used to recover AFSM speed) */
static int	 fan_val = 0;		/* Fan speed value */
static int	 interval = 0;		/* AFSM interval (in seconds) */
static int	 temp = DEF_TEMP;	/* CPU temperature threshold */
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
static void set_valslider(FL_OBJECT *, int, int, int, int, int, void *, long);
static void set_path(void);
static int  check_root(void);
static void ask_pass(void);
static void showinfo(FL_OBJECT *, long);
static void checkexit(FL_OBJECT *, long);
static void askexit(FL_OBJECT *, long );
static void set_speed(FL_OBJECT *, long);
static void apply_speed(FL_OBJECT *, long);
static void set_afsm_val(FL_OBJECT *, long);
static void set_afsm_secs(FL_OBJECT *, long);
static void afsm(FL_OBJECT *, long);

/* Setup valslider */
static void
set_valslider(FL_OBJECT *obj, int min, int max, int precision, int val, int align, void *func, long data)
{
	fl_set_slider_bounds(obj, min, max);
	fl_set_slider_precision(obj, precision);
	fl_set_slider_value(obj, val);
	fl_set_object_lalign(obj, align);
	fl_set_object_callback(obj, func, data);
	fl_set_slider_return(obj, FL_RETURN_CHANGED);
}

/* Set path */
static void
set_path(void)
{
	FILE *fd;

	/* If we aren't using a user-defined path,
	 * set path to $PWD/DEFAULT_LOCAL_PATH */
	if (!using_custom_path) {
		DEBUG("Set Path", "Not using custom path.\n");

		/* Get current directory */
		char cwd[PATH_MAX] = { 0 };
		if (getcwd(cwd, sizeof(cwd)) == NULL) {
			fprintf(stderr, "Unable to get current working directory! "
					"Exiting with ENOTRECOVERABLE.\n");
			exit(ENOTRECOVERABLE);
		}

		DEBUG("Set Path", "Gotten current working directory: %s.\n", cwd);

		/* Copy default local path to path */
		snprintf(path, PATH_MAX, "%s%s", cwd, DEFAULT_LOCAL_PATH);

		DEBUG("Set Path", "Set path to %s.\n", path);

	/* Otherwise, set path to $USER_DIR/DEFAULT_LOCAL_PATH */
	} else {
		DEBUG("Set Path", "Using custom directory: %s.\n", user_path);

		if (user_path != NULL)
			snprintf(path, PATH_MAX, "%s%s", user_path, DEFAULT_LOCAL_PATH);
		else {
			fprintf(stderr, "Invalid directory! Exiting with ENOENT.\n");
			exit(ENOENT);
		}

		DEBUG("Set Path", "Set path to %s.\n", path);
	}

	/* Check if apply.sh exists */
	fd = fopen(path, "r");

	/* Does not exist */
	if (fd == NULL) {
		DEBUG("Set Path", "Warning: could not open %s, trying %s.\n",
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
}

/* Check if current user is root */
static int
check_root(void)
{
	uid_t euid = geteuid();

	DEBUG("Check Root", "Gotten EUID: %d\n", euid);

	return (euid == 0 ? 1 : 0);
}

/* Ask password */
static void
ask_pass(void)
{
	/* Check if we have to set the password,
	 * if password wasn't cached */
	if (pass[0] == -1) {
askpass:
		DEBUG("Ask Pass", "Asking root password to user.\n");

		char *pass_l = (char *)fl_show_input("Enter the root password:", "pass");

		if (pass_l == NULL || !strcmp(pass_l, " ") || !strcmp(pass_l, "") || !strcmp(pass_l, "(null)")) {
			fl_show_alert("Error!", "Enter a valid password!", "", 1);
			goto askpass;
		}

		/* Copy local password to password cache */
		DEBUG("Ask Pass", "Copying local password to password cache.\n");
		strncpy(pass, pass_l, PASS_MAX);
		
		DEBUG("Ask Pass", "Gotten password.\n");
	}
}

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

	/* If we're getting called from AFSM,
	 * copy fan_val to fan_rec */
	fan_rec = fan_val;

	DEBUG("Set Speed", "Done Setting fan speed variable.\n");
}

/* Appy speed */
static void
apply_speed(FL_OBJECT *obj, long data)
{
	/* Set path */
	set_path();

	DEBUG("Apply Speed", "Gotten fan value: %d\n", fan_val);

	/* Append speed value to path */
	char val[3] = { 0 };
	snprintf(val, 3, " %d", fan_val);
	strncat(path, val, strlen(path) + 2);

	DEBUG("Apply Speed", "Added speed value [%s] to path: %s.\n", val, path);

	/* Check if current user is root or not */
	int is_root = 0;

	if (check_root()) {
		is_root = 1;

		DEBUG("Apply Speed", "User is root, not asking password.\n");

	/* User is not root, ask root password */
	} else {
		DEBUG("Apply Speed", "User is NOT root, asking password.\n");
		ask_pass();
	}

	int size = (is_root != 0 ? strlen(path) + 2 : strlen(pass) + strlen(path) + 80);
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

/* Set min/max values for AFSM */
static void
set_afsm_val(FL_OBJECT *obj, long data)
{
	DEBUG("Set AFSM val", "Setting CPU temperature threshold.\n");
	temp = fl_get_slider_value(obj);
	DEBUG("Set AFSM val", "Set CPU temperature to %d.\n", temp);
}

/* Set AFSM interval */
static void
set_afsm_secs(FL_OBJECT *obj, long data)
{
	DEBUG("Set AFSM seconds", "Setting AMFS Interval.\n", interval);

	interval = fl_get_slider_value(obj);

	DEBUG("Set AFSM seconds", "Set AMFS Interval to %d.\n", interval);
}

/* Run AFSM */
static void
afsm_run(FL_OBJECT *obj, long data)
{
run_afsm:
	DEBUG("AFSM Run", "Starting AFSM.\n");

	/* Check CPU temperature after interval secs */
	fl_msleep(interval * 1000);

	/* Check CPU temperature */
	if (get_temp() <= temp) {
		DEBUG("AFSM Run", "Current temp (%d) is less than threshold (%d).\n", get_temp(), temp);

		/* Set fan speed to auto */
		fan_val = 0;
		apply_speed(obj, data);
		goto run_afsm;	/* Loop */
	} else {
		DEBUG("AFSM Run", "Current temp is greater than threshold.\n");

		/* Apply desired speed */
		fan_val = fan_rec;
		apply_speed(obj, data);
		goto run_afsm;	/* Loop */
	}
}

/* Stop AFSM */
static void
afsm_stop(FL_OBJECT *obj, long data)
{
	// data = 1 -> close AFSM form
	if (data) {
		fl_free_form(current_form);
		DEBUG("AFSM Stop", "Destroyed AFSM Form.\n");
	// data = 0 -> stop AFSM
	} else {
		DEBUG("AFSM Stop", "Stopping afsm.\n");

		fan_val = 0;
		apply_speed(obj, data);

		DEBUG("AFSM Stop", "Set fan speed to auto.\n");
	}
}

/* Auto Fan Speed Management */
static void
afsm(FL_OBJECT *obj, long data)
{
	DEBUG("AFSM", "Building AFSM Form via XForms.\n");

	/* Create new XForms objects */
	FL_FORM *afsm_form;
	FL_OBJECT *tmp, *fan, *sec, *btn;

	DEBUG("AFSM", "Adding Objects to AFSM Form.\n");

	afsm_form = fl_bgn_form(FL_UP_BOX, 320, 330);
	current_form = afsm_form;

	/* Add text to form center */
	fl_add_box(FL_UP_BOX, 160, 40, 0, 0, "Auto Fan Speed Management");
	
	/* Add sliders and handlers */
	/* CPU temperature threshold */
	tmp = fl_add_valslider(FL_HOR_SLIDER, 20, 70, 280, 30, "CPU temp threshold");
	set_valslider(tmp, MIN_TEMP, MAX_TEMP, 0, DEF_TEMP, FL_ALIGN_TOP, set_afsm_val, 0);

	/* Fan speed */
	fan = fl_add_valslider(FL_HOR_SLIDER, 20, 130, 130, 30, "Fan speed");
	set_valslider(fan, 0, 8, 0, 7, FL_ALIGN_TOP, set_speed, 1);

	/* Interval in seconds */
	sec = fl_add_valslider(FL_HOR_SLIDER, 170, 130, 130, 30, "Interval (in seconds)");
	set_valslider(sec, 0, 10, 0, 5, FL_ALIGN_TOP, set_afsm_secs, 0);

	/* Start monitoring CPU temp */
	btn = fl_add_button(FL_NORMAL_BUTTON, 20, 170, 280, 40, "Apply & Run");
	fl_set_object_callback(btn, afsm_run, 0);

	/* Button to stop */
	btn = fl_add_button(FL_NORMAL_BUTTON, 20, 220, 280, 40, "Set fan speed to auto");
	fl_set_object_callback(btn, afsm_stop, 0);

	/* Button to close AFSM Form */
	btn = fl_add_button(FL_NORMAL_BUTTON, 20, 270, 280, 40, "Close");
	fl_set_object_callback(btn, afsm_stop, 1);

	current_form = afsm_form;

	fl_end_form();

	DEBUG("AFSM", "Done Adding Objects to Exit Form.\n");

	/* Show form */
	fl_show_form(afsm_form, FL_PLACE_CENTER, FL_TRANSIENT,
			"Auto Fan Speed Management");

	fl_do_forms();

	DEBUG("AFSM", "Showing AFSM Form.\n");

	current_form = afsm_form;

	fl_free_form(afsm_form);

	DEBUG("AFSM", "AFSM Form destroyed.\n");
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
	form = fl_bgn_form(FL_UP_BOX, 250, 300);
	current_form = form;

	DEBUG("Main", "Building Main Form via XForms.\n");

	/* Add a slider to select values between 0 (auto) and 8 (max) */
	obj = fl_add_valslider(FL_HOR_SLIDER, 10, 25, 230, 40, "Fan speed (0 = auto, 8 = max)");
	set_valslider(obj, 0, 8, 0, 0, FL_ALIGN_TOP, set_speed, 0);
	
	/* Align label to left */
	fl_set_object_lalign(obj, FL_ALIGN_LEFT_TOP);

	/* Add an "Apply" button, that applies current speed */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 80, 230, 50, "Apply");
	fl_set_object_callback(obj, apply_speed, 0);

	/* Add a button to use Auto Fan Speed Management */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 140, 230, 50, "Auto Fan Speed Management");
	fl_set_object_callback(obj, afsm, 0);

	/* Add an "Info" button, that shows app info */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 200, 230, 40, "Info");
	fl_set_object_callback(obj, showinfo, 0);

	/* Add a "Quit" button, that ends XForms */
	obj = fl_add_button(FL_NORMAL_BUTTON, 10, 250, 230, 40, "Quit");
	fl_set_object_callback(obj, askexit, 0);

	DEBUG("Main", "Finished building Main Form.\n");

	fl_end_form();

	DEBUG("Main", "Showing Main Form.\n");

	/* Show the form, with title "Greet" */
	fl_show_form(form, FL_PLACE_CENTER, FL_FULLBORDER, "TP-XFan by Salonia Matteo");
	fl_do_forms();

	current_form = form;

	DEBUG("Main", "Exiting Main Form, Destroying all Forms.\n");

	fl_deactivate_all_forms();

	DEBUG("Main", "Destroyed all forms.\n");

	return 0;
}
