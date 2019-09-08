
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <sys/ioctl.h>
#include <mysql.h>

#define MAX(x, y) ( ((x) > (y)) ? (x) : (y) )

int
print(FILE *fd, MYSQL *m, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *buffer = (char*) calloc(2+strlen(fmt), sizeof(char));
	sprintf(buffer, "%s\n", fmt);
	vprintf(buffer, args);
	va_end(args);
	free(buffer);
	return m ? mysql_errno(m) : 0;
}

int
error(MYSQL *m) {
	int err = mysql_errno(m);
	if(err) {
		print(stderr, m, "[%6d]: %s", err, mysql_error(m));
	}
	return err;
}

unsigned
columnprint(
	int index,
	char lining,
	const char *text,
	unsigned long maxwidth,
	enum enum_field_types type,
	...
) {
	return printf(
		"%s%*s%s",
		(!index && lining) ? "\n " : (lining ? " " : "["),
		(int) ((IS_NUM(type) && !lining) ? maxwidth : -maxwidth), text,
		lining ? "\n" : "]",
		NULL
	);
}

unsigned
columnhelper(
	MYSQL_FIELD *fields,
	MYSQL_ROW row,
	unsigned long ncols,
	char multiline,
	char isheader,
	...
) {
	unsigned chars = 0;
	chars += printf(isheader ? "head: ^" : "_row: ^");
	for(int j=0; j<ncols; j++) {
		//tableprinthelper(width, colwidth, ln[j], j, n, row[j]);
		chars += columnprint(
			j, multiline,
			(isheader) ? fields[j].name : row[j],
			MAX(fields[j].name_length, fields[j].max_length),
			fields[j].type,
			NULL
		);
		continue;
	}
	return chars + printf("$\n");
}

void
tableprint(MYSQL_RES *result, long rows) {
	if(!result) {
		printf("> %ld rows affected\n", rows);
		return;
	}
	struct winsize termsz;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &termsz);
	unsigned long width = (termsz.ws_col ? termsz.ws_col : 80);
	for(long i=0, colcnt=mysql_num_fields(result); i<mysql_num_rows(result); i++) {
		MYSQL_ROW row = mysql_fetch_row(result);
		MYSQL_FIELD *fields = mysql_fetch_fields(result);
		unsigned long totalwidth = 0;
		for(int j=0; j<colcnt; j++) 
			totalwidth += MAX(fields[j].name_length, fields[j].max_length);
		char multiline = (totalwidth > (width - 2*colcnt - 8));
		if(!i) columnhelper(fields, row, colcnt, multiline, 1);
		printf("", columnhelper(fields, row, colcnt, multiline, 0));
		continue;
	}
	if(result) mysql_free_result(result);
	return;
}

int
tester(
	const char *host,
	unsigned short port,
	const char *db,
	const char *user,
	const char *pass,
	const char **commands
) {
	MYSQL *con = mysql_init(NULL);
	mysql_real_connect(con, host, user, pass, db, port, NULL, 0);
	if(print(stdout, con, "isrv: %s", mysql_get_server_info(con))) goto quit;
	if(print(stdout, con, "prot: %d", mysql_get_proto_info(con)))  goto quit;
	if(print(stdout, con, "host: %s", mysql_get_host_info(con)))   goto quit;
	if(print(stdout, con, " ssl: %s", mysql_get_ssl_cipher(con)))  goto quit;
	if(print(stdout, con, "icli: %s", mysql_get_client_info()))    goto quit;
	if(print(stdout, con, "stat: %s", mysql_stat(con)))            goto quit;
	if(print(stdout, con, "info: %s", mysql_info(con)))            goto quit;
	tableprint(mysql_list_dbs(con, NULL), 0);
	tableprint(mysql_list_tables(con, NULL), 0);
	while(commands && *commands) {
		if('-' == **commands) {
			size_t length = 999;
			FILE *in = stdin;
			while(!feof(in)) {
				char *buffer = (char*) calloc(length, sizeof(char));
				printf("sql> ");
				unsigned chars = getline(&buffer, &length, in);
				if(!buffer || !*buffer || !strlen(buffer))
					break;
				mysql_real_query(con, buffer, chars);
				tableprint(
					mysql_store_result(con),
					mysql_affected_rows(con)
				);
				error(con);
				free(buffer);
				continue;
			}
			print(stdout, con, "...done");
			goto next;
		}
		print(stdout, con, "exec: %s", *commands);
		if(print(stdout, con, "query: %d", mysql_query(con, *commands)))
			goto quit;
		tableprint(mysql_store_result(con), mysql_affected_rows(con));
		next: {
			commands += 1;
			continue;
		}
		goto next;
	}
	quit: {
		int err = error(con);
		mysql_close(con);
		return err;
	}
	goto quit;
}

int
main(const int argc, const char **argv) {
	if(argc < 5) {
		print(
			stdout,
			NULL,
			"usage: %s <host> <port> <schema> <user> [pass [sql...]]",
			argv[0]
		);
		return 1;
	}
	const char  *pass = (argc > 5 && strlen(argv[5])) ? argv[5] : NULL;
	const char **cmds = (argc > 6) ? &(argv[6]) : NULL;
	return tester(argv[1], atoi(argv[2]), argv[3], argv[4], pass, cmds);
}
