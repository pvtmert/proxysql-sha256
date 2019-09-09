
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#include <sys/ioctl.h>
#include <mysql.h>

#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#define MAX(x, y) ( ((x) > (y)) ? (x) : (y) )

const char*
input(FILE *fp, size_t size, const char *fmt, ...) {
	#ifdef READLINE
	if(stdin == fp) {
		char *line = readline(fmt);
		add_history(line);
		return line;
	}
	#endif
	if(fmt && *fmt) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
	char *buffer = (char*) calloc(size, sizeof(char));
	unsigned chars = getline(&buffer, &size, fp);
	if(chars > 0 && *buffer) buffer[chars-1] = 0x00;
	return ((chars > 0) ? realloc(buffer, chars) : (free(buffer), NULL));
}

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
	maxwidth = ((!IS_NUM(type) && maxwidth < 6) ? 6 : maxwidth);
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
		if(rows > 0) printf("> %ld row(s) affected\n", rows);
		return;
	}
	struct winsize termsz;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &termsz);
	unsigned long width = (termsz.ws_col ? termsz.ws_col : 80);
	for(long i=-1, ncols=mysql_num_fields(result); i<(long)mysql_num_rows(result); i++) {
		MYSQL_FIELD *fields = mysql_fetch_fields(result);
		unsigned long totalwidth = 0;
		for(int j=0; j<ncols; j++) 
			totalwidth += MAX(fields[j].name_length, fields[j].max_length);
		char multiline = (totalwidth > (width - 2*ncols - 8));
		printf("", columnhelper(fields,
			(i < 0) ? NULL : mysql_fetch_row(result), 
			ncols, multiline, i < 0)
		);
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
	const char **commands,
	...
) {
	FILE *in = NULL;
	MYSQL *con = mysql_init(NULL);
	const char *schema = (db && strlen(db)) ? db : NULL;
	mysql_real_connect(con, host, user, pass, schema, port, NULL, 0);
	if(print(stdout, con, "isrv: %s", mysql_get_server_info(con))) goto quit;
	if(print(stdout, con, "prot: %d", mysql_get_proto_info(con)))  goto quit;
	if(print(stdout, con, "host: %s", mysql_get_host_info(con)))   goto quit;
	if(print(stdout, con, "_ssl: %s", mysql_get_ssl_cipher(con)))  goto quit;
	if(print(stdout, con, "icli: %s", mysql_get_client_info()))    goto quit;
	if(print(stdout, con, "stat: %s", mysql_stat(con)))            goto quit;
	if(print(stdout, con, "info: %s", mysql_info(con)))            goto quit;
	tableprint(mysql_list_dbs(con, NULL), 0);
	if(schema) tableprint(mysql_list_tables(con, NULL), 0);
	while(commands && *commands) {
		if('-' == **commands || '/' == **commands || '.' == **commands) {
			size_t length = 999;
			in = ('-' == **commands) ? stdin : fopen(*commands, "r");
			while(!feof(in)) {
				char *buffer = input(in, 999, (in == stdin) ? "sql> " : NULL);
				#ifdef READLINE
				if(!buffer) break;
				#endif
				if(!buffer || !*buffer) continue;
				mysql_query(con, buffer);
				tableprint(
					mysql_store_result(con),
					mysql_affected_rows(con)
				);
				error(con);
				free(buffer);
				continue;
			}
			if('-' == **commands) {
				print(stdout, con, "...done");
			} else {
				fclose(in);
			}
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
		if(in == stdin) return (mysql_close(con), 0);
		int err = error(con);
		mysql_close(con);
		return err;
	}
	goto quit;
}

int
main(const int argc, const char **argv) {
	if(argc < 5) {
		print(stdout, NULL,
			"usage:              %s <host> <port> <schema> <user> [<pass|-> [(-|sql|file) ...]]",
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (shell):         %s my.sql.com 3306 test root passwd -", 
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (exec):          %s my.sql.com 3306 test root passwd /path/to/file", 
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (inline):        %s my.sql.com 3306 test root passwd 'select 1;'", 
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (exec/relative): %s my.sql.com 3306 test root passwd ./dump.sql",
			argv[0], NULL);
		return 1;
	}
	const char *pass = (argc > 5 && strlen(argv[5])) ? (
		(1 == strlen(argv[5]) && '-' == *argv[5]) ? input(stdin, 80, "pass:") : argv[5]
	): NULL;
	const char **cmds = (argc > 6) ? &(argv[6]) : NULL;
	#ifdef READLINE
	rl_bind_key('\t', rl_complete);
	#endif
	return tester(argv[1], atoi(argv[2]), argv[3], argv[4], pass, cmds);
}
