/*
 * Copyright (c) Kerlabs 2008.
 * Copyright (c) International Business Machines  Corp., 2008
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;  without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Created by Renaud Lottiaux
 */

/*
 * Check if setreuid behaves correctly with file permissions.
 * The test creates a file as ROOT with permissions 0644, does a setreuid
 * and then tries to open the file with RDWR permissions.
 * The same test is done in a fork to check if new UIDs are correctly
 * passed to the son.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>

#include "test.h"
#include "compat_16.h"

TCID_DEFINE(setreuid07);
int TST_TOTAL = 1;

static char testfile[256] = "";
static struct passwd *ltpuser;

static int fd = -1;

static void setup(void);
static void cleanup(void);
static void do_master_child(void);

int main(int ac, char **av)
{
	pid_t pid;

	tst_parse_opts(ac, av, NULL, NULL);

	setup();

	pid = FORK_OR_VFORK();
	if (pid < 0)
		tst_brkm(TBROK, cleanup, "Fork failed");

	if (pid == 0)
		do_master_child();

	tst_record_childstatus(cleanup, pid);

	cleanup();
	tst_exit();
}

static void do_master_child(void)
{
	int lc;
	int pid;
	int status;

	for (lc = 0; TEST_LOOPING(lc); lc++) {
		int tst_fd;

		tst_count = 0;

		if (SETREUID(NULL, 0, ltpuser->pw_uid) == -1) {
			perror("setreuid failed");
			exit(TFAIL);
		}

		/* Test 1: Check the process with new uid cannot open the file
		 *         with RDWR permissions.
		 */
		TEST(tst_fd = open(testfile, O_RDWR));

		if (TEST_RETURN != -1) {
			printf("open succeeded unexpectedly\n");
			close(tst_fd);
			exit(TFAIL);
		}

		if (TEST_ERRNO == EACCES) {
			printf("open failed with EACCES as expected\n");
		} else {
			perror("open failed unexpectedly");
			exit(TFAIL);
		}

		/* Test 2: Check a son process cannot open the file
		 *         with RDWR permissions.
		 */
		pid = FORK_OR_VFORK();
		if (pid < 0)
			tst_brkm(TBROK, cleanup, "Fork failed");

		if (pid == 0) {
			int tst_fd2;

			/* Test to open the file in son process */
			TEST(tst_fd2 = open(testfile, O_RDWR));

			if (TEST_RETURN != -1) {
				printf("call succeeded unexpectedly\n");
				close(tst_fd2);
				exit(TFAIL);
			}

			if (TEST_ERRNO == EACCES) {
				printf("open failed with EACCES as expected\n");
				exit(TPASS);
			} else {
				printf("open failed unexpectedly\n");
				exit(TFAIL);
			}
		} else {
			/* Wait for son completion */
			if (waitpid(pid, &status, 0) == -1) {
				perror("waitpid failed");
				exit(TFAIL);
			}
			if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0))
				exit(WEXITSTATUS(status));
		}

		/* Test 3: Fallback to initial uid and check we can again open
		 *         the file with RDWR permissions.
		 */
		tst_count++;
		if (SETREUID(NULL, 0, 0) == -1) {
			perror("setreuid failed");
			exit(TFAIL);
		}

		TEST(tst_fd = open(testfile, O_RDWR));

		if (TEST_RETURN == -1) {
			perror("open failed unexpectedly");
			exit(TFAIL);
		} else {
			printf("open call succeeded\n");
			close(tst_fd);
		}
	}
	exit(TPASS);
}

static void setup(void)
{
	tst_require_root();

	ltpuser = getpwnam("nobody");
	if (ltpuser == NULL)
		tst_brkm(TBROK, NULL, "nobody must be a valid user.");

	tst_tmpdir();

	sprintf(testfile, "setreuid07file%d.tst", getpid());

	/* Create test file */
	fd = open(testfile, O_CREAT | O_RDWR, 0644);
	if (fd < 0)
		tst_brkm(TBROK | TERRNO,
			cleanup, "cannot create test file");

	tst_sig(FORK, DEF_HANDLER, cleanup);

	TEST_PAUSE;
}

static void cleanup(void)
{
	close(fd);

	tst_rmdir();
}
